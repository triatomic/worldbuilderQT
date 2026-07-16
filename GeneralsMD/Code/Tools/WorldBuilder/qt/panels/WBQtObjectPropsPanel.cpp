// WBQtObjectPropsPanel.cpp -- see WBQtObjectPropsPanel.h.
#include "WBQtObjectPropsPanel.h"
#include "ui_WBQtObjectPropsPanel.h"
#include "WBQtObjectPropsBridge.h"
#include "WBQtScrubSpinBox.h"
#include "WBQtShortcuts.h"	// WBQtShortcuts_PostCommand -- post the delete command

#include <QCheckBox>
#include <QApplication>
#include <QAbstractSpinBox>
#include <QEvent>
#include <QComboBox>
#include <QKeyEvent>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

WBQtObjectPropsPanel *WBQtObjectPropsPanel::s_instance = NULL;

WBQtObjectPropsPanel::WBQtObjectPropsPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtObjectPropsPanel),
	  m_soundListBuilt(false),
	  m_updating(false)
{
	// The static widget tree lives in WBQtObjectPropsPanel.ui; bind the members the
	// logic below uses, then wire what Designer can't express.
	m_ui->setupUi(this);
	resize(300, 520);	// == the MFC IDD_MAPOBJECT_PROPS width (200 DLU ~= 300px)

	m_selectionLabel = m_ui->selectionLabel;
	m_generalBox = m_ui->generalBox;
	m_name = m_ui->name;
	m_team = m_ui->team;
	m_script = m_ui->script;
	m_logicalBox = m_ui->logicalBox;
	m_health = m_ui->health;
	m_healthEdit = m_ui->healthEdit;
	m_maxHPs = m_ui->maxHPs;
	m_aggressiveness = m_ui->aggressiveness;
	m_veterancy = m_ui->veterancy;
	m_enabled = m_ui->enabled;
	m_indestructible = m_ui->indestructible;
	m_unsellable = m_ui->unsellable;
	m_targetable = m_ui->targetable;
	m_powered = m_ui->powered;
	m_recruitableAI = m_ui->recruitableAI;
	m_selectable = m_ui->selectable;
	m_distanceBox = m_ui->distanceBox;
	m_stopping = m_ui->stopping;
	m_targeting = m_ui->targeting;
	m_shroud = m_ui->shroud;
	m_visualBox = m_ui->visualBox;
	m_xyPos = m_ui->xyPos;
	m_zOffset = m_ui->zOffset;
	m_weather = m_ui->weather;
	m_angle = m_ui->angle;
	m_time = m_ui->time;
	m_soundBox = m_ui->soundBox;
	m_sound = m_ui->sound;
	m_listen = m_ui->listen;
	m_customize = m_ui->customize;
	m_soundEnabled = m_ui->soundEnabled;
	m_looping = m_ui->looping;
	m_loopCount = m_ui->loopCount;
	m_priority = m_ui->priority;
	m_volume = m_ui->volume;
	m_minVolume = m_ui->minVolume;
	m_minRange = m_ui->minRange;
	m_maxRange = m_ui->maxRange;
	m_upgradesBox = m_ui->upgradesBox;
	m_upgrades = m_ui->upgrades;

	// Don't let a long team name dictate the panel width (== the MFC combo, which stays
	// fixed and elides). The dropdown popup can still be wide.
	m_team->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	m_team->setMinimumContentsLength(12);

	// Script row: disabled, matching the MFC dialog (the script attachment is inert in this build).
	m_ui->scriptLabel->setEnabled(false);
	m_script->setEnabled(false);

	m_zOffset->setAxisVertical(true);	// promotion can't pass the ctor arg
	m_zOffset->setRange(-100.0, 100.0);	// matches the MFC IDC_HEIGHT_POPUP slider range
	m_angle->setRange(0.0, 360.0);	// matches the MFC IDC_ANGLE_POPUP slider range
	m_angle->setWrapping(true);	// 360 wraps to 0, like a compass heading

	// The sound list holds hundreds of long names; without this it forces the whole panel
	// far wider than the MFC dialog. Cap the field width; the popup can still be wide.
	m_sound->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	m_sound->setMinimumContentsLength(12);
	m_sound->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	// Seed from the current selection under the guard.
	pushRefresh();

	connect(m_name, SIGNAL(editingFinished()), this, SLOT(onNameChanged()));
	connect(m_team, SIGNAL(currentIndexChanged(int)), this, SLOT(onTeamChanged(int)));
	connect(m_health, SIGNAL(currentIndexChanged(int)), this, SLOT(onHealthChanged(int)));
	connect(m_healthEdit, SIGNAL(editingFinished()), this, SLOT(onHealthEditChanged()));
	connect(m_maxHPs, SIGNAL(editTextChanged(const QString &)), this, SLOT(onMaxHPsChanged()));
	connect(m_aggressiveness, SIGNAL(currentIndexChanged(int)), this, SLOT(onAggressivenessChanged(int)));
	connect(m_veterancy, SIGNAL(currentIndexChanged(int)), this, SLOT(onVeterancyChanged(int)));
	connect(m_enabled, SIGNAL(clicked()), this, SLOT(onFlagToggled()));
	connect(m_indestructible, SIGNAL(clicked()), this, SLOT(onFlagToggled()));
	connect(m_unsellable, SIGNAL(clicked()), this, SLOT(onFlagToggled()));
	connect(m_targetable, SIGNAL(clicked()), this, SLOT(onFlagToggled()));
	connect(m_powered, SIGNAL(clicked()), this, SLOT(onFlagToggled()));
	connect(m_recruitableAI, SIGNAL(clicked()), this, SLOT(onFlagToggled()));
	connect(m_selectable, SIGNAL(clicked()), this, SLOT(onFlagToggled()));
	connect(m_targeting, SIGNAL(editingFinished()), this, SLOT(onTargetingChanged()));
	connect(m_shroud, SIGNAL(editingFinished()), this, SLOT(onShroudChanged()));
	connect(m_stopping, SIGNAL(editingFinished()), this, SLOT(onStoppingChanged()));
	connect(m_weather, SIGNAL(currentIndexChanged(int)), this, SLOT(onWeatherChanged(int)));
	connect(m_time, SIGNAL(currentIndexChanged(int)), this, SLOT(onTimeChanged(int)));
	connect(m_xyPos, SIGNAL(editingFinished()), this, SLOT(onPositionChanged()));
	connect(m_zOffset, SIGNAL(valueChanged(double)), this, SLOT(onZChanged()));
	connect(m_angle, SIGNAL(valueChanged(double)), this, SLOT(onAngleChanged()));
	// Batch a whole scrub drag into one undoable (== the MFC pop-slider behavior).
	connect(m_zOffset, SIGNAL(scrubStarted()), this, SLOT(onPosScrubStarted()));
	connect(m_zOffset, SIGNAL(scrubFinished()), this, SLOT(onPosScrubFinished()));
	connect(m_angle, SIGNAL(scrubStarted()), this, SLOT(onPosScrubStarted()));
	connect(m_angle, SIGNAL(scrubFinished()), this, SLOT(onPosScrubFinished()));
	connect(m_sound, SIGNAL(currentIndexChanged(int)), this, SLOT(onSoundChanged(int)));
	connect(m_listen, SIGNAL(clicked()), this, SLOT(onListenClicked()));
	connect(m_customize, SIGNAL(clicked()), this, SLOT(onSoundFlagToggled()));
	connect(m_soundEnabled, SIGNAL(clicked()), this, SLOT(onSoundFlagToggled()));
	connect(m_looping, SIGNAL(clicked()), this, SLOT(onSoundFlagToggled()));
	connect(m_loopCount, SIGNAL(editingFinished()), this, SLOT(onLoopCountChanged()));
	connect(m_priority, SIGNAL(currentIndexChanged(int)), this, SLOT(onPriorityChanged(int)));
	connect(m_volume, SIGNAL(editingFinished()), this, SLOT(onVolumeChanged()));
	connect(m_minVolume, SIGNAL(editingFinished()), this, SLOT(onMinVolumeChanged()));
	connect(m_minRange, SIGNAL(editingFinished()), this, SLOT(onMinRangeChanged()));
	connect(m_maxRange, SIGNAL(editingFinished()), this, SLOT(onMaxRangeChanged()));
	connect(m_upgrades, SIGNAL(itemSelectionChanged()), this, SLOT(onUpgradeSelectionChanged()));

	// App-level filter so a bare Delete/Backspace anywhere in this panel deletes the object
	// (see eventFilter). qApp outlives the panel; the panel is process-lived (one instance).
	qApp->installEventFilter(this);

	s_instance = this;
}

