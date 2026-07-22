// WBQtTerrainModalDialog.cpp -- see WBQtTerrainModalDialog.h. Mirrors IDD_TERRAIN_MODAL:
// the missing-texture path on top, the class-grouped texture tree, the selected name +
// swatch, OK/Cancel. Selecting a leaf drives the shared foreground texture class exactly
// like the MFC dialog (that side effect is what the MFC swatch rendered from).
#include "WBQtTerrainModalDialog.h"
#include "ui_WBQtTerrainModalDialog.h"
#include "WBQtTerrainModalBridge.h"
#include "WBQtTerrainMaterialBridge.h"
#include "WBQtTreeStyle.h"
#include "WBQtNameMatch.h"

// NewSearch toggle (WBQtObjectBridge.cpp): live-filter search when on.
extern "C" int WBQtConfig_GetNewSearch(void);

// Stage 1 phase 3: modal-dialog parent (active modal if nested, else main window). WBQtBridge.cpp.
QWidget *WBQt_DialogParent(void);

#include <QApplication>
#include <QHash>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QTreeWidget>

#include <qt_windows.h>

#include <string.h>

namespace
{
	const int kLeafRole = Qt::UserRole;
	const int kNameCap = 512;
}

WBQtTerrainModalDialog::WBQtTerrainModalDialog(const QString &missingPath, QWidget *parent)
	: QDialog(parent),
	m_ui(new Ui::WBQtTerrainModalDialog),
	m_picked(-1),
	m_matchIndex(-1)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// The static widget tree lives in WBQtTerrainModalDialog.ui; bind the members the
	// logic below uses, then wire what Designer can't express.
	m_ui->setupUi(this);

	m_tree = m_ui->tree;
	m_searchEdit = m_ui->searchEdit;
	m_findNextButton = m_ui->findNextButton;
	m_nameLabel = m_ui->nameLabel;
	m_preview = m_ui->preview;

	m_ui->missingPathLabel->setText(missingPath);	// ctor arg, so set at runtime
	// The missing path is "class/.../leaf"; the suggestion matches on its final component.
	const QString missingLeaf = missingPath.section('/', -1).section('\\', -1);
	WBQtTreeStyle::applyTreeLines(m_tree);

	connect(m_ui->okButton, SIGNAL(clicked()), this, SLOT(accept()));
	connect(m_ui->cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

	// A search row (matching the Pick/Replace Unit dialogs) filters the class-grouped texture
	// tree by name -- searching a big texture list beats scrolling it. The MFC dialog had none.
	connect(m_ui->findButton, SIGNAL(clicked()), this, SLOT(onSearch()));
	connect(m_ui->resetButton, SIGNAL(clicked()), this, SLOT(onReset()));
	if (WBQtConfig_GetNewSearch() != 0)
	{
		connect(m_searchEdit, SIGNAL(textChanged(QString)), this, SLOT(onSearchLive(QString)));
	}
	// "Find Next" steps through the other close name matches; the suggestion below pre-selects the
	// best. Disabled when there are zero or one matches to cycle.
	connect(m_findNextButton, SIGNAL(clicked()), this, SLOT(onFindNextMatch()));

	connect(m_tree, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
			this, SLOT(onCurrentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));

	populate(QString());
	// Rank the close name matches to the missing texture (best-first; leaves carry texClass >= 0)
	// and keep their NAMES, so a later search/reset can't dangle them. Prefer a strong match as the
	// start for a "replace missing" pick; only when none clears the bar fall back to the default
	// class (first unused, == the MFC dialog). Ordered so the default's currentItemChanged side
	// effects fire only when it's the final selection -- not seeded then immediately superseded.
	const QList<QTreeWidgetItem *> ranked = WBQtNameMatch::rankMatches(m_tree, missingLeaf, kLeafRole, 0);
	for (int i = 0; i < ranked.size(); ++i)
	{
		m_matchNames.append(ranked.at(i)->text(0));
	}
	if (!m_matchNames.isEmpty())
	{
		selectMatch(0);
	}
	else
	{
		selectTexClass(WBQtTerrainModalData_GetInitialSelection());
	}
	m_findNextButton->setEnabled(m_matchNames.size() > 1);
}

