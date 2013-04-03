#include "prefsdialog.hpp"

#include <QSettings>
#include <QGridLayout>
#include <QLabel>
#include <QColorDialog>
#include <QPushButton>
#include <QSlider>
#include <QCheckBox>
#include <QButtonGroup>
#include <QRadioButton>
#include <QFontDialog>
#include <QSpinBox>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <vterm.h>

PrefsDialog::PrefsDialog(QSettings &settings, QWidget *parent) :
	QDialog(parent),
	settings_(settings)
{
	setWindowTitle("Preferences");

	QGridLayout* grid = new QGridLayout(this);
	//== BACKGROUND COLOUR
	grid->addWidget(new QLabel("Background colour:",this),0,0);
	QPalette p;
	bgColourButton = new QPushButton(this);
	connect(bgColourButton, SIGNAL(clicked()), this, SLOT(getBackgroundColour()));
	p.setColor(QPalette::Button, QColor(settings.value("background","black").toString()));
	bgColourButton->setPalette(p);
	bgColourButton->setAutoFillBackground(true);
	bgColourButton->setFlat(true);
	grid->addWidget(bgColourButton, 0, 1);
	//== FOREGROUND COLOUR
	grid->addWidget(new QLabel("Foreground colour:",this),1,0);
	fgColourButton = new QPushButton(this);
	connect(fgColourButton, SIGNAL(clicked()), this, SLOT(getForegroundColour()));
	p.setColor(QPalette::Button, QColor(settings.value("foreground","gray90").toString()));
	fgColourButton->setPalette(p);
	fgColourButton->setAutoFillBackground(true);
	fgColourButton->setFlat(true);
	grid->addWidget(fgColourButton, 1, 1);
	//== FONT
	grid->addWidget(new QLabel("Font:",this),2,0);
	fontButton_ = new QPushButton(this);
	connect(fontButton_, SIGNAL(clicked()), this, SLOT(getFont()));
	QFont f;
	f.setFamily(settings.value("font","monospace").toString());
    f.setPixelSize(settings.value("font_size",9).toInt());
	fontButton_->setFont(f);
    fontButton_->setText(f.family() + " " + QString::number(f.pixelSize()) + "px");
	fontButton_->setFlat(true);
	grid->addWidget(fontButton_, 2, 1);
	//== DOUBLE CLICK FULL WORD
	grid->addWidget(new QLabel("Double-click selects full word:",this),3,0);
	doubleClickFullWord_ = new QCheckBox(this);
	doubleClickFullWord_->setChecked(settings.value("doubleclick_fullword").toBool());
	grid->addWidget(doubleClickFullWord_, 3, 1);
	//== UNSCROLL ON KEY
	grid->addWidget(new QLabel("Scroll to bottom on keystroke:",this),4,0);
	unscrollOnKey_ = new QCheckBox(this);
	unscrollOnKey_->setChecked(settings.value("unscroll_on_key").toBool());
	grid->addWidget(unscrollOnKey_, 4, 1);
	//== UNSCROLL ON OUTPUT
	grid->addWidget(new QLabel("Scroll to bottom on output:",this),5,0);
	unscrollOnOutput_ = new QCheckBox(this);
	unscrollOnOutput_->setChecked(settings.value("unscroll_on_output").toBool());
	grid->addWidget(unscrollOnOutput_, 5, 1);
	//== CURSOR BLINK INTERVAL
	grid->addWidget(new QLabel("Cursor blink interval (ms):",this),6,0);
	QHBoxLayout* cbih = new QHBoxLayout(this);
	QLabel* cbil = new QLabel(this);
	cursorBlinkInterval_ = new QSlider(Qt::Horizontal,this);
	connect(cursorBlinkInterval_, SIGNAL(valueChanged(int)), cbil, SLOT(setNum(int)));
	cursorBlinkInterval_->setRange(100,1000);
	cursorBlinkInterval_->setValue(settings.value("cursor_blink_interval", 500).toInt());
	cbih->addWidget(cursorBlinkInterval_);
	cbih->addWidget(cbil);
	grid->addLayout(cbih, 6,1);
	//== CURSOR SHAPE
	grid->addWidget(new QLabel("Cursor shape:",this),7,0);
	cursorShape_ = new QButtonGroup(this);
	QRadioButton* csb = new QRadioButton("Block", this);
	QRadioButton* csu = new QRadioButton("Underline", this);
	QRadioButton* csl = new QRadioButton("Bar", this);
	cursorShape_->addButton(csb, VTERM_PROP_CURSORSHAPE_BLOCK);
	cursorShape_->addButton(csu, VTERM_PROP_CURSORSHAPE_UNDERLINE);
	cursorShape_->addButton(csl, VTERM_PROP_CURSORSHAPE_BAR_LEFT);
	cursorShape_->button(settings.value("cursor_shape", 1).toInt())->setChecked(true);
	QHBoxLayout* csh = new QHBoxLayout(this);
	csh->addWidget(csb);
	csh->addWidget(csu);
	csh->addWidget(csl);
	grid->addLayout(csh, 7,1);
	//== ENABLE ALT SCREEN
	grid->addWidget(new QLabel("Enable Alt Screen:",this),8,0);
	enableAltScreen_ = new QCheckBox(this);
	enableAltScreen_->setChecked(settings.value("altscreen").toBool());
	grid->addWidget(enableAltScreen_, 8, 1);
	//== BOLD HIGHBRIGHT
	grid->addWidget(new QLabel("Render bold as high brightness:",this),9,0);
	boldHighBright_ = new QCheckBox(this);
	boldHighBright_->setChecked(settings.value("bold_highbright").toBool());
	grid->addWidget(boldHighBright_, 9, 1);
	//== SCROLLBACK
	grid->addWidget(new QLabel("Scrollback buffer lines:",this),10,0);
	scrollBufferSize_ = new QSpinBox(this);
	scrollBufferSize_->setRange(0,100000);
	scrollBufferSize_->setValue(settings.value("scrollback_size").toInt());
	grid->addWidget(scrollBufferSize_, 10, 1);
	//== TERM ENV VAR
	grid->addWidget(new QLabel("TERM environment variable:",this),11,0);
	termEnv_ = new QLineEdit(settings.value("term_env").toString(), this);
	grid->addWidget(termEnv_, 11, 1);
	//== SHELL
	grid->addWidget(new QLabel("Shell:",this),12,0);
	shell_ = new QLineEdit(settings.value("shell").toString(), this);
	grid->addWidget(shell_, 12, 1);
	//== LAUNCH CMD
	grid->addWidget(new QLabel("Launch command:",this),13,0);
	launchCmd_ = new QLineEdit(settings.value("launch_cmd").toString(), this);
	grid->addWidget(launchCmd_, 13, 1);

	//== SHOW MENUBAR
	grid->addWidget(new QLabel("Show Menu Bar:",this),14,0);
	showMenubar_ = new QCheckBox(this);
	showMenubar_->setChecked(settings.value("show_menubar").toBool());
	grid->addWidget(showMenubar_, 14, 1);
	//== ALWAYS SHOW TAB BAR
	grid->addWidget(new QLabel("Always show Tab Bar:",this),15,0);
	alwaysShowTabbar_ = new QCheckBox(this);
	alwaysShowTabbar_->setChecked(settings.value("always_show_tabbar").toBool());
	grid->addWidget(alwaysShowTabbar_, 15, 1);


	QDialogButtonBox* qdbb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
	connect(qdbb, SIGNAL(accepted()), this, SLOT(accept()));
	connect(qdbb, SIGNAL(rejected()), this, SLOT(reject()));
	grid->addWidget(qdbb);
}

