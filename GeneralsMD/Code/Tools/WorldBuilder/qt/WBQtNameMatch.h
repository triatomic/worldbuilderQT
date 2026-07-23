// WBQtNameMatch.h -- fuzzy name similarity for the "replace missing X" dialogs.
//
// When a map references a unit or terrain texture the current data set no longer has, the replace
// dialogs ask the user to pick a substitute. To save hunting through the tree, they pre-select the
// candidate whose name is closest to the missing one. This is that closeness metric: a normalized
// similarity in [0,1] from the Levenshtein edit distance (1 - dist/maxLen), compared
// case-insensitively. A caller uses a threshold (~0.70) to decide whether the best candidate is a
// good enough suggestion to auto-select.
#ifndef WB_QT_NAME_MATCH_H
#define WB_QT_NAME_MATCH_H

#include <QAbstractButton>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVector>

#include <algorithm>

namespace WBQtNameMatch
{
	// Default "good enough to auto-select" bar for the replace-missing suggestion (see below).
	const float kSuggestThreshold = 0.70f;

	// Case-insensitive normalized similarity in [0,1] (1 == equal ignoring case, 0 == nothing in
	// common). Two empty strings are treated as identical.
	inline float similarity(const QString &aIn, const QString &bIn)
	{
		const QString a = aIn.toLower();
		const QString b = bIn.toLower();
		const int la = a.size();
		const int lb = b.size();
		if (la == 0 && lb == 0)
		{
			return 1.0f;
		}
		if (la == 0 || lb == 0)
		{
			return 0.0f;
		}
		// Levenshtein edit distance, two-row rolling buffer (C++98-friendly).
		QVector<int> prev(lb + 1);
		QVector<int> curr(lb + 1);
		for (int j = 0; j <= lb; ++j)
		{
			prev[j] = j;
		}
		for (int i = 1; i <= la; ++i)
		{
			curr[0] = i;
			for (int j = 1; j <= lb; ++j)
			{
				const int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
				int best = prev[j] + 1;			// deletion
				const int ins = curr[j - 1] + 1;	// insertion
				if (ins < best)
				{
					best = ins;
				}
				const int sub = prev[j - 1] + cost;	// substitution
				if (sub < best)
				{
					best = sub;
				}
				curr[j] = best;
			}
			prev.swap(curr);	// O(1); curr is fully overwritten before being read next row
		}
		const int dist = prev[lb];
		const int maxLen = (la > lb) ? la : lb;
		return 1.0f - (float(dist) / float(maxLen));
	}

	// One scored leaf, for ranking (see rankMatches).
	struct ScoredLeaf
	{
		QTreeWidgetItem *item;
		float score;
	};

	// Sort predicate: higher score first (ties keep tree order, which std::stable_sort preserves).
	inline bool scoredLeafBetter(const ScoredLeaf &a, const ScoredLeaf &b)
	{
		return a.score > b.score;
	}

	// Collect every leaf whose name is at least `threshold` similar to `target`, sorted best-first.
	// A "leaf" is a top-level-0 item carrying an int in role `leafRole` that is >= `leafMin` (folders
	// store a smaller sentinel). Empty when the target is empty or nothing clears the bar. The
	// dialogs use this to pre-select the closest match and let a "Find Next" button step through the
	// runners-up. Catalogs are small (and the >= threshold set is smaller), so this is cheap.
	inline QList<QTreeWidgetItem *> rankMatches(QTreeWidget *tree, const QString &target,
		int leafRole, int leafMin, float threshold = kSuggestThreshold)
	{
		QList<QTreeWidgetItem *> out;
		if (tree == NULL || target.isEmpty())
		{
			return out;
		}
		QVector<ScoredLeaf> scored;
		QTreeWidgetItemIterator it(tree);
		while (*it)
		{
			if ((*it)->data(0, leafRole).toInt() >= leafMin)
			{
				const float score = similarity(target, (*it)->text(0));
				if (score >= threshold)
				{
					ScoredLeaf s;
					s.item = *it;
					s.score = score;
					scored.push_back(s);
				}
			}
			++it;
		}
		std::stable_sort(scored.begin(), scored.end(), scoredLeafBetter);
		for (int i = 0; i < scored.size(); ++i)
		{
			out.push_back(scored[i].item);
		}
		return out;
	}

