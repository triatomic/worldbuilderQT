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

// WBParallel.h
//
// A tiny persistent fork-join thread pool for data-parallel loops in the
// WorldBuilder tool (minimap resample, viewport label projection) and the
// shared map-load decode. NOT a general task system: parallelFor() splits a
// half-open integer range into N disjoint contiguous sub-ranges, runs them on
// pooled worker threads (plus the calling thread), and joins before returning.
// Each sub-range owns its output exclusively, so the loop body needs no locks.
//
// Lives in Core/WWLib (on the engine's public include path) so the WorldBuilder
// tool AND the shared game-engine-device lib can both use it.
//
// Modern C++ only (std::thread). Guard engine-shared callers behind the same
// non-VS6 condition the build already uses for its C++17 sources; the legacy
// VC6 toolchain has no <thread> and must keep its serial loop.

#pragma once

#include <functional>

namespace WBParallel
{
	// Number of worker bands a parallelFor splits into:
	//   clamp(hardware_concurrency() - 1, 1, 8), leaving a core for the UI.
	// Returns 1 (forcing fully-serial execution) when the environment variable
	// WB_PARALLEL is set to "0" -- used for byte-for-byte A/B validation against
	// the single-threaded path without a rebuild. Computed once and cached.
	int workerCount();

	// Run body() over the half-open range [begin, end), split into workerCount()
	// disjoint contiguous bands. body is invoked as body(bandBegin, bandEnd) on
	// one or more threads concurrently; it MUST only touch storage unique to its
	// band (no shared writes, no non-thread-safe singletons / GDI / D3D). This
	// call blocks until every band has finished (fork-join). For a trivially
	// small range or workerCount()==1 it runs inline on the caller -- no threads.
	// An exception escaping a worker band is rethrown on the calling thread.
	void parallelFor(int begin, int end, const std::function<void(int, int)>& body);

	// Join and destroy the worker threads. Safe to call if the pool was never
	// started. Call once at app shutdown (CWorldBuilderApp::ExitInstance()).
	void shutdown();
}