// == updateTextures: build [class or **LegacyGDF/path] / leaf from the bridge rows.
// WBQtTerrainModal_Run built the rows (with the real heightmap) before constructing the dialog;
// this only reads them back. A non-empty filter keeps only leaves whose name contains it
// case-insensitively (== the unit picker's OnSearch substring match). Returns the leaf count.
int WBQtTerrainModalDialog::populate(const QString &filter)
{
	m_tree->clear();
	const QString lowerFilter = filter.toLower();
	QHash<QString, QTreeWidgetItem *> folders;
	char group[kNameCap];
	char leaf[kNameCap];
	int matches = 0;
	for (int i = 0; ; i++)
	{
		group[0] = 0;
		leaf[0] = 0;
		int texClass = -1;
		WBQtTerrainModalData_GetInfo(i, group, sizeof(group), leaf, sizeof(leaf), &texClass);
		if (texClass < 0 && leaf[0] == 0)
		{
			break;
		}
		QString leafName = QString::fromLocal8Bit(leaf);
		if (!lowerFilter.isEmpty() && !leafName.toLower().contains(lowerFilter))
		{
			continue;
		}
		QStringList parts = QString::fromLocal8Bit(group).split('/', QString::SkipEmptyParts);
		QTreeWidgetItem *parentItem = NULL;
		QString key;
		for (int p = 0; p < parts.size(); p++)
		{
			key += parts[p];
			key += '/';
			QTreeWidgetItem *folder = folders.value(key, NULL);
			if (folder == NULL)
			{
				if (parentItem == NULL)
				{
					folder = new QTreeWidgetItem(m_tree, QStringList(parts[p]));
				}
				else
				{
					folder = new QTreeWidgetItem(parentItem, QStringList(parts[p]));
				}
				folder->setData(0, kLeafRole, -1);
				folders.insert(key, folder);
			}
			parentItem = folder;
		}
		QTreeWidgetItem *item = new QTreeWidgetItem(parentItem, QStringList(leafName));
		item->setData(0, kLeafRole, texClass);
		matches++;
	}
	return matches;
}

void WBQtTerrainModalDialog::onSearch()
{
	// == the unit picker's OnSearch: empty text beeps and restores the full list; no matches
	// informs; matches show expanded.
	QString filter = m_searchEdit->text();
	if (filter.isEmpty())
	{
		QApplication::beep();
		populate(QString());
		return;
	}
	if (populate(filter) == 0)
	{
		QMessageBox::information(this, "Search", "No matches found.");
	}
	else
	{
		m_tree->expandAll();
	}
}

void WBQtTerrainModalDialog::onSearchLive(const QString &text)
{
	// NewSearch: filter live -- empty box restores the full list, no beep / no message box.
	if (text.trimmed().isEmpty())
	{
		populate(QString());
		return;
	}
	if (populate(text) > 0)
	{
		m_tree->expandAll();
	}
}

void WBQtTerrainModalDialog::onReset()
{
	populate(QString());
}

// Select + scroll to the ranked match at `index` and remember the cursor. The match is looked up
// by name in the CURRENT tree (m_matchNames stores names, not pointers), so it survives a tree
// rebuild; if a substring Search has filtered that name out, there's simply nothing to select.
void WBQtTerrainModalDialog::selectMatch(int index)
{
	if (index < 0 || index >= m_matchNames.size())
	{
		return;
	}
	m_matchIndex = index;
	const QList<QTreeWidgetItem *> found =
		m_tree->findItems(m_matchNames.at(index), Qt::MatchExactly | Qt::MatchRecursive);
	if (!found.isEmpty())
	{
		m_tree->setCurrentItem(found.first());
		m_tree->scrollToItem(found.first());
	}
}

