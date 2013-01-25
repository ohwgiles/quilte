
#include "quilte.hpp"
#include <QApplication>
#include <QSettings>

int main(int argc, char **argv) {

	QApplication a(argc,argv);

	QSettings gSettings("/home/og/.config/qvtermwidget.conf", QSettings::IniFormat);
	gSettings.setValue("test","this");

//	int lines = gSettings.value("lines", 25).toInt();
//	int cols = gSettings.value("cols",50).toInt();
	Quilte m;
//	QVTermWidget v;

//	QColor fg_col(gSettings.value("foreground","gray90").toString());
//	QColor bg_col(gSettings.value("background","black").toString());
//	QFont font(gSettings.value("font","Inconsolata").toString());
//	font.setPointSize(gSettings.value("font_size",12.0).toDouble());

//	v.setFont(font);
//	v.setDoubleClickFullWord(gSettings.value("doubleclick_fullword", false).toBool());
//	v.setCursorBlinkInterval(gSettings.value("cursor_blink_interval",500).toInt());
//	v.setUnscrollOnKey(gSettings.value("unscroll_on_key",true).toBool());
//	v.setCursorColour(QColor(gSettings.value("cursor_colour","pink").toString()));
//	v.setBoldHighBright(gSettings.value("bold_highbright",true).toBool());
//	v.setEnableAltScreen(gSettings.value("altscreen",true).toBool());
//	v.setScrollBufferSize(gSettings.value("scrollback_size",1000).toInt());
//	v.setCursorShape(gSettings.value("cursor_shape",VTERM_PROP_CURSORSHAPE_BLOCK).toInt());
//	v.setUnscrollOnOutput(gSettings.value("unscroll_on_output",false).toBool());
//	v.setTermSize(lines,cols);
//	v.setDefaultColours(fg_col, bg_col);
//	v.start(gSettings.value("TERM","xterm").toString(), "/bin/bash");

	m.show();
	m.newTab();
	a.exec();

	gSettings.sync();
	return 0;
}
