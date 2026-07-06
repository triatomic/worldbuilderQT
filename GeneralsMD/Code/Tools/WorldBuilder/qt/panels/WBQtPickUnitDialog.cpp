// WBQtPickUnitDialog.cpp -- see WBQtPickUnitDialog.h. Layouts mirror IDD_PICKUNIT (search
// row / tree / preview swatch beside a tall OK over Cancel) and IDD_REPLACEUNIT (missing
// label / tree / OK-Cancel / "Continue without replacing...").
#include "WBQtPickUnitDialog.h"
#include "WBQtPickUnitBridge.h"
#include "WBQtPreviewImage.h"
#include "WBQtTreeStyle.h"

// Stage 1 phase 3: modal-dialog parent (active modal if nested, else main window). WBQtBridge.cpp.
QWidget *WBQt_DialogParent(void);

#include <QApplication>
#include <QHBoxLayout>
#include <QHash>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMoveEvent>
#include <QPixmap>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <qt_windows.h>

#include <string.h>

namespace
{
	const int kLeafRole = Qt::UserRole;
	const int kNameCap = 256;
	const int kPreviewW = 256;	// the 2x Qt render (see WBQT_PREVIEW_W in the bridge)
	const int kPreviewH = 256;
}

WBQtPickUnitDialog::WBQtPickUnitDialog(bool replaceMode, const QString &missingName, QWidget *parent)
	: QDialog(parent),
	m_replaceMode(replaceMode),
	m_panelMode(false),
	m_panelFactionOnly(0),
	m_searchEdit(NULL),
	m_tree(NULL),
	m_preview(NULL),
	m_cancelButton(NULL)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle(replaceMode ? "Replace Missing Unit" : "Pick A Unit");

	QVBoxLayout *root = new QVBoxLayout(this);

	if (replaceMode)
	{
		root->addWidget(new QLabel(missingName, this));
	}
	else
	{
		QHBoxLayout *searchRow = new QHBoxLayout();
		searchRow->addWidget(new QLabel("Search:", this));
		m_searchEdit = new QLineEdit(this);
		searchRow->addWidget(m_searchEdit, 1);
		QPushButton *findButton = new QPushButton("Find", this);
		findButton->setAutoDefault(false);
		searchRow->addWidget(findButton);
		QPushButton *resetButton = new QPushButton("Reset", this);
		resetButton->setAutoDefault(false);
		searchRow->addWidget(resetButton);
		root->addLayout(searchRow);
		connect(findButton, SIGNAL(clicked()), this, SLOT(onSearch()));
		connect(resetButton, SIGNAL(clicked()), this, SLOT(onReset()));
	}

	m_tree = new QTreeWidget(this);
	m_tree->setHeaderHidden(true);
	WBQtTreeStyle::applyTreeLines(m_tree);
	root->addWidget(m_tree, 1);
	connect(m_tree, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
			this, SLOT(onCurrentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));

	QPushButton *okButton = new QPushButton("OK", this);
	okButton->setDefault(true);
	QPushButton *cancelButton = new QPushButton("Cancel", this);
	cancelButton->setAutoDefault(false);
	m_cancelButton = cancelButton;
	connect(okButton, SIGNAL(clicked()), this, SLOT(accept()));
	connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

	// The model preview renders in BOTH modes: the MFC OnInitDialog creates the
	// ObjectPreview over the placeholder rect and force-shows it, so the replace dialog
	// had one too (the template's NOT WS_VISIBLE only hid the placeholder static).
	m_preview = new QLabel(this);
	m_preview->setFixedSize(96, 80);
	m_preview->setFrameShape(QFrame::StyledPanel);
	m_preview->setAlignment(Qt::AlignCenter);

	if (replaceMode)
	{
		QHBoxLayout *bottomRow = new QHBoxLayout();
		bottomRow->addWidget(m_preview);
		QVBoxLayout *buttonCol = new QVBoxLayout();
		QHBoxLayout *buttonRow = new QHBoxLayout();
		buttonRow->addWidget(okButton);
		buttonRow->addWidget(cancelButton);
		buttonRow->addStretch(1);
		buttonCol->addLayout(buttonRow);
		QPushButton *ignoreButton = new QPushButton("Continue without replacing...", this);
		ignoreButton->setAutoDefault(false);
		buttonCol->addWidget(ignoreButton, 0, Qt::AlignLeft);
		buttonCol->addStretch(1);
		bottomRow->addLayout(buttonCol, 1);
		root->addLayout(bottomRow);
		connect(ignoreButton, SIGNAL(clicked()), this, SLOT(onIgnore()));
	}
	else
	{
		// == IDD_PICKUNIT's bottom strip: preview swatch left, the tall OK over Cancel right.
		QHBoxLayout *bottomRow = new QHBoxLayout();
		bottomRow->addWidget(m_preview);
		QVBoxLayout *buttonCol = new QVBoxLayout();
		okButton->setMinimumHeight(44);
		buttonCol->addWidget(okButton);
		buttonCol->addWidget(cancelButton);
		bottomRow->addLayout(buttonCol, 1);
		root->addLayout(bottomRow);
	}

	populate(QString());

	resize(replaceMode ? QSize(380, 540) : QSize(320, 620));
}

