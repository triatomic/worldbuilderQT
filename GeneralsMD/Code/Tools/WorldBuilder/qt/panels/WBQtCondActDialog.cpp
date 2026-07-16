// WBQtCondActDialog.cpp -- see WBQtCondActDialog.h. Layout and behavior mirror the MFC
// IDD_ScriptCondition / IDD_ScriptAction dialogs: top command row (search / Compress Script /
// OK / Cancel), template tree left, sentence + warnings + developer notes right. The parameter
// links pop the (still MFC) EditParameter modals through the bridge; the sentence and warnings
// re-render when they return.
#include "WBQtCondActDialog.h"
#include "ui_WBQtCondActDialog.h"
#include "WBQtCondActBridge.h"
#include "WBQtTreeStyle.h"

// NewSearch toggle (WBQtObjectBridge.cpp): live-filter search when on.
extern "C" int WBQtConfig_GetNewSearch(void);

#include <QApplication>
#include <QCheckBox>
#include <QEvent>
#include <QGroupBox>
#include <QHash>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QUrl>

namespace
{
	const int kNameCap = 512;
	const int kTextCap = 1024;
	const int kBigCap = 4096;

	// Item data role holding the template index on leaves (-1 on category folders).
	const int kTemplateRole = Qt::UserRole;

	QString templateName(int isAction, int i)
	{
		char buf[kNameCap];
		buf[0] = 0;
		WBQtCondActData_GetTemplateName(isAction, i, buf, sizeof(buf));
		return QString::fromLocal8Bit(buf);
	}

	QString templateName2(int isAction, int i)
	{
		char buf[kNameCap];
		buf[0] = 0;
		WBQtCondActData_GetTemplateName2(isAction, i, buf, sizeof(buf));
		return QString::fromLocal8Bit(buf);
	}

	QString templateHelp(int isAction, int i)
	{
		char buf[kBigCap];
		buf[0] = 0;
		WBQtCondActData_GetTemplateHelp(isAction, i, buf, sizeof(buf));
		// == ParseHelpText: the help strings carry literal "\n" escapes.
		return QString::fromLocal8Bit(buf).replace("\\n", "\n");
	}
}

