// WBQtMapFileDialogs.cpp -- see WBQtMapFileDialogs.h. Layouts mirror IDD_OPEN_MAP /
// IDD_SAVE_MAP (the push-like mode strip on top, the map list, search / name row).
#include "WBQtMapFileDialogs.h"
#include "ui_WBQtOpenMapDialog.h"
#include "ui_WBQtSaveMapDialog.h"
#include "WBQtMapFileBridge.h"
#include "WBQtPreviewImage.h"

#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

#include <qt_windows.h>

#include <string.h>

// Stage 1 phase 3: the parent for a modal Qt dialog (active modal if nested, else the main
// window). Defined in WBQtBridge.cpp.
QWidget *WBQt_DialogParent(void);

namespace
{
	const int kPathCap = 1024;

	int runMapModal(QDialog &dlg, void * /*frameHwnd*/)
	{
		// Stage 1 phase 3: parent to the Qt main window (or the active modal, if nested) and
		// rely on Qt ApplicationModal -- the QWinHost WindowBlocked path fences the hosted
		// viewport, so the old EnableWindow(frame) discipline is gone.
		dlg.setParent(WBQt_DialogParent(), dlg.windowFlags());
		dlg.setWindowModality(Qt::ApplicationModal);
		int rc = dlg.exec();
		return (rc == QDialog::Accepted) ? 1 : 0;
	}
}

// ===================== WBQtOpenMapDialog =====================

WBQtOpenMapDialog::WBQtOpenMapDialog(QWidget *parent)
	: QDialog(parent),
	m_ui(new Ui::WBQtOpenMapDialog),
	m_updating(false)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// The static widget tree lives in WBQtOpenMapDialog.ui; bind the members the
	// logic below uses. The preview label shows the <name>.tga next to the .map in
	// the map's folder, the same image the game lobby shows.
	m_ui->setupUi(this);

	m_packedButton = m_ui->packedButton;
	m_userButton = m_ui->userButton;
	m_systemButton = m_ui->systemButton;
	m_searchEdit = m_ui->searchEdit;
	m_list = m_ui->list;
	m_preview = m_ui->preview;
	m_okButton = m_ui->okButton;

	if (WBQtMapFileData_RadiosVisible() == 0)
	{
		// == the MFC release hide (System/User hidden; Packed stays).
		m_userButton->hide();
		m_systemButton->hide();
	}

	connect(m_packedButton, SIGNAL(clicked()), this, SLOT(onModeClicked()));
	connect(m_userButton, SIGNAL(clicked()), this, SLOT(onModeClicked()));
	connect(m_systemButton, SIGNAL(clicked()), this, SLOT(onModeClicked()));
	connect(m_ui->findBtn, SIGNAL(clicked()), this, SLOT(onFind()));
	connect(m_ui->resetBtn, SIGNAL(clicked()), this, SLOT(onReset()));
	connect(m_ui->browseBtn, SIGNAL(clicked()), this, SLOT(onBrowse()));
	connect(m_list, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(onDoubleClicked()));
	connect(m_list, SIGNAL(currentRowChanged(int)), this, SLOT(onSelectionChanged()));
	connect(m_okButton, SIGNAL(clicked()), this, SLOT(accept()));
	connect(m_ui->cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

	reload();
	resize(700, 480);
}

WBQtOpenMapDialog::~WBQtOpenMapDialog()
{
	delete m_ui;
}

void WBQtOpenMapDialog::reload()
{
	m_updating = true;
	int mode = WBQtOpenMapData_GetMode();
	m_packedButton->setChecked(mode == WB_QT_MAPMODE_PACKED);
	m_userButton->setChecked(mode == WB_QT_MAPMODE_USER);
	m_systemButton->setChecked(mode == WB_QT_MAPMODE_SYSTEM);

	int cur = WBQtOpenMapData_ListCurSel();
	m_list->clear();
	char buf[kPathCap];
	int count = WBQtOpenMapData_ListCount();
	for (int i = 0; i < count; i++)
	{
		buf[0] = 0;
		WBQtOpenMapData_ListItem(i, buf, sizeof(buf));
		new QListWidgetItem(QString::fromLocal8Bit(buf), m_list);
	}
	if (cur >= 0 && cur < count)
	{
		m_list->setCurrentRow(cur);
	}
	m_okButton->setEnabled(WBQtOpenMapData_OkEnabled() != 0);
	m_updating = false;
	updatePreview();
}

