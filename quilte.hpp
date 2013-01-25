#ifndef QUILTE_HPP
#define QUILTE_HPP

#include <QMainWindow>

class QTabWidget;
class QVTermWidget;

class Quilte : public QMainWindow {
	Q_OBJECT
public:
	Quilte();
	virtual ~Quilte();
private:
	QTabWidget* tabs_;
	QMenuBar* menu_;

	QVTermWidget* currentTerm();

public slots:
	void newTab();
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
};

#endif // QUILTE_HPP
