#include "quilte.hpp"
#include "searchpanel.hpp"
#include "prefsdialog.hpp"
#include <QTabWidget>
#include <QMenuBar>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QVTermWidget>
#include <QDebug>
#include <QFileDialog>
#include <QSettings>
#include <QTabBar>
#include <QContextMenuEvent>

Quilte::Quilte(QSettings& settings) : QMainWindow(),
	settings_(settings){
	tabs_ = new QTabWidget_();
	tabs_->setStyleSheet("QTabWidget::pane{border:0;}");
	menu_ = new QMenuBar();
	QIcon::setThemeName("gnome");

	//// FILE MENU
	QMenu* fileMenu = menu_->addMenu("File");
	QAction* newTab = fileMenu->addAction("New Tab", this, SLOT(newTab()));
	addAction(newTab);
	newTab->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_T));
	newTab->setIcon(QIcon::fromTheme("window-new"));
	QAction* saveBuffer = fileMenu->addAction("Save Buffer", this, SLOT(saveBuffer()));
	addAction(saveBuffer);
	saveBuffer->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_S));
	saveBuffer->setIcon(QIcon::fromTheme("document-save"));
	QAction* closeTab = fileMenu->addAction("Close Tab", this, SLOT(closeCurrentTab()));
	addAction(closeTab);
	closeTab->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_W));
	closeTab->setIcon(QIcon::fromTheme("window-close"));
	QAction* closeWindow = fileMenu->addAction("Close Window", this, SLOT(close()));
	addAction(closeWindow);
	closeWindow->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Q));
	closeWindow->setIcon(QIcon::fromTheme("window-close"));
	//// EDIT MENU
	QMenu* editMenu = menu_->addMenu("Edit");
	QAction* copy = editMenu->addAction("Copy", this, SLOT(editCopy()));
	copy->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_C));
	copy->setIcon(QIcon::fromTheme("edit-copy"));
	QAction* paste = editMenu->addAction("Paste", this, SLOT(editPaste()));
	paste->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_V));
	paste->setIcon(QIcon::fromTheme("edit-paste"));
	QAction* clear = editMenu->addAction("Clear", this, SLOT(editClear()));
	clear->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_L));
	clear->setIcon(QIcon::fromTheme("edit-clear"));
	editMenu->addSeparator();
	QAction* prefs = editMenu->addAction("Preferences", this, SLOT(showPrefs()));
	prefs->setIcon(QIcon::fromTheme("document-properties"));
	//// VIEW MENU
	QMenu* viewMenu = menu_->addMenu("View");
	QAction* toggleMenubar = viewMenu->addAction("Toggle Menubar", this, SLOT(toggleMenubar()));
	addAction(toggleMenubar);
	toggleMenubar->setShortcut(QKeySequence(Qt::Key_F10));
	QAction* fullScreen = viewMenu->addAction("Full Screen", this, SLOT(toggleFullScreen()));
	addAction(fullScreen);
	fullScreen->setShortcut(QKeySequence(Qt::Key_F11));
	fullScreen->setIcon(QIcon::fromTheme("view-fullscreen"));
	viewMenu->addSeparator();
	QAction* nextTab = viewMenu->addAction("Next Tab", this, SLOT(nextTab()));
	addAction(nextTab);
	nextTab->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_PageDown));
	nextTab->setIcon(QIcon::fromTheme("go-next"));
	QAction* prevTab = viewMenu->addAction("Previous Tab", this, SLOT(prevTab()));
	addAction(prevTab);
	prevTab->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_PageUp));
	prevTab->setIcon(QIcon::fromTheme("go-previous"));
	//// SEARCH MENU
	QMenu* searchMenu = menu_->addMenu("Search");
	QAction* find = searchMenu->addAction("Find...", this, SLOT(find()));
	find->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_F));
	addAction(find);
	find->setIcon(QIcon::fromTheme("edit-find"));
	QAction* findNext = searchMenu->addAction("Find Next", this, SLOT(searchNext()));
	addAction(findNext);
	findNext->setShortcut(QKeySequence(Qt::Key_F3));
	findNext->setIcon(QIcon::fromTheme("edit-find"));
	//// HELP MENU
	QMenu* helpMenu = menu_->addMenu("Help");
	QAction* website = helpMenu->addAction("Website", this, SLOT(website()));
	addAction(website);
	website->setShortcut(QKeySequence(Qt::Key_F1));
	website->setIcon(QIcon::fromTheme("help-contents"));
	QAction* about = helpMenu->addAction("About quilte", this, SLOT(about()));
	about->setIcon(QIcon::fromTheme("help-about"));

	setMenuBar(menu_);
	contextMenu_ = new QMenu(this);
	contextMenu_->addMenu(fileMenu);
	contextMenu_->addMenu(editMenu);
	contextMenu_->addMenu(viewMenu);
	contextMenu_->addMenu(searchMenu);
	contextMenu_->addMenu(helpMenu);

	setCentralWidget(tabs_);
	menu_->setVisible(settings.value("show_menubar").toBool());

	searchPanel_ = new SearchPanel(this);
	searchPanel_->hide();
	connect(searchPanel_, SIGNAL(visibilityChanged(bool)), this, SLOT(searchPanelVisibilityChanged(bool)));
	connect(searchPanel_, SIGNAL(searchTerm(QString)), this, SLOT(searchTerm(QString)));
	connect(searchPanel_, SIGNAL(nextResult()), this, SLOT(searchNext()));
	addToolBar(Qt::BottomToolBarArea, searchPanel_);
	connect(tabs_, SIGNAL(currentChanged(int)), this, SLOT(showTermAt(int)));
}