// Rebuild the tree from the bridge catalog; a non-empty filter keeps only leaves whose name
// contains it case-insensitively (== OnSearch's lowercase substring match).
int WBQtPickUnitDialog::populate(const QString &filter)
{
	// The panel outlives any modal pick/replace dialog, and those rebuild the shared bridge
	// catalog with THEIR filters -- re-Build with ours before every read (== the MFC panel
	// snapshotting its own m_objectsList).
	if (m_panelMode)
	{
		WBQtPickUnitData_Build(m_panelAllowable.constData(), m_panelAllowable.size(),
			m_panelFactionOnly);
	}
	m_tree->clear();
	QString lowerFilter = filter.toLower();
	// == PickUnitDialog::addObject: [TEST/]side/editor-sorting/name, sorted at each level.
	QHash<QString, QTreeWidgetItem *> folders;
	char name[kNameCap];
	char side[kNameCap];
	char sorting[kNameCap];
	int matches = 0;
	for (int i = 0; ; i++)
	{
		name[0] = 0;
		side[0] = 0;
		sorting[0] = 0;
		int isTest = 0;
		if (WBQtPickUnitData_GetInfo(i, name, sizeof(name), side, sizeof(side),
				sorting, sizeof(sorting), &isTest) == 0)
		{
			break;
		}
		QString leafName = QString::fromLocal8Bit(name);
		if (!lowerFilter.isEmpty() && !leafName.toLower().contains(lowerFilter))
		{
			continue;
		}
		QStringList parts;
		if (isTest)
		{
			parts << "TEST";
		}
		parts << QString::fromLocal8Bit(side) << QString::fromLocal8Bit(sorting);
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
				folder->setData(0, kLeafRole, 0);
				folders.insert(key, folder);
			}
			parentItem = folder;
		}
		QTreeWidgetItem *leaf = new QTreeWidgetItem(parentItem, QStringList(leafName));
		leaf->setData(0, kLeafRole, 1);
		matches++;
	}
	m_tree->sortItems(0, Qt::AscendingOrder);
	return matches;
}

void WBQtPickUnitDialog::onSearch()
{
	// == PickUnitDialog::OnSearch: empty text beeps and restores the full list; no matches
	// informs; matches show expanded.
	QString filter = m_searchEdit->text();
	if (filter.isEmpty())
	{
		QApplication::beep();
		populate(QString());
		return;
	}
	int matches = populate(filter);
	if (matches == 0)
	{
		QMessageBox::information(this, "Search", "No matches found.");
	}
	else
	{
		m_tree->expandAll();
	}
}

void WBQtPickUnitDialog::onReset()
{
	// == PickUnitDialog::OnReset: repopulate the full list (the search text stays).
	populate(QString());
}

void WBQtPickUnitDialog::onIgnore()
{
	done(2);	// == EndDialog(IDIGNORE)
}