void PrefsDialog::getBackgroundColour() {
	QPalette p = bgColourButton->palette();
	p.setColor(QPalette::Button, QColorDialog::getColor(bgColourButton->palette().color(QPalette::Button), this));
	bgColourButton->setPalette(p);
}

void PrefsDialog::getForegroundColour() {
	QPalette p = fgColourButton->palette();
	p.setColor(QPalette::Button, QColorDialog::getColor(fgColourButton->palette().color(QPalette::Button), this));
	fgColourButton->setPalette(p);
}

void PrefsDialog::getFont() {
	QFont f(QFontDialog::getFont(0, fontButton_->font(), this));
	fontButton_->setFont(f);
	fontButton_->setText(f.family() + " " + QString::number(f.pointSize()) + "pt");

}

void PrefsDialog::accept() {
	// save all the selections
	settings_.setValue("background", bgColourButton->palette().color(QPalette::Button));
	settings_.setValue("foreground", fgColourButton->palette().color(QPalette::Button));
	settings_.setValue("font", fontButton_->font().family());
    settings_.setValue("font_size", fontButton_->font().pixelSize());
	settings_.setValue("doubleclick_fullword", doubleClickFullWord_->isChecked());
	settings_.setValue("cursor_shape", cursorShape_->checkedId());
	settings_.setValue("cursor_blink_interval", cursorBlinkInterval_->value());
	settings_.setValue("unscroll_on_key", unscrollOnKey_->isChecked());
	settings_.setValue("unscroll_on_output", unscrollOnOutput_->isChecked());
	settings_.setValue("altscreen", enableAltScreen_->isChecked());
	settings_.setValue("bold_highbright", boldHighBright_->isChecked());
	settings_.setValue("scrollback_size",scrollBufferSize_->value());
	settings_.setValue("term_env", termEnv_->text());
	settings_.setValue("shell", shell_->text());
	settings_.setValue("launch_cmd", launchCmd_->text());

	settings_.setValue("show_menubar", showMenubar_->isChecked());
	settings_.setValue("always_show_tabbar", alwaysShowTabbar_->isChecked());
	done(true);
}