// Step to the next close name match, wrapping past the end back to the best.
void WBQtTerrainModalDialog::onFindNextMatch()
{
	if (m_matchNames.isEmpty())
	{
		return;
	}
	selectMatch((m_matchIndex + 1) % m_matchNames.size());
}

WBQtTerrainModalDialog::~WBQtTerrainModalDialog()
{
	delete m_ui;
}

// == setTerrainTreeViewSelection: find and select the leaf carrying this class.
void WBQtTerrainModalDialog::selectTexClass(int texClass)
{
	QTreeWidgetItemIterator it(m_tree);
	while (*it)
	{
		if ((*it)->data(0, kLeafRole).toInt() == texClass)
		{
			m_tree->setCurrentItem(*it);
			m_tree->scrollToItem(*it);
			return;
		}
		++it;
	}
}

void WBQtTerrainModalDialog::onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
	Q_UNUSED(previous);
	// == the TVN_SELCHANGED handler: only leaves change the pick / fg class / preview.
	if (current == NULL)
	{
		return;
	}
	int texClass = current->data(0, kLeafRole).toInt();
	if (texClass < 0)
	{
		return;
	}
	m_picked = texClass;
	WBQtTerrainModal_SetFgTexClass(texClass);
	char name[kNameCap];
	name[0] = 0;
	WBQtTerrainModalData_GetUiNameLeaf(texClass, name, sizeof(name));
	m_nameLabel->setText(QString::fromLocal8Bit(name));
	refreshPreview(texClass);
}

// Swatch pixels via the WBQtTerrainMaterial bridge (bottom-up BGRA; same conversion as
// the Terrain Material panel).
void WBQtTerrainModalDialog::refreshPreview(int texClass)
{
	int extent = WBQtTerrainMaterial_GetSwatchExtent();
	if (extent <= 0)
	{
		m_preview->setText("(n/a)");
		return;
	}
	QByteArray bgra(extent * extent * 4, 0);
	if (!WBQtTerrainMaterial_GetSwatchPixels(texClass, reinterpret_cast<unsigned char*>(bgra.data()), bgra.size()))
	{
		QPixmap pm(extent, extent);
		pm.fill(QColor(0, 128, 0));
		m_preview->setPixmap(pm.scaled(m_preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
		return;
	}
	QImage img(extent, extent, QImage::Format_RGB888);
	for (int y = 0; y < extent; ++y)
	{
		const unsigned char *src = reinterpret_cast<const unsigned char*>(bgra.constData()) + (extent - 1 - y) * extent * 4;
		unsigned char *dst = img.scanLine(y);
		for (int x = 0; x < extent; ++x)
		{
			dst[x * 3 + 0] = src[x * 4 + 2];
			dst[x * 3 + 1] = src[x * 4 + 1];
			dst[x * 3 + 2] = src[x * 4 + 0];
		}
	}
	m_preview->setPixmap(QPixmap::fromImage(img).scaled(m_preview->size(),
		Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

// ===================== the modal entry point =====================

extern "C" int WBQtTerrainModal_Run(void * /*frameHwnd*/, const char *missingPath, void *heightMapEdit,
	int *pickedOut)
{
	if (pickedOut != NULL)
	{
		*pickedOut = -1;
	}
	if (qApp == NULL)
	{
		return -1;	// map validated before WBQt_Startup -- fall back to the MFC dialog
	}
	WBQtTerrainModalData_Build(heightMapEdit);
	// Stage 1 phase 3: parent to the active modal (nested — this can run during a map-load
	// flow) else the main window; Qt ApplicationModal fences the viewport.
	WBQtTerrainModalDialog dlg(QString::fromLocal8Bit(missingPath ? missingPath : ""),
		WBQt_DialogParent());
	dlg.setWindowModality(Qt::ApplicationModal);
	int rc = dlg.exec();
	if (pickedOut != NULL)
	{
		*pickedOut = dlg.pickedIndex();
	}
	return (rc == QDialog::Accepted) ? 1 : 0;
}