WBQtObjectPropsPanel::~WBQtObjectPropsPanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

void WBQtObjectPropsPanel::rebuildTeams()
{
	// Caller has set m_updating (repopulation must not fire onTeamChanged).
	m_team->clear();
	const int cap = 256;
	char buf[cap];
	int count = WBQtObjectProps_GetTeamCount();
	for (int i = 0; i < count; ++i)
	{
		if (WBQtObjectProps_GetTeamName(i, buf, cap))
		{
			m_team->addItem(QString::fromLatin1(buf));
		}
		else
		{
			m_team->addItem(QString());
		}
	}
}

// ID_EDIT_DELETE (resource.h) -- the MFC view command that deletes the selected object(s).
#define WBID_EDIT_DELETE 32931

// App-level filter: a bare Delete/Backspace pressed anywhere INSIDE this panel deletes the
// selected map object. Installed on qApp (in the ctor) so it sees the key before the focused
// child widget (a combo / list / checkbox) can consume it -- a plain keyPressEvent override on
// the panel never fires because those children handle the key themselves. Gated so it only acts
// when the focus is a descendant of this panel and not a text-editing widget.
bool WBQtObjectPropsPanel::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::KeyPress)
	{
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
		if ((keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace)
			&& keyEvent->modifiers() == Qt::NoModifier)
		{
			QWidget *f = QApplication::focusWidget();
			// Only when the focus is this panel or one of its children.
			if (f != NULL && (f == this || isAncestorOf(f)))
			{
				// A line edit / editable combo / spin box needs Delete/Backspace to edit text.
				bool textField = qobject_cast<QLineEdit *>(f) != NULL
					|| qobject_cast<QAbstractSpinBox *>(f) != NULL;
				QComboBox *combo = qobject_cast<QComboBox *>(f);
				if (combo != NULL && combo->isEditable())
				{
					textField = true;
				}
				if (!textField)
				{
					// Route to the MFC view's OnEditDelete like a menu click (unchecked: the
					// gated post drops ID_EDIT_DELETE because its update probe reports disabled).
					WBQtShortcuts_PostCommandUnchecked(WBID_EDIT_DELETE);
					return true;	// consume it
				}
			}
		}
	}
	return QWidget::eventFilter(watched, event);
}