void WBQtOpenMapDialog::onSelectionChanged()
{
	if (m_updating)
	{
		return;	// reload() refreshes the preview itself after reseeding the list
	}
	updatePreview();
}

void WBQtOpenMapDialog::updatePreview()
{
	// One bridge call for every mode (disk or packed) -- the bridge owns the storage
	// knowledge, like the other panels' _RenderPreview bridges. TGA has no magic
	// header, so the format is passed to the (deployed) qtga plugin explicitly.
	if (m_previewBuf.isEmpty())
	{
		m_previewBuf.resize(1024 * 1024);	// previews are ~100KB; one-time buffer
	}
	int n = WBQtOpenMapData_ItemPreviewData(m_list->currentRow(),
		reinterpret_cast<unsigned char *>(m_previewBuf.data()), m_previewBuf.size());
	if (n > 0)
	{
		QImage img = QImage::fromData(
			reinterpret_cast<const uchar *>(m_previewBuf.constData()), n, "TGA");
		if (!img.isNull())
		{
			m_preview->setPixmap(WBQtPreviewImage::toLabelPixmap(img, m_preview->size()));
			return;
		}
	}
	m_preview->setPixmap(QPixmap());
	m_preview->setText("(no preview)");
}

void WBQtOpenMapDialog::onModeClicked()
{
	if (m_updating)
	{
		return;
	}
	int mode = WB_QT_MAPMODE_USER;
	if (sender() == m_packedButton)
	{
		mode = WB_QT_MAPMODE_PACKED;
	}
	else if (sender() == m_systemButton)
	{
		mode = WB_QT_MAPMODE_SYSTEM;
	}
	WBQtOpenMap_SetMode(mode);
	reload();
}

void WBQtOpenMapDialog::onFind()
{
	QByteArray text = m_searchEdit->text().toLocal8Bit();
	WBQtOpenMap_Search(text.constData());
	reload();
}

void WBQtOpenMapDialog::onReset()
{
	m_searchEdit->clear();
	WBQtOpenMap_ResetSearch();
	reload();
}

void WBQtOpenMapDialog::onBrowse()
{
	// Packed mode: pops the MFC .big chooser then relists here; normal mode completes with
	// the standard-file-dialog fallback.
	if (WBQtOpenMap_BrowsePick() != 0)
	{
		QDialog::accept();
		return;
	}
	reload();
}

void WBQtOpenMapDialog::onDoubleClicked()
{
	accept();
}

void WBQtOpenMapDialog::accept()
{
	// == OnOK's first check: Enter pressed in the search box performs a search instead of
	// opening a map (the list always pre-selects row 0, so without this Enter-to-search would
	// silently open the wrong map). Only when the search box has focus and holds text.
	if (m_searchEdit != NULL && m_searchEdit->hasFocus() && !m_searchEdit->text().isEmpty())
	{
		onFind();
		return;
	}

	// == OK/double-click: completes with a map (or drills into an archive and stays open).
	if (WBQtOpenMap_Pick(m_list->currentRow()) != 0)
	{
		QDialog::accept();
		return;
	}
	reload();	// drilled into a .big (or nothing selected in packed mode)
}

// ===================== WBQtSaveMapDialog =====================

WBQtSaveMapDialog::WBQtSaveMapDialog(const QString &initialFilename, QWidget *parent)
	: QDialog(parent),
	m_ui(new Ui::WBQtSaveMapDialog),
	m_updating(false),
	m_browse(false),
	m_systemDir(WBQtSaveMapData_GetUseSystemDir() != 0)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// The static widget tree lives in WBQtSaveMapDialog.ui; bind the members the
	// logic below uses.
	m_ui->setupUi(this);

	m_systemButton = m_ui->systemButton;
	m_userButton = m_ui->userButton;
	m_list = m_ui->list;
	m_nameEdit = m_ui->nameEdit;

	if (WBQtMapFileData_RadiosVisible() == 0)
	{
		m_userButton->hide();
		m_systemButton->hide();
	}

	connect(m_systemButton, SIGNAL(clicked()), this, SLOT(onModeClicked()));
	connect(m_userButton, SIGNAL(clicked()), this, SLOT(onModeClicked()));
	connect(m_list, SIGNAL(itemSelectionChanged()), this, SLOT(onListSelectionChanged()));
	connect(m_ui->browseBtn, SIGNAL(clicked()), this, SLOT(onBrowse()));
	connect(m_ui->okBtn, SIGNAL(clicked()), this, SLOT(accept()));
	connect(m_ui->cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

	reload();

	// == populateMapListbox's edit seed: strip the path and the ".map" extension.
	QString name = initialFilename;
	name.replace('/', '\\');
	int slash = name.lastIndexOf('\\');
	if (slash >= 0)
	{
		name = name.mid(slash + 1);
	}
	if (name.endsWith(".map", Qt::CaseInsensitive))
	{
		name.chop(4);
	}
	m_nameEdit->setText(name);
	m_nameEdit->selectAll();
	m_nameEdit->setFocus();

	resize(460, 420);
}