WBQtCondActDialog::WBQtCondActDialog(void *item, bool isAction, QWidget *parent)
	: QDialog(parent),
	m_ui(new Ui::WBQtCondActDialog),
	m_item(item),
	m_isAction(isAction ? 1 : 0),
	m_updating(false)
{
	// The static widget tree lives in WBQtCondActDialog.ui; bind the members the
	// logic below uses, then wire what Designer can't express.
	m_ui->setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle(isAction ? "Edit Action:" : "Edit Condition:");

	m_searchEdit = m_ui->searchEdit;
	m_compressCheck = m_ui->compressCheck;
	m_tree = m_ui->tree;
	m_sentence = m_ui->sentence;
	m_warningsBox = m_ui->warningsBox;
	m_warningsLabel = m_ui->warningsLabel;
	m_helpLabel = m_ui->helpLabel;

	m_searchEdit->installEventFilter(this);
	WBQtTreeStyle::applyTreeLines(m_tree);

	connect(m_tree, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
			this, SLOT(onCurrentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));
	connect(m_sentence, SIGNAL(anchorClicked(QUrl)), this, SLOT(onLinkClicked(QUrl)));
	connect(m_ui->findBtn, SIGNAL(clicked()), this, SLOT(onSearch()));
	connect(m_ui->resetBtn, SIGNAL(clicked()), this, SLOT(onReset()));
	if (WBQtConfig_GetNewSearch() != 0)
	{
		// NewSearch: filter live as the user types (Find button still works).
		connect(m_searchEdit, SIGNAL(textChanged(QString)), this, SLOT(onSearchLive(QString)));
	}
	connect(m_compressCheck, SIGNAL(toggled(bool)), this, SLOT(onCompressToggled(bool)));
	connect(m_ui->okBtn, SIGNAL(clicked()), this, SLOT(accept()));
	connect(m_ui->cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

	m_updating = true;
	m_compressCheck->setChecked(WBQtCondAct_GetCompress() != 0);
	m_updating = false;
	applyTreeFont();

	populateTree();
	renderSentence();
	showHelpForType(WBQtCondActData_GetType(m_item, m_isAction));
	m_tree->setFocus();

	resize(900, 560);
}

WBQtCondActDialog::~WBQtCondActDialog()
{
	delete m_ui;
}

bool WBQtCondActDialog::eventFilter(QObject *watched, QEvent *event)
{
	// == the MFC OnOK special case: Enter in the search box searches instead of closing.
	if (watched == m_searchEdit && event->type() == QEvent::KeyPress)
	{
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
		if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
		{
			onSearch();
			return true;
		}
	}
	return QDialog::eventFilter(watched, event);
}

void WBQtCondActDialog::populateTree()
{
	m_updating = true;
	m_tree->clear();

	// == the MFC OnInitDialog/OnReset walk: each template's '/'-separated name path builds
	// sorted category folders with the leaf at the end; name2 (if any) adds a second leaf.
	QHash<QString, QTreeWidgetItem *> folders;
	QTreeWidgetItem *selLeaf = NULL;
	int curType = WBQtCondActData_GetType(m_item, m_isAction);
	int count = WBQtCondActData_GetTemplateCount(m_isAction);
	for (int i = 0; i < count; i++)
	{
		for (int pass = 0; pass < 2; pass++)
		{
			QString path = (pass == 0) ? templateName(m_isAction, i) : templateName2(m_isAction, i);
			if (path.isEmpty())
			{
				continue;
			}
			QStringList parts = path.split('/');
			QString leafLabel = parts.takeLast();
			QTreeWidgetItem *parent = NULL;
			QString key;
			for (int p = 0; p < parts.size(); p++)
			{
				key += parts[p];
				key += '/';
				QTreeWidgetItem *folder = folders.value(key, NULL);
				if (folder == NULL)
				{
					if (parent == NULL)
					{
						folder = new QTreeWidgetItem(m_tree, QStringList(parts[p]));
					}
					else
					{
						folder = new QTreeWidgetItem(parent, QStringList(parts[p]));
					}
					folder->setData(0, kTemplateRole, -1);
					folders.insert(key, folder);
				}
				parent = folder;
			}
			QTreeWidgetItem *leaf;
			if (parent == NULL)
			{
				leaf = new QTreeWidgetItem(m_tree, QStringList(leafLabel));
			}
			else
			{
				leaf = new QTreeWidgetItem(parent, QStringList(leafLabel));
			}
			leaf->setData(0, kTemplateRole, i);
			if (pass == 0 && i == curType)
			{
				selLeaf = leaf;
			}
		}
	}
	m_tree->sortItems(0, Qt::AscendingOrder);
	m_updating = false;
	selectCurrentType(selLeaf);
}

void WBQtCondActDialog::selectCurrentType(QTreeWidgetItem *leaf)
{
	if (leaf != NULL)
	{
		m_updating = true;
		m_tree->setCurrentItem(leaf);
		m_tree->scrollToItem(leaf, QAbstractItemView::PositionAtTop);
		m_updating = false;
	}
}

void WBQtCondActDialog::onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
	Q_UNUSED(previous);
	if (m_updating || current == NULL)
	{
		return;
	}
	int type = current->data(0, kTemplateRole).toInt();
	if (type < 0)
	{
		return;
	}
	// == the TVN_SELCHANGED handler: only react when the type actually changes.
	if (type != WBQtCondActData_GetType(m_item, m_isAction))
	{
		WBQtCondActData_SetType(m_item, m_isAction, type);
		renderSentence();
		showHelpForType(type);
	}
}

void WBQtCondActDialog::renderSentence()
{
	// The sentence interleaves uiStrings[0], param[0], uiStrings[1], param[1], ... with each
	// parameter as a link (== the rich edit's blue CFE_LINK ranges).
	int numStrings = WBQtCondActData_GetUiStringCount(m_item, m_isAction);
	int numParams = WBQtCondActData_GetParameterCount(m_item, m_isAction);
	char buf[kTextCap];
	QString html;
	int total = (numStrings > numParams) ? numStrings : numParams;
	for (int i = 0; i < total; i++)
	{
		if (i < numStrings)
		{
			buf[0] = 0;
			WBQtCondActData_GetUiString(m_item, m_isAction, i, buf, sizeof(buf));
			html += QString::fromLocal8Bit(buf).toHtmlEscaped();
		}
		if (i < numParams)
		{
			buf[0] = 0;
			WBQtCondActData_GetParameterText(m_item, m_isAction, i, buf, sizeof(buf));
			QString text = QString::fromLocal8Bit(buf);
			if (text.isEmpty())
			{
				text = "???";
			}
			html += QString("<a href=\"%1\">%2</a>").arg(i).arg(text.toHtmlEscaped());
		}
	}
	m_sentence->setHtml(html);
	updateWarnings();
}