void WBQtObjectPropsPanel::pushRefresh()
{
	m_updating = true;

	int selCount = WBQtObjectProps_GetSelCount();
	bool single = (WBQtObjectProps_HasSelection() != 0);

	const int cap = 256;
	char buf[cap];

	// Selection summary + name field. The name is single-select only (matches the MFC panel).
	if (selCount == 0)
	{
		m_selectionLabel->setText("No Selection");
	}
	else if (selCount > 1)
	{
		m_selectionLabel->setText(QString("%1 objects selected").arg(selCount));
	}
	else if (WBQtObjectProps_GetName(buf, cap) && buf[0] != 0)
	{
		m_selectionLabel->setText(QString::fromLatin1(buf));
	}
	else
	{
		m_selectionLabel->setText("1 object selected");
	}

	if (single && WBQtObjectProps_GetName(buf, cap))
	{
		m_name->setText(QString::fromLatin1(buf));
	}
	else
	{
		m_name->clear();
	}
	m_name->setEnabled(single);

	// Team combo: always available for any selection (applies to all selected objects).
	rebuildTeams();
	int curTeam = WBQtObjectProps_GetCurTeam();
	if (curTeam >= 0 && curTeam < m_team->count())
	{
		m_team->setCurrentIndex(curTeam);
	}
	m_team->setEnabled(selCount > 0);
	m_generalBox->setEnabled(selCount > 0);

	// --- Logical section ---------------------------------------------------------------------
	// Health: pick the matching preset or fall to "Other" + the explicit value.
	int health = WBQtObjectProps_GetHealthPercent();
	int healthIndex = -1;
	if (health == 0)        { healthIndex = 0; }
	else if (health == 25)  { healthIndex = 1; }
	else if (health == 50)  { healthIndex = 2; }
	else if (health == 75)  { healthIndex = 3; }
	else if (health == 100) { healthIndex = 4; }
	if (healthIndex >= 0)
	{
		m_health->setCurrentIndex(healthIndex);
		m_healthEdit->clear();
		m_healthEdit->setEnabled(false);
	}
	else
	{
		m_health->setCurrentIndex(5);	// Other
		m_healthEdit->setText(QString::number(health));
		m_healthEdit->setEnabled(true);
	}

	// Max hit points: rebuild so the explicit value is the second entry (like the MFC combo).
	m_maxHPs->clear();
	m_maxHPs->addItem("Default For Unit");
	int hps = WBQtObjectProps_GetMaxHPs();
	if (hps > 0)
	{
		m_maxHPs->addItem(QString::number(hps));
		m_maxHPs->setCurrentIndex(1);
	}
	else
	{
		m_maxHPs->setCurrentIndex(0);
	}

	// Aggressiveness value (-2..2) maps to combo index 0..4.
	int agg = WBQtObjectProps_GetAggressiveness();
	int aggIndex = agg + 2;
	if (aggIndex >= 0 && aggIndex < m_aggressiveness->count())
	{
		m_aggressiveness->setCurrentIndex(aggIndex);
	}

	int vet = WBQtObjectProps_GetVeterancy();
	if (vet >= 0 && vet < m_veterancy->count())
	{
		m_veterancy->setCurrentIndex(vet);
	}

	m_enabled->setChecked(WBQtObjectProps_GetFlag(WBQT_OBJPROP_FLAG_ENABLED) != 0);
	m_indestructible->setChecked(WBQtObjectProps_GetFlag(WBQT_OBJPROP_FLAG_INDESTRUCTIBLE) != 0);
	m_unsellable->setChecked(WBQtObjectProps_GetFlag(WBQT_OBJPROP_FLAG_UNSELLABLE) != 0);
	m_targetable->setChecked(WBQtObjectProps_GetFlag(WBQT_OBJPROP_FLAG_TARGETABLE) != 0);
	m_powered->setChecked(WBQtObjectProps_GetFlag(WBQT_OBJPROP_FLAG_POWERED) != 0);
	m_recruitableAI->setChecked(WBQtObjectProps_GetFlag(WBQT_OBJPROP_FLAG_RECRUITABLEAI) != 0);
	// Selectable: 2 == the "default" (key absent) state, and objects default to selectable,
	// so show it checked. Two-state on the Qt side -- the tri-state 'default' wrote an empty
	// dict through the bridge (_SelectableToDict's remove-the-key path) and hit the
	// unknown-dict-key assert; a toggle now always writes an explicit bool.
	int sel = WBQtObjectProps_GetFlag(WBQT_OBJPROP_FLAG_SELECTABLE);
	m_selectable->setChecked(sel != 0);

	// Distance box. "Targeting" is the vision/visual range; blank when unset (0), like the MFC edits.
	int targeting = WBQtObjectProps_GetVisionDistance();
	m_targeting->setText(targeting > 0 ? QString::number(targeting) : QString());
	int shroud = WBQtObjectProps_GetShroudClearingDistance();
	m_shroud->setText(shroud > 0 ? QString::number(shroud) : QString());
	m_stopping->setText(QString::number(WBQtObjectProps_GetStoppingDistance(), 'g'));

	m_logicalBox->setEnabled(selCount > 0);
	m_distanceBox->setEnabled(selCount > 0);

	// --- Visual section ----------------------------------------------------------------------
	int weather = WBQtObjectProps_GetWeather();
	if (weather >= 0 && weather < m_weather->count())
	{
		m_weather->setCurrentIndex(weather);
	}
	int timeOfDay = WBQtObjectProps_GetTime();
	if (timeOfDay >= 0 && timeOfDay < m_time->count())
	{
		m_time->setCurrentIndex(timeOfDay);
	}

	const int posCap = 64;
	char posBuf[posCap];
	if (single && WBQtObjectProps_GetPosition(posBuf, posCap))
	{
		m_xyPos->setText(QString::fromLatin1(posBuf));
	}
	else
	{
		m_xyPos->clear();
	}
	if (single)
	{
		m_zOffset->setValue(WBQtObjectProps_GetZOffset());
		m_angle->setValue(WBQtObjectProps_GetAngle());
	}
	else
	{
		m_zOffset->setValue(0.0);
		m_angle->setValue(0.0);
	}

	// Weather/Time apply to the whole selection; XY/Z/Angle are single-object (like the MFC edits).
	m_visualBox->setEnabled(selCount > 0);
	m_xyPos->setEnabled(single);
	m_zOffset->setEnabled(single);
	m_angle->setEnabled(single);

	// --- Sound section -----------------------------------------------------------------------
	rebuildSoundList();	// enumerates once, then mirrors the current selection each time
	int soundSel = WBQtObjectProps_GetSoundCurSel();
	if (soundSel >= 0 && soundSel < m_sound->count())
	{
		m_sound->setCurrentIndex(soundSel);
	}

	// Priority combo (fixed list; fill once).
	if (m_priority->count() == 0)
	{
		const int cap = 128;
		char pbuf[cap];
		int pcount = WBQtObjectProps_GetSoundPriorityCount();
		for (int i = 0; i < pcount; ++i)
		{
			if (WBQtObjectProps_GetSoundPriorityName(i, pbuf, cap))
			{
				m_priority->addItem(QString::fromLatin1(pbuf));
			}
		}
	}

	// The MFC handlers set each control's value AND its gated enabled state; mirror both.
	m_customize->setChecked(WBQtObjectProps_GetSoundFlag(WBQT_SND_CUSTOMIZE) != 0);
	m_customize->setEnabled(WBQtObjectProps_GetSoundFlagEnabled(WBQT_SND_CUSTOMIZE) != 0);
	m_soundEnabled->setChecked(WBQtObjectProps_GetSoundFlag(WBQT_SND_ENABLED) != 0);
	m_soundEnabled->setEnabled(WBQtObjectProps_GetSoundFlagEnabled(WBQT_SND_ENABLED) != 0);
	m_looping->setChecked(WBQtObjectProps_GetSoundFlag(WBQT_SND_LOOPING) != 0);
	m_looping->setEnabled(WBQtObjectProps_GetSoundFlagEnabled(WBQT_SND_LOOPING) != 0);

	int en = 0;
	int lc = WBQtObjectProps_GetSoundInt(WBQT_SNDINT_LOOPCOUNT, &en);
	m_loopCount->setText(QString::number(lc));
	m_loopCount->setEnabled(en != 0);
	int vol = WBQtObjectProps_GetSoundInt(WBQT_SNDINT_VOLUME, &en);
	m_volume->setText(QString::number(vol));
	m_volume->setEnabled(en != 0);
	int minVol = WBQtObjectProps_GetSoundInt(WBQT_SNDINT_MINVOLUME, &en);
	m_minVolume->setText(QString::number(minVol));
	m_minVolume->setEnabled(en != 0);
	int minRng = WBQtObjectProps_GetSoundInt(WBQT_SNDINT_MINRANGE, &en);
	m_minRange->setText(QString::number(minRng));
	m_minRange->setEnabled(en != 0);
	int maxRng = WBQtObjectProps_GetSoundInt(WBQT_SNDINT_MAXRANGE, &en);
	m_maxRange->setText(QString::number(maxRng));
	m_maxRange->setEnabled(en != 0);

	int prioEnabled = 0;
	int prio = WBQtObjectProps_GetSoundPriority(&prioEnabled);
	if (prio >= 0 && prio < m_priority->count())
	{
		m_priority->setCurrentIndex(prio);
	}
	m_priority->setEnabled(prioEnabled != 0);

	m_listen->setText(WBQtObjectProps_GetSoundPlaying() ? "Stop" : "Listen");
	m_soundBox->setEnabled(selCount > 0);

	// --- Pre-built upgrades ------------------------------------------------------------------
	// Rebuild each refresh (the available upgrades vary per object; it's a short list unlike the
	// sound combo). The MFC listbox has already been filled + pre-selected by _DictToPrebuiltUpgrades.
	m_upgrades->clear();
	int upCount = WBQtObjectProps_GetUpgradeCount();
	const int upCap = 256;
	char upBuf[upCap];
	for (int i = 0; i < upCount; ++i)
	{
		if (WBQtObjectProps_GetUpgradeItem(i, upBuf, upCap))
		{
			QListWidgetItem *item = new QListWidgetItem(QString::fromLatin1(upBuf), m_upgrades);
			item->setSelected(WBQtObjectProps_GetUpgradeSelected(i) != 0);
		}
	}
	// Single-object only: a multi-select shows the "Single Selection Only" placeholder, so disable
	// interaction when more than one object is selected.
	m_upgrades->setEnabled(single);
	m_upgradesBox->setEnabled(selCount > 0);

	m_updating = false;
}

