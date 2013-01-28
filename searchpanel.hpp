#ifndef SEARCHPANEL_HPP
#define SEARCHPANEL_HPP

#include <QToolBar>

class QLineEdit;

class SearchPanel : public QToolBar
{
	Q_OBJECT
public:
	explicit SearchPanel(QWidget *parent = 0);
	
signals:
	void searchTerm(QString);
	void nextResult();

public slots:
	void textChanged(QString);

protected:
	QLineEdit* lineEdit_;
	void focusInEvent(QFocusEvent *);
	void keyPressEvent(QKeyEvent *);


};

#endif // SEARCHPANEL_HPP