void WBQtPickUnitDialog::onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
	Q_UNUSED(previous);
	if (m_preview == NULL)
	{
		return;
	}
	// == the TVN_SELCHANGED handler: a leaf renders its template, anything else clears.
	if (current != NULL && current->data(0, kLeafRole).toInt() == 1)
	{
		refreshPreview(current->text(0));
	}
	else
	{
		m_preview->clear();
	}
}

void WBQtPickUnitDialog::refreshPreview(const QString &name)
{
	QByteArray nameBytes = name.toLocal8Bit();
	QByteArray bgr(kPreviewW * kPreviewH * 3, 0);
	if (WBQtPickUnit_RenderPreview(nameBytes.constData(),
			reinterpret_cast<unsigned char*>(bgr.data()), bgr.size()))
	{
		// Flip + convert + the MFC center-quarter zoom, shared with the other previews.
		QImage img = WBQtPreviewImage::fromBridgeBgr(
			reinterpret_cast<const unsigned char*>(bgr.constData()), kPreviewW, kPreviewH);
		m_preview->setPixmap(WBQtPreviewImage::toLabelPixmap(img, m_preview->size()));
	}
	else
	{
		m_preview->clear();
	}
}

void WBQtPickUnitDialog::accept()
{
	// == OnOK/getPickedUnit: a leaf yields the pick; a folder or nothing yields an empty pick
	// (callers treat it as a no-op, like getPickedThing() returning NULL).
	QTreeWidgetItem *item = m_tree->currentItem();
	if (item != NULL && item->data(0, kLeafRole).toInt() == 1)
	{
		m_pickedName = item->text(0);
	}
	else
	{
		m_pickedName.clear();
	}
	QDialog::accept();
}

// == PickUnitDialog::SetupAsPanel + the Create/ShowWindow(SW_SHOWNA) panel discipline:
// Qt::Tool floats it above the main window like the other floating panels, and
// WA_ShowWithoutActivating keeps show() from stealing activation from the 3D viewport.
// OK/Escape just hide it (QDialog accept/reject on a modeless dialog); BuildListTool
// re-shows it on the next activate.
void WBQtPickUnitDialog::setupAsPanel(const int *allowable, int allowCount, int factionOnly)
{
	m_panelMode = true;
	m_panelAllowable.clear();
	for (int i = 0; i < allowCount; i++)
	{
		m_panelAllowable.append(allowable[i]);
	}
	m_panelFactionOnly = factionOnly;
	setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) | Qt::Tool);
	setAttribute(Qt::WA_ShowWithoutActivating, true);
	if (m_cancelButton != NULL)
	{
		m_cancelButton->hide();	// == SetupAsPanel hiding IDCANCEL
	}
	populate(QString());
}

// The live selection (== the MFC panel's getPickedUnit): a leaf yields its template name,
// a folder or nothing yields empty. BuildListTool polls this on every hover/click.
QString WBQtPickUnitDialog::currentLeafName() const
{
	QTreeWidgetItem *item = m_tree->currentItem();
	if (item != NULL && item->data(0, kLeafRole).toInt() == 1)
	{
		return item->text(0);
	}
	return QString();
}

void WBQtPickUnitDialog::moveEvent(QMoveEvent *event)
{
	QDialog::moveEvent(event);
	// == PickUnitDialog::OnMove. The replace dialog's message map does not chain
	// PickUnitDialog's, so it never saved -- mirror that.
	if (!m_replaceMode && isVisible() && !isMinimized())
	{
		WBQtPickUnit_SavePos(frameGeometry().top(), frameGeometry().left());
	}
}

// ===================== the modal entry points =====================

namespace
{
	// Stage 1 phase 3: nesting-safe by parent (the ctor already parents to the active modal --
	// the team sheet's "..." flow -- else the caller passes NULL and Qt picks the app-modal
	// stack). Qt ApplicationModal fences the viewport via QWinHost WindowBlocked; the old
	// EnableWindow(frame) discipline is gone.
	int runPickModal(QDialog &dlg, void * /*frameHwnd*/)
	{
		dlg.setWindowModality(Qt::ApplicationModal);
		return dlg.exec();
	}