void WBQtObjectPropsPanel::rebuildSoundList()
{
	// The attached-sound combo holds thousands of audio events, so enumerate the full list only
	// ONCE (the first refresh with a live MFC list); re-walking it per click is what caused the
	// MFC ~200ms select/deselect latency. Item 0 is the "Default <...>" entry whose text changes
	// per selection, so we refresh just that one entry on every call.
	int count = WBQtObjectProps_GetSoundCount();
	if (count <= 0)
	{
		return;	// no live list yet (no selection); try again on a later refresh
	}
	const int cap = 256;
	char buf[cap];

	if (!m_soundListBuilt)
	{
		// Build the whole item list first, then addItems() ONCE. Thousands of individual
		// addItem() calls each recompute the combo's size hint / layout -- that per-item
		// churn blocked the UI thread long enough on first open to show a white panel
		// before the first paint. A single addItems() adds them in one shot.
		QStringList items;
		items.reserve(count);
		for (int i = 0; i < count; ++i)
		{
			if (WBQtObjectProps_GetSoundItem(i, buf, cap))
			{
				items.append(QString::fromLatin1(buf));
			}
			else
			{
				items.append(QString());
			}
		}
		m_sound->clear();
		m_sound->addItems(items);
		m_soundListBuilt = true;
	}
	else if (m_sound->count() > 0 && WBQtObjectProps_GetSoundItem(0, buf, cap))
	{
		// Keep the Default entry's label current (it embeds the object's own ambient sound).
		m_sound->setItemText(0, QString::fromLatin1(buf));
	}
}

