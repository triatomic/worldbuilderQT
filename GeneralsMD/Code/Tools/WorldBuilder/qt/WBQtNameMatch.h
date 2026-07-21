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

#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVector>

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

	// Replace-missing suggestion: scan a QTreeWidget for the leaf whose name is most similar to
	// `target` and, if the best clears `threshold`, select + scroll to it (so the user usually just
	// confirms) and return true. A "leaf" is a top-level-0 item carrying an int in role `leafRole`
	// that is >= `leafMin` (folders store a smaller sentinel). Returns false (selecting nothing) on
	// an empty target or when nothing clears the bar, so a caller can fall back to its own default
	// selection. The catalogs are small, so a single linear pass is fine.
	inline bool selectBestMatch(QTreeWidget *tree, const QString &target,
		int leafRole, int leafMin, float threshold = kSuggestThreshold)
	{
		if (tree == NULL || target.isEmpty())
		{
			return false;
		}
		QTreeWidgetItem *best = NULL;
		float bestScore = 0.0f;
		QTreeWidgetItemIterator it(tree);
		while (*it)
		{
			if ((*it)->data(0, leafRole).toInt() >= leafMin)
			{
				const float score = similarity(target, (*it)->text(0));
				if (score > bestScore)
				{
					bestScore = score;
					best = *it;
				}
			}
			++it;
		}
		if (best == NULL || bestScore < threshold)
		{
			return false;
		}
		tree->setCurrentItem(best);
		tree->scrollToItem(best);
		return true;
	}
}

#endif // WB_QT_NAME_MATCH_H