	void copyName(const QString &name, char *nameOut, int nameCap)
	{
		if (nameOut == NULL || nameCap <= 0)
		{
			return;
		}
		QByteArray bytes = name.toLocal8Bit();
		int n = bytes.size();
		if (n > nameCap - 1)
		{
			n = nameCap - 1;
		}
		memcpy(nameOut, bytes.constData(), n);
		nameOut[n] = 0;
	}
}

extern "C" int WBQtPickUnit_Run(void *frameHwnd, const int *allowable, int allowCount,
	int factionOnly, char *nameOut, int nameCap)
{
	if (qApp == NULL)
	{
		return -1;	// Qt not up yet -- the caller falls back to the MFC dialog
	}
	WBQtPickUnitData_Build(allowable, allowCount, factionOnly);
	// Parent to the active Qt modal (the team sheet's "..." flow) else the main window.
	WBQtPickUnitDialog dlg(false, QString(), WBQt_DialogParent());
	int rc = runPickModal(dlg, frameHwnd);
	copyName(dlg.pickedName(), nameOut, nameCap);
	return (rc == QDialog::Accepted) ? 1 : 0;
}

extern "C" int WBQtReplaceUnit_Run(void *frameHwnd, const char *missingName, const int *allowable,
	int allowCount, int factionOnly, char *nameOut, int nameCap)
{
	if (qApp == NULL)
	{
		return -1;	// Qt not up yet (command-line map load) -- fall back to the MFC dialog
	}
	WBQtPickUnitData_Build(allowable, allowCount, factionOnly);
	WBQtPickUnitDialog dlg(true, QString::fromLocal8Bit(missingName ? missingName : ""),
		WBQt_DialogParent());
	int rc = runPickModal(dlg, frameHwnd);
	copyName(dlg.pickedName(), nameOut, nameCap);
	if (rc == 2)
	{
		return 2;	// == IDIGNORE ("Continue without replacing...")
	}
	return (rc == QDialog::Accepted) ? 1 : 0;
}

// ===================== BuildListTool's modeless pick panel =====================

namespace
{
	// Created once per session on the first Show (== BuildListTool::createWindow's
	// once-per-session Create) and kept for reuse; hidden, never destroyed.
	WBQtPickUnitDialog *g_buildPickPanel = NULL;
}

extern "C" int WBQtBuildPickPanel_Show(const int *allowable, int allowCount, int factionOnly,
	int top, int left)
{
	if (qApp == NULL)
	{
		return 0;	// Qt not up -- BuildListTool falls back to the MFC panel
	}
	if (g_buildPickPanel == NULL)
	{
		WBQtPickUnitData_Build(allowable, allowCount, factionOnly);
		g_buildPickPanel = new WBQtPickUnitDialog(false, QString(), WBQt_DialogParent());
		g_buildPickPanel->setupAsPanel(allowable, allowCount, factionOnly);
		// Seed the saved BUILD_PICK_PANEL_SECTION position once; later shows keep
		// whatever the user dragged it to (moveEvent writes the drags back).
		g_buildPickPanel->move(left, top);
	}
	g_buildPickPanel->show();
	g_buildPickPanel->raise();
	return 1;
}

extern "C" void WBQtBuildPickPanel_Hide(void)
{
	if (g_buildPickPanel != NULL)
	{
		g_buildPickPanel->hide();
	}
}

extern "C" int WBQtBuildPickPanel_IsVisible(void)
{
	return (g_buildPickPanel != NULL && g_buildPickPanel->isVisible()) ? 1 : 0;
}

extern "C" void WBQtBuildPickPanel_GetPicked(char *nameOut, int nameCap)
{
	if (nameOut != NULL && nameCap > 0)
	{
		nameOut[0] = 0;
	}
	if (g_buildPickPanel != NULL)
	{
		copyName(g_buildPickPanel->currentLeafName(), nameOut, nameCap);
	}
}

extern "C" void WBQtBuildPickPanel_ResetPos(int top, int left)
{
	if (g_buildPickPanel != NULL)
	{
		g_buildPickPanel->move(left, top);
	}
}
