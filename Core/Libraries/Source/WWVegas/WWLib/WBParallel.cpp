/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// WBParallel.cpp -- persistent fork-join thread pool. See WBParallel.h.

#include "WBParallel.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

namespace WBParallel
{

// --- worker count -----------------------------------------------------------

int workerCount()
{
	// Cached on first use. magic-statics make this initialization thread-safe
	// under C++11+, but workerCount() is in practice first touched from the UI
	// thread well before any parallelFor anyway.
	static const int s_count = []() -> int {
		const char *env = std::getenv("WB_PARALLEL");
		if (env && env[0] == '0' && env[1] == '\0')
			return 1;								// forced-serial A/B mode
		unsigned hc = std::thread::hardware_concurrency();
		if (hc == 0) hc = 1;						// unknown -> assume single core
		int n = (int)hc - 1;						// leave a core for the UI thread
		if (n < 1) n = 1;
		if (n > 8) n = 8;
		return n;
	}();
	return s_count;
}

// --- the pool ----------------------------------------------------------------
//
// A fixed set of (workerCount()-1) worker threads. The calling thread always
// runs one band itself, so the workers cover the remaining bands -- meaning a
// 4-wide split uses 3 workers + the caller. Workers park on a condition
// variable between calls (no per-call thread creation).
//
// Each parallelFor bumps a generation counter and publishes the band function;
// workers wake, run their assigned band, and decrement a "remaining" counter.
// The caller runs band (count-1) inline, then waits for remaining==0.

namespace {

struct Pool
{
	std::vector<std::thread>            threads;
	std::mutex                          mtx;
	std::condition_variable             wake;		// workers wait here for work
	std::condition_variable             done;		// caller waits here for completion

	const std::function<void(int,int)> *body = nullptr;	// current call's body (not owned)
	int                                 begin = 0;
	int                                 end = 0;
	int                                 bands = 0;		// total bands this call
	unsigned long long                  generation = 0;	// bumped per parallelFor
	std::atomic<int>                    nextBand{0};	// bands claimed so far
	std::atomic<int>                    remaining{0};	// worker bands still running
	std::exception_ptr                  error;			// first exception from any band
	bool                                stopping = false;

	~Pool() { stop(); }

	// Compute band b's [lo,hi) sub-range of [begin,end) for `bands` even-ish splits.
	void bandRange(int b, int &lo, int &hi) const
	{
		const int total = end - begin;
		lo = begin + (int)((long long)total * b / bands);
		hi = begin + (int)((long long)total * (b + 1) / bands);
	}

	void runBand(int b)
	{
		int lo, hi;
		bandRange(b, lo, hi);
		if (lo >= hi) return;
		try {
			(*body)(lo, hi);
		} catch (...) {
			// Record the first error; the caller rethrows after join.
			std::lock_guard<std::mutex> lk(mtx);
			if (!error) error = std::current_exception();
		}
	}

	void workerLoop()
	{
		unsigned long long seen = 0;
		for (;;) {
			{
				std::unique_lock<std::mutex> lk(mtx);
				wake.wait(lk, [&]{ return stopping || generation != seen; });
				if (stopping) return;
				seen = generation;
			}
			// Claim and run bands until the worker bands are exhausted. Band
			// (bands-1) is reserved for the calling thread, so workers take
			// [0 .. bands-2].
			for (;;) {
				int b = nextBand.fetch_add(1);
				if (b >= bands - 1) break;
				runBand(b);
				if (remaining.fetch_sub(1) == 1) {
					std::lock_guard<std::mutex> lk(mtx);
					done.notify_one();
				}
			}
		}
	}

	void ensureStarted(int workers)
	{
		if (!threads.empty()) return;
		for (int i = 0; i < workers; ++i)
			threads.emplace_back([this]{ workerLoop(); });
	}

	void stop()
	{
		{
			std::lock_guard<std::mutex> lk(mtx);
			if (stopping) return;
			stopping = true;
		}
		wake.notify_all();
		for (auto &t : threads)
			if (t.joinable()) t.join();
		threads.clear();
	}
};

Pool &pool()
{
	static Pool s_pool;
	return s_pool;
}

} // anonymous namespace

// --- parallelFor -------------------------------------------------------------

void parallelFor(int begin, int end, const std::function<void(int,int)>& body)
{
	if (begin >= end) return;

	const int n = end - begin;
	const int bands = workerCount();

	// Serial fast path: single band, or a range too small to be worth waking
	// threads for. Avoids all synchronization overhead.
	const int kMinPerBand = 64;
	if (bands <= 1 || n < bands * kMinPerBand) {
		body(begin, end);
		return;
	}

	Pool &p = pool();
	const int workers = bands - 1;					// caller runs the last band

	// Only one parallelFor runs at a time (all callers are the UI thread, but
	// be explicit). Hold the dispatch lock for the whole call.
	static std::mutex s_dispatch;
	std::lock_guard<std::mutex> dispatch(s_dispatch);

	p.ensureStarted(workers);

	{
		std::lock_guard<std::mutex> lk(p.mtx);
		p.body = &body;
		p.begin = begin;
		p.end = end;
		p.bands = bands;
		p.error = nullptr;
		p.nextBand.store(0);
		p.remaining.store(workers);					// worker bands [0 .. bands-2]
		++p.generation;
	}
	p.wake.notify_all();

	// Caller runs the final band inline.
	p.runBand(bands - 1);

	// Wait for all worker bands to finish.
	{
		std::unique_lock<std::mutex> lk(p.mtx);
		p.done.wait(lk, [&]{ return p.remaining.load() == 0; });
	}

	// Surface the first worker exception, if any.
	std::exception_ptr err;
	{
		std::lock_guard<std::mutex> lk(p.mtx);
		err = p.error;
		p.body = nullptr;
	}
	if (err) std::rethrow_exception(err);
}

void shutdown()
{
	pool().stop();
}

} // namespace WBParallel