void WBQtCondActDialog::updateWarnings()
{
	// == the formatConditionText/formatActionText warning panel logic (captions from the
	// IDS_SCRIPT_* string table).
	char warnBuf[kBigCap];
	char infoBuf[kBigCap];
	warnBuf[0] = 0;
	infoBuf[0] = 0;
	WBQtCondActData_GetWarnings(m_item, m_isAction, warnBuf, sizeof(warnBuf), infoBuf, sizeof(infoBuf));
	QString warnings = QString::fromLocal8Bit(warnBuf);
	QString information = QString::fromLocal8Bit(infoBuf);
	if (!warnings.isEmpty())
	{
		m_warningsBox->setTitle("Warnings:");
		m_warningsBox->setEnabled(true);
		m_warningsLabel->setText(warnings);
	}
	else if (!information.isEmpty())
	{
		m_warningsBox->setTitle("Information:");
		m_warningsBox->setEnabled(true);
		m_warningsLabel->setText(information);
	}
	else
	{
		m_warningsBox->setTitle("No Warnings");
		m_warningsBox->setEnabled(false);
		m_warningsLabel->setText("");
	}
}

void WBQtCondActDialog::showHelpForType(int type)
{
	if (type >= 0 && type < WBQtCondActData_GetTemplateCount(m_isAction))
	{
		m_helpLabel->setText(templateHelp(m_isAction, type));
	}
	else
	{
		m_helpLabel->setText("");
	}
}

void WBQtCondActDialog::onLinkClicked(const QUrl &url)
{
	bool ok = false;
	int index = url.toString().toInt(&ok);
	if (!ok)
	{
		return;
	}
	// Pops the (still MFC) parameter editor; the sentence and warnings re-render on return.
	WBQtCondAct_EditParameter(m_item, m_isAction, index);
	renderSentence();
}

// NewSearch: filter live as the user types -- empty box restores the full tree, no
// beep and no "No matches" box (both are jarring on every keystroke).
void WBQtCondActDialog::onSearchLive(const QString &text)
{
	if (text.trimmed().isEmpty())
	{
		populateTree();
		return;
	}
	applyFilter(text.toLower(), false);
}

void WBQtCondActDialog::onSearch()
{
	// == the MFC OnSearch: flatten the catalog to full-path leaves matching the search text
	// (in name or name2, case-insensitive).
	QString searchText = m_searchEdit->text().toLower();
	if (searchText.isEmpty())
	{
		QApplication::beep();
		onReset();
		return;
	}
	applyFilter(searchText, true);
}

void WBQtCondActDialog::applyFilter(const QString &searchText, bool announce)
{
	m_updating = true;
	m_tree->clear();
	QTreeWidgetItem *selLeaf = NULL;
	int curType = WBQtCondActData_GetType(m_item, m_isAction);
	int count = WBQtCondActData_GetTemplateCount(m_isAction);
	int matchCount = 0;
	for (int i = 0; i < count; i++)
	{
		QString name = templateName(m_isAction, i);
		QString name2 = templateName2(m_isAction, i);
		if (name.toLower().contains(searchText) || name2.toLower().contains(searchText))
		{
			QTreeWidgetItem *leaf = new QTreeWidgetItem(m_tree, QStringList(name));
			leaf->setData(0, kTemplateRole, i);
			if (i == curType)
			{
				selLeaf = leaf;
				showHelpForType(i);
			}
			matchCount++;
		}
	}
	m_tree->sortItems(0, Qt::AscendingOrder);
	m_updating = false;

	if (matchCount == 0)
	{
		if (announce)
		{
			QMessageBox::information(this, "Search", "No matches found.");
		}
	}
	else
	{
		selectCurrentType(selLeaf);
	}
}

void WBQtCondActDialog::onReset()
{
	m_searchEdit->clear();
	populateTree();
	m_tree->setFocus();
}

void WBQtCondActDialog::onCompressToggled(bool checked)
{
	if (!m_updating)
	{
		WBQtCondAct_SetCompress(checked ? 1 : 0);
	}
	applyTreeFont();
}

void WBQtCondActDialog::applyTreeFont()
{
	// == OnCompress: a tree-density toggle done via font height (14px compressed, 16px not).
	QFont font = m_tree->font();
	font.setPixelSize(m_compressCheck->isChecked() ? 14 : 16);
	m_tree->setFont(font);
}

// ===================== the modal entry point =====================

extern "C" int WBQtCondAct_Run(void *item, int isAction)
{
	if (item == NULL)
	{
		return 0;
	}
	// == EditCondition/EditAction::OnInitDialog clearing the item's warning flag.
	WBQtCondActData_ClearWarningFlag(item, isAction);
	// Parent to the active Qt modal (the script-edit dialog); exec() is application-modal, and
	// the MFC frame is already disabled by the outer WBQtScriptEdit_Run.
	WBQtCondActDialog dlg(item, isAction != 0, QApplication::activeModalWidget());
	int rc = dlg.exec();
	return (rc == QDialog::Accepted) ? 1 : 0;
}