	// Replace-missing suggestion: select + scroll to the leaf whose name is most similar to `target`
	// (if any clears `threshold`) and return true; else select nothing and return false, so a caller
	// can fall back to its own default selection. Thin wrapper over rankMatches (the top-ranked hit).
	inline bool selectBestMatch(QTreeWidget *tree, const QString &target,
		int leafRole, int leafMin, float threshold = kSuggestThreshold)
	{
		const QList<QTreeWidgetItem *> ranked = rankMatches(tree, target, leafRole, leafMin, threshold);
		if (ranked.isEmpty())
		{
			return false;
		}
		tree->setCurrentItem(ranked.first());
		tree->scrollToItem(ranked.first());
		return true;
	}

	// Select + scroll to the (first) leaf whose text is exactly `name` in the CURRENT tree. The
	// replace dialogs track their matches by name, not item pointer, so a rebuild (search/reset)
	// can't dangle them; if a substring filter hid the name, there's simply nothing to select.
	inline void selectLeafByName(QTreeWidget *tree, const QString &name)
	{
		const QList<QTreeWidgetItem *> found =
			tree->findItems(name, Qt::MatchExactly | Qt::MatchRecursive);
		if (!found.isEmpty())
		{
			tree->setCurrentItem(found.first());
			tree->scrollToItem(found.first());
		}
	}

	// Cursor over the ranked match names, backing the replace dialogs' "Find Next" / "^" buttons:
	// holds the best-first name list plus the current position, and steps through it with
	// wrap-around in either direction. Kept by name (see selectLeafByName) so tree rebuilds are
	// harmless.
	class MatchCursor
	{
	public:
		MatchCursor() : m_index(0) {}

		void reset(const QStringList &names)
		{
			m_names = names;
			m_index = 0;
		}

		bool isEmpty() const { return m_names.isEmpty(); }
		int size() const { return m_names.size(); }

		// Callers must check isEmpty() first.
		const QString &current() const { return m_names.at(m_index); }

		// Step forward (dir = +1) or back (dir = -1) with wrap-around; returns the new current
		// name. Callers must check isEmpty() first.
		const QString &step(int dir)
		{
			const int n = m_names.size();
			m_index = (m_index + dir + n) % n;
			return m_names.at(m_index);
		}

	private:
		QStringList m_names;
		int m_index;
	};

	// The pickers' shared arming sequence: rank the close name matches to `target` (best-first,
	// leaves per leafRole/leafMin), load their names into the cursor, pre-select the best in the
	// tree, and enable the Find Next / "^" buttons only when there's more than one to cycle.
	// Returns true when at least one match cleared the bar (callers with a fallback selection --
	// e.g. the texture dialog's default class -- check this).
	inline bool armMatchCursor(MatchCursor &cursor, QTreeWidget *tree, const QString &target,
		int leafRole, int leafMin, QAbstractButton *nextButton, QAbstractButton *prevButton)
	{
		const QList<QTreeWidgetItem *> ranked = rankMatches(tree, target, leafRole, leafMin);
		QStringList names;
		for (int i = 0; i < ranked.size(); ++i)
		{
			names.append(ranked.at(i)->text(0));
		}
		cursor.reset(names);
		if (!cursor.isEmpty())
		{
			selectLeafByName(tree, cursor.current());
		}
		if (nextButton != NULL)
		{
			nextButton->setEnabled(cursor.size() > 1);
		}
		if (prevButton != NULL)
		{
			prevButton->setEnabled(cursor.size() > 1);
		}
		return !cursor.isEmpty();
	}

	// The Find Next / "^" slot body: step the cursor (dir = +1 / -1, wrap-around) and select the
	// new current match in the tree. No-op while the cursor is empty.
	inline void stepMatchCursor(MatchCursor &cursor, QTreeWidget *tree, int dir)
	{
		if (!cursor.isEmpty())
		{
			selectLeafByName(tree, cursor.step(dir));
		}
	}
}

#endif // WB_QT_NAME_MATCH_H