void WBQtObjectPropsPanel::onNameChanged()
{
	if (m_updating)
	{
		return;
	}
	WBQtObjectProps_SetName(m_name->text().toLatin1().constData());
}

void WBQtObjectPropsPanel::onTeamChanged(int index)
{
	if (m_updating)
	{
		return;
	}
	if (index >= 0)
	{
		WBQtObjectProps_SetTeam(index);
	}
}

// --- Logical section slots ------------------------------------------------------------------

void WBQtObjectPropsPanel::onHealthChanged(int index)
{
	if (m_updating)
	{
		return;
	}
	// Index 5 == "Other": enable the edit box and let the user type a value; do NOT write
	// anything yet. Writing a placeholder (e.g. 100) here would round-trip through the dict
	// back to the matching "100%" preset and bounce the combo straight off "Other",
	// re-disabling the box. This mirrors the MFC _HealthToDict, which enables the edit and
	// early-returns while it is empty. Only an existing typed value is committed.
	if (index == 5)
	{
		m_updating = true;
		m_healthEdit->setEnabled(true);
		m_updating = false;
		m_healthEdit->setFocus();

		bool ok = false;
		int typed = m_healthEdit->text().toInt(&ok);
		if (ok)
		{
			WBQtObjectProps_SetHealthPercent(typed);
		}
		return;
	}

	// Index 0..4 == 0/25/50/75/100% presets.
	int value = 100;
	switch (index)
	{
		case 0: value = 0;   break;
		case 1: value = 25;  break;
		case 2: value = 50;  break;
		case 3: value = 75;  break;
		case 4: value = 100; break;
		default: value = 100; break;
	}
	m_updating = true;
	m_healthEdit->setEnabled(false);
	m_healthEdit->clear();
	m_updating = false;
	WBQtObjectProps_SetHealthPercent(value);
}

