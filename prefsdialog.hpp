#ifndef PREFSDIALOG_HPP
#define PREFSDIALOG_HPP

#include <QDialog>

class QSettings;
class QCheckBox;
class QSlider;
class QSpinBox;
class QLineEdit;
class QButtonGroup;

class PrefsDialog : public QDialog {
	Q_OBJECT

public:
	explicit PrefsDialog(QSettings& settings, QWidget *parent = 0);

protected:
	void accept();

private slots:
	void getBackgroundColour();
	void getForegroundColour();
	void getFont();

private:
	QPushButton* bgColourButton;
	QPushButton* fgColourButton;
	QPushButton* fontButton_;
	QCheckBox* doubleClickFullWord_;
	QButtonGroup* cursorShape_;
	QSlider* cursorBlinkInterval_;
	QCheckBox* unscrollOnKey_;
	QCheckBox* unscrollOnOutput_;
	QCheckBox* enableAltScreen_;
	QCheckBox* boldHighBright_;
	QSpinBox* scrollBufferSize_;
	QLineEdit* termEnv_;
	QLineEdit* shell_;
	QLineEdit* launchCmd_;
	QSettings& settings_;
};

#endif // PREFSDIALOG_HPP
