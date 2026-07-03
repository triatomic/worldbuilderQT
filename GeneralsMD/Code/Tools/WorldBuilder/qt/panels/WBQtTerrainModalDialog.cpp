// WBQtTerrainModalDialog.cpp -- see WBQtTerrainModalDialog.h. Mirrors IDD_TERRAIN_MODAL:
// the missing-texture path on top, the class-grouped texture tree, the selected name +
// swatch, OK/Cancel. Selecting a leaf drives the shared foreground texture class exactly
// like the MFC dialog (that side effect is what the MFC swatch rendered from).
#include "WBQtTerrainModalDialog.h"
#include "WBQtTerrainModalBridge.h"
#include "WBQtTerrainMaterialBridge.h"
#include "WBQtTreeStyle.h"

// Stage 1 phase 3: modal-dialog parent (active modal if nested, else main window). WBQtBridge.cpp.
QWidget *WBQt_DialogParent(void);

#include <QApplication>
#include <QHBoxLayout>
#include <QHash>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <qt_windows.h>

#include <string.h>

namespace
{
	const int kLeafRole = Qt::UserRole;
	const int kNameCap = 512;
}

WBQtTerrainModalDialog::WBQtTerrainModalDialog(const QString &missingPath, QWidget *parent)
	: QDialog(parent),
	m_picked(-1),
	m_tree(NULL),
	m_nameLabel(NULL),
	m_preview(NULL)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle("Replace Missing Texture");

	QVBoxLayout *root = new QVBoxLayout(this);

	root->addWidget(new QLabel(missingPath, this));

	m_tree = new QTreeWidget(this);
	m_tree->setHeaderHidden(true);
	WBQtTreeStyle::applyTreeLines(m_tree);
	root->addWidget(m_tree, 1);

	QHBoxLayout *bottomRow = new QHBoxLayout();
	m_preview = new QLabel(this);
	m_preview->setFixedSize(96, 96);
	m_preview->setFrameShape(QFrame::StyledPanel);
	m_preview->setAlignment(Qt::AlignCenter);
	bottomRow->addWidget(m_preview);
	QVBoxLayout *rightCol = new QVBoxLayout();
	m_nameLabel = new QLabel(this);
	rightCol->addWidget(m_nameLabel);
	QHBoxLayout *buttons = new QHBoxLayout();
	buttons->addStretch(1);
	QPushButton *okButton = new QPushButton("OK", this);
	okButton->setDefault(true);
	buttons->addWidget(okButton);
	QPushButton *cancelButton = new QPushButton("Cancel", this);
	cancelButton->setAutoDefault(false);
	buttons->addWidget(cancelButton);
	rightCol->addLayout(buttons);
	rightCol->addStretch(1);
	bottomRow->addLayout(rightCol, 1);
	root->addLayout(bottomRow);
	connect(okButton, SIGNAL(clicked()), this, SLOT(accept()));
	connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

	// == updateTextures: build [class or **LegacyGDF/path] / leaf from the bridge rows.
	// WBQtTerrainModal_Run built the rows (with the real heightmap) before constructing
	// the dialog; the ctor only reads them back.
	QHash<QString, QTreeWidgetItem *> folders;
	char group[kNameCap];
	char leaf[kNameCap];
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
		QTreeWidgetItem *item = new QTreeWidgetItem(parentItem, QStringList(QString::fromLocal8Bit(leaf)));
		item->setData(0, kLeafRole, texClass);
	}

	connect(m_tree, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
			this, SLOT(onCurrentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));

	selectTexClass(WBQtTerrainModalData_GetInitialSelection());

	resize(380, 560);
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