WBQtSaveMapDialog::~WBQtSaveMapDialog()
{
	delete m_ui;
}

void WBQtSaveMapDialog::reload()
{
	m_updating = true;
	m_systemButton->setChecked(m_systemDir);
	m_userButton->setChecked(!m_systemDir);
	m_list->clear();
	char buf[kPathCap];
	int count = WBQtSaveMapData_Enumerate(m_systemDir ? 1 : 0);
	for (int i = 0; i < count; i++)
	{
		buf[0] = 0;
		WBQtSaveMapData_GetMapName(i, buf, sizeof(buf));
		new QListWidgetItem(QString::fromLocal8Bit(buf), m_list);
	}
	m_list->sortItems(Qt::AscendingOrder);	// == the LBS_SORT display
	m_updating = false;
}

QString WBQtSaveMapDialog::mapName() const
{
	return m_nameEdit->text();
}

void WBQtSaveMapDialog::onModeClicked()
{
	if (m_updating)
	{
		return;
	}
	m_systemDir = (sender() == m_systemButton);
	reload();
}

void WBQtSaveMapDialog::onListSelectionChanged()
{
	if (m_updating)
	{
		return;
	}
	QListWidgetItem *item = m_list->currentItem();
	if (item != NULL)
	{
		m_nameEdit->setText(item->text());
	}
}

void WBQtSaveMapDialog::onBrowse()
{
	// == OnBrowse: hand back to the standard save-file dialog.
	m_browse = true;
	QDialog::accept();
}

void WBQtSaveMapDialog::accept()
{
	// == OnOK: the overwrite prompt may keep the dialog open.
	QByteArray name = m_nameEdit->text().toLocal8Bit();
	if (WBQtSaveMap_ConfirmOverwrite(name.constData(), m_systemDir ? 1 : 0) == 0)
	{
		return;
	}
	m_browse = false;
	QDialog::accept();
}

// ===================== the modal entry points =====================

extern "C" int WBQtOpenMap_Run(void *frameHwnd, char *filenameOut, int cap, int *browseOut)
{
	if (filenameOut != NULL && cap > 0)
	{
		filenameOut[0] = 0;
	}
	if (browseOut != NULL)
	{
		*browseOut = 0;
	}
	WBQtOpenMapData_Open();
	WBQtOpenMapDialog dlg;
	int rc = runMapModal(dlg, frameHwnd);
	if (rc != 0)
	{
		WBQtOpenMapData_GetResult(filenameOut, cap, browseOut);
	}
	WBQtOpenMapData_Close();
	return rc;
}

extern "C" int WBQtSaveMap_Run(void *frameHwnd, const char *initialFilename,
	char *filenameOut, int cap, int *browseOut, int *usingSystemDirOut)
{
	WBQtSaveMapDialog dlg(QString::fromLocal8Bit(initialFilename ? initialFilename : ""));
	if (runMapModal(dlg, frameHwnd) == 0)
	{
		return 0;
	}
	QByteArray name = dlg.mapName().toLocal8Bit();
	strncpy(filenameOut, name.constData(), cap - 1);
	filenameOut[cap - 1] = 0;
	if (browseOut != NULL)
	{
		*browseOut = dlg.browseRequested() ? 1 : 0;
	}
	if (usingSystemDirOut != NULL)
	{
		*usingSystemDirOut = dlg.usingSystemDir() ? 1 : 0;
	}
	return 1;
}
