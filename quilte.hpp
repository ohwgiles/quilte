#ifndef QUILTE_HPP
#define QUILTE_HPP

#include <QMainWindow>

class QTabWidget;
class QVTermWidget;
class SearchPanel;
class QSettings;

// expose tabBar() publicly
class QTabWidget_ : public QTabWidget { public:
	QTabBar* tabBar() const { return QTabWidget::tabBar(); }
};

class Quilte : public QMainWindow {
	Q_OBJECT
public:
	explicit Quilte(QSettings& settings);
	virtual ~Quilte();
private:
	QSettings& settings_;
	QTabWidget_* tabs_;
	QMenuBar* menu_;

	QVTermWidget* currentTerm();
	SearchPanel* searchPanel_;
QMenu* contextMenu_;
	void configureTerminal(QVTermWidget *term);
	void contextMenuEvent(QContextMenuEvent *event);
public slots:
	void newTab();
	void saveBuffer();
	void closeCurrentTab();
	void closeTerm(QVTermWidget* term);
	void showTermAt(int);
	void setTermTitle(QVTermWidget* term, QString title);
	void editCopy();
	void editPaste();
	void editClear();
	void showPrefs();
	void toggleMenubar();
	void toggleFullScreen();
	void nextTab();
	void prevTab();
	void find();
	void findNext();
	void website();
	void about();
	void searchPanelVisibilityChanged(bool);
	void searchTerm(QString);
	void searchNext();
};

#endif // QUILTE_HPP