void WBQtObjectPropsPanel::onHealthEditChanged()
{
	if (m_updating)
	{
		return;
	}
	// Only meaningful while "Other" is selected.
	if (m_health->currentIndex() != 5)
	{
		return;
	}
	bool ok = false;
	int typed = m_healthEdit->text().toInt(&ok);
	if (ok)
	{
		WBQtObjectProps_SetHealthPercent(typed);
	}
}

void WBQtObjectPropsPanel::onMaxHPsChanged()
{
	if (m_updating)
	{
		return;
	}
	QString text = m_maxHPs->currentText();
	if (text == "Default For Unit")
	{
		WBQtObjectProps_SetMaxHPs(-1);
		return;
	}
	bool ok = false;
	int hps = text.toInt(&ok);
	WBQtObjectProps_SetMaxHPs(ok ? hps : -1);
}

void WBQtObjectPropsPanel::onAggressivenessChanged(int index)
{
	if (m_updating)
	{
		return;
	}
	if (index >= 0)
	{
		// Combo index 0..4 -> value -2..2.
		WBQtObjectProps_SetAggressiveness(index - 2);
	}
}

void WBQtObjectPropsPanel::onVeterancyChanged(int index)
{
	if (m_updating)
	{
		return;
	}
	if (index >= 0)
	{
		WBQtObjectProps_SetVeterancy(index);
	}
}

