#include "searchpanel.hpp"
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QIcon>
#include <QKeyEvent>

SearchPanel::SearchPanel(QWidget *parent) :
	QToolBar(parent)
{
	addWidget(new QLabel("Search term:", this));
	lineEdit_ = new QLineEdit(this);
	connect(lineEdit_, SIGNAL(textEdited(QString)), this, SLOT(textChanged(QString)));
	addWidget(lineEdit_);
	setMovable(false);
	setFloatable(false);
	setIconSize(QSize(16,16));
	addAction(QIcon::fromTheme("window-close"),"Close", this, SLOT(close()));
}

void SearchPanel::focusInEvent(QFocusEvent *) {
	lineEdit_->setFocus();
}

void SearchPanel::keyPressEvent(QKeyEvent *e) {
	if(e->key() == Qt::Key_Escape)
		hide();
	if(e->key() == Qt::Key_Return)
		emit nextResult();
}

void SearchPanel::textChanged(QString s) {
	emit searchTerm(s);
}