void Quilte::configureTerminal(QVTermWidget* term) {
	term->setDefaultColours(QColor(settings_.value("foreground").toString()), QColor(settings_.value("background").toString()));
	term->setDoubleClickFullWord(settings_.value("doubleclick_fullword").toBool());
	term->setCursorBlinkInterval(settings_.value("cursor_blink_interval").toInt());
	term->setUnscrollOnKey(settings_.value("unscroll_on_key").toBool());
	term->setUnscrollOnOutput(settings_.value("unscroll_on_output").toBool());
	term->setCursorColour(QColor(settings_.value("cursor_colour").toString()));
	term->setBoldHighBright(settings_.value("bold_highbright").toBool());
	term->setEnableAltScreen(settings_.value("altscreen").toBool());
	term->setScrollBufferSize(settings_.value("scrollback_size").toInt());
	QFont f;
	f.setFamily(settings_.value("font").toString());
	f.setPixelSize(settings_.value("font_size").toInt());
	term->setFont(f);
	term->setCursorShape(settings_.value("cursor_shape").toInt());
}

void Quilte::searchPanelVisibilityChanged(bool v) {
	if(!v) currentTerm()->clearSearchResults();
}

void Quilte::searchTerm(QString s) {
	if(s.length() > 2)
		currentTerm()->findText(s);
	else
		currentTerm()->clearSearchResults();
}

void Quilte::searchNext() {
	currentTerm()->findNext();
}

QVTermWidget* Quilte::currentTerm() {
	return qobject_cast<QVTermWidget*>(tabs_->currentWidget());
}

Quilte::~Quilte() {
	delete menu_;
	delete tabs_;
}

void Quilte::newTab() {
	QVTermWidget* t = new QVTermWidget(settings_.value("altscreen").toBool());
	configureTerminal(t);
	t->start(settings_.value("term_env").toString(),settings_.value("shell").toString(), settings_.value("launch_cmd").toString());
	connect(t, SIGNAL(finished(QVTermWidget*)), this, SLOT(closeTerm(QVTermWidget*)));
	connect(t, SIGNAL(titleChanged(QVTermWidget*,QString)), this, SLOT(setTermTitle(QVTermWidget*,QString)));
	tabs_->addTab(t,"quilte");
	tabs_->setCurrentWidget(t);
	tabs_->tabBar()->setVisible(settings_.value("always_show_tabbar").toBool() || tabs_->count() != 1);
}

void Quilte::saveBuffer() {
	QString fileName = QFileDialog::getSaveFileName(this, "Save Buffer");
	if(!fileName.isEmpty()) {
		QString buffer = currentTerm()->getEntireBuffer();
		QFile f(fileName);
		if(!f.open(QIODevice::WriteOnly)) {
			QMessageBox::warning(this, "Error", "Could not open " + fileName + " for writing");
			return;
		}
		f.write(buffer.toLocal8Bit());
	}
}

void Quilte::showTermAt(int i) {
	searchPanel_->hide();
	if(i >= 0) {
		setTermTitle(currentTerm(), currentTerm()->title());
		tabs_->widget(i)->setFocus();
	}
}

void Quilte::setTermTitle(QVTermWidget* term, QString title) {
	tabs_->setTabText(tabs_->indexOf(term), title);
	if(term == currentTerm())
		setWindowTitle(title + " - quilte");
}

void Quilte::closeCurrentTab() {
	closeTerm(currentTerm());
}

void Quilte::closeTerm(QVTermWidget* term) {
	delete term;
	if(tabs_->count() == 0)
		close();
	tabs_->tabBar()->setVisible(settings_.value("always_show_tabbar").toBool() || tabs_->count() != 1);
}

void Quilte::editCopy(){
	currentTerm()->copySelectedText();
}

void Quilte::editPaste() {
	currentTerm()->pasteFromClipboard();
}

void Quilte::editClear() {

}

void Quilte::showPrefs() {
	PrefsDialog* prefsDialog = new PrefsDialog(settings_, this);
	if(prefsDialog->exec()) {
		for(int i = 0; i < tabs_->count(); ++i) {
			QVTermWidget* term = qobject_cast<QVTermWidget*>(tabs_->widget(i));
			configureTerminal(term);
		}
	}
	menu_->setVisible(settings_.value("show_menubar").toBool());
	tabs_->tabBar()->setVisible(settings_.value("always_show_tabbar").toBool() || tabs_->count() != 1);
}

void Quilte::contextMenuEvent(QContextMenuEvent *event) {
	contextMenu_->exec(mapToGlobal(event->pos()));
}

void Quilte::toggleMenubar() {
	menu_->setVisible(!menu_->isVisible());
}

void Quilte::toggleFullScreen() {
	if(!isFullScreen())
		showFullScreen();
	else
		showNormal();
}

void Quilte::nextTab() {
	tabs_->setCurrentIndex((tabs_->currentIndex()+1)%tabs_->count());
}

void Quilte::prevTab() {
	tabs_->setCurrentIndex((tabs_->currentIndex()-1)%tabs_->count());
}

void Quilte::find() {
	searchPanel_->show();
	searchPanel_->setFocus();
}

void Quilte::findNext() {

}

void Quilte::website() {
	QDesktopServices::openUrl(QUrl("https://github.com/ohwgiles/quilte"));
}

void Quilte::about() {
	QMessageBox::about(this, "About quilte", "TODO");
}