void WBQtObjectPropsPanel::applyFlag(int flagId, QCheckBox *box)
{
	int state = 0;
	if (box->checkState() == Qt::Checked)
	{
		state = 1;
	}
	WBQtObjectProps_SetFlag(flagId, state);
}

void WBQtObjectPropsPanel::onFlagToggled()
{
	if (m_updating)
	{
		return;
	}
	QObject *src = sender();
	if (src == m_enabled)             { applyFlag(WBQT_OBJPROP_FLAG_ENABLED, m_enabled); }
	else if (src == m_indestructible) { applyFlag(WBQT_OBJPROP_FLAG_INDESTRUCTIBLE, m_indestructible); }
	else if (src == m_unsellable)     { applyFlag(WBQT_OBJPROP_FLAG_UNSELLABLE, m_unsellable); }
	else if (src == m_targetable)     { applyFlag(WBQT_OBJPROP_FLAG_TARGETABLE, m_targetable); }
	else if (src == m_powered)        { applyFlag(WBQT_OBJPROP_FLAG_POWERED, m_powered); }
	else if (src == m_recruitableAI)  { applyFlag(WBQT_OBJPROP_FLAG_RECRUITABLEAI, m_recruitableAI); }
	else if (src == m_selectable)     { applyFlag(WBQT_OBJPROP_FLAG_SELECTABLE, m_selectable); }
}

void WBQtObjectPropsPanel::onTargetingChanged()
{
	if (m_updating)
	{
		return;
	}
	// "Targeting" edits the object's vision/visual range. Blank == unset (0).
	bool ok = false;
	int v = m_targeting->text().toInt(&ok);
	WBQtObjectProps_SetVisionDistance(ok ? v : 0);
}

void WBQtObjectPropsPanel::onShroudChanged()
{
	if (m_updating)
	{
		return;
	}
	bool ok = false;
	int v = m_shroud->text().toInt(&ok);
	WBQtObjectProps_SetShroudClearingDistance(ok ? v : 0);
}

void WBQtObjectPropsPanel::onStoppingChanged()
{
	if (m_updating)
	{
		return;
	}
	bool ok = false;
	double v = m_stopping->text().toDouble(&ok);
	if (ok)
	{
		WBQtObjectProps_SetStoppingDistance(v);
	}
}

// --- Visual section slots -------------------------------------------------------------------

void WBQtObjectPropsPanel::onWeatherChanged(int index)
{
	if (m_updating)
	{
		return;
	}
	if (index >= 0)
	{
		WBQtObjectProps_SetWeather(index);
	}
}

void WBQtObjectPropsPanel::onTimeChanged(int index)
{
	if (m_updating)
	{
		return;
	}
	if (index >= 0)
	{
		WBQtObjectProps_SetTime(index);
	}
}

void WBQtObjectPropsPanel::onPositionChanged()
{
	if (m_updating)
	{
		return;
	}
	WBQtObjectProps_SetPosition(m_xyPos->text().toLatin1().constData());
}

void WBQtObjectPropsPanel::onZChanged()
{
	if (m_updating)
	{
		return;
	}
	WBQtObjectProps_SetZOffset(m_zOffset->value());
}

void WBQtObjectPropsPanel::onPosScrubStarted()
{
	WBQtObjectProps_BeginPosScrub();
}

void WBQtObjectPropsPanel::onPosScrubFinished()
{
	WBQtObjectProps_EndPosScrub();
}

void WBQtObjectPropsPanel::onAngleChanged()
{
	if (m_updating)
	{
		return;
	}
	WBQtObjectProps_SetAngle(m_angle->value());
}

// --- Sound section slots --------------------------------------------------------------------

void WBQtObjectPropsPanel::onSoundChanged(int index)
{
	if (m_updating)
	{
		return;
	}
	if (index >= 0)
	{
		WBQtObjectProps_SetSoundCurSel(index);
	}
}

void WBQtObjectPropsPanel::onListenClicked()
{
	WBQtObjectProps_ToggleSoundPreview();
	m_listen->setText(WBQtObjectProps_GetSoundPlaying() ? "Stop" : "Listen");
}

void WBQtObjectPropsPanel::onSoundFlagToggled()
{
	if (m_updating)
	{
		return;
	}
	QObject *src = sender();
	if (src == m_customize)
	{
		WBQtObjectProps_SetSoundFlag(WBQT_SND_CUSTOMIZE, m_customize->isChecked() ? 1 : 0);
	}
	else if (src == m_soundEnabled)
	{
		WBQtObjectProps_SetSoundFlag(WBQT_SND_ENABLED, m_soundEnabled->isChecked() ? 1 : 0);
	}
	else if (src == m_looping)
	{
		WBQtObjectProps_SetSoundFlag(WBQT_SND_LOOPING, m_looping->isChecked() ? 1 : 0);
	}
}

void WBQtObjectPropsPanel::onLoopCountChanged()
{
	if (m_updating)
	{
		return;
	}
	WBQtObjectProps_SetSoundInt(WBQT_SNDINT_LOOPCOUNT, m_loopCount->text().toInt());
}

void WBQtObjectPropsPanel::onPriorityChanged(int index)
{
	if (m_updating)
	{
		return;
	}
	if (index >= 0)
	{
		WBQtObjectProps_SetSoundPriority(index);
	}
}

void WBQtObjectPropsPanel::onVolumeChanged()
{
	if (m_updating)
	{
		return;
	}
	WBQtObjectProps_SetSoundInt(WBQT_SNDINT_VOLUME, m_volume->text().toInt());
}

void WBQtObjectPropsPanel::onMinVolumeChanged()
{
	if (m_updating)
	{
		return;
	}
	WBQtObjectProps_SetSoundInt(WBQT_SNDINT_MINVOLUME, m_minVolume->text().toInt());
}

void WBQtObjectPropsPanel::onMinRangeChanged()
{
	if (m_updating)
	{
		return;
	}
	WBQtObjectProps_SetSoundInt(WBQT_SNDINT_MINRANGE, m_minRange->text().toInt());
}

void WBQtObjectPropsPanel::onMaxRangeChanged()
{
	if (m_updating)
	{
		return;
	}
	WBQtObjectProps_SetSoundInt(WBQT_SNDINT_MAXRANGE, m_maxRange->text().toInt());
}

// --- Pre-built upgrades slot ----------------------------------------------------------------

void WBQtObjectPropsPanel::onUpgradeSelectionChanged()
{
	if (m_updating)
	{
		return;
	}
	// Push the whole Qt selection state to the MFC listbox, then commit once so a multi-item
	// change is a single undoable (and _PrebuiltUpgradesToDict rebuilds the grant keys from it).
	int count = m_upgrades->count();
	for (int i = 0; i < count; ++i)
	{
		WBQtObjectProps_SetUpgradeSelected(i, m_upgrades->item(i)->isSelected() ? 1 : 0);
	}
	WBQtObjectProps_CommitUpgrades();
}

// --- Forward push (MapObjectProps refresh -> widget), Qt side of WBQtObjectPropsBridge.h -----
extern "C" void WBQtObjectProps_PushRefresh(void)
{
	if (WBQtObjectPropsPanel::instance() != NULL)
	{
		WBQtObjectPropsPanel::instance()->pushRefresh();
	}
}
