#include "quilte.hpp"
#include <QApplication>
#include <QSettings>
#include <vterm.h>

int main(int argc, char **argv) {
	QApplication a(argc,argv);

	QSettings prefs(QString(getenv("HOME")) + "/.config/quilte.conf", QSettings::IniFormat);
	// configure all default settings
	if(!prefs.contains("foreground"))
		prefs.setValue("foreground","white");
	if(!prefs.contains("background"))
		prefs.setValue("background","black");
	if(!prefs.contains("font"))
		prefs.setValue("font","monospace");
	if(!prefs.contains("font_size"))
		prefs.setValue("font_size", 12);
	if(!prefs.contains("doubleclick_fullword"))
		prefs.setValue("doubleclick_fullword", false);
	if(!prefs.contains("cursor_blink_interval"))
		prefs.setValue("cursor_blink_interval", 500);
	if(!prefs.contains("unscroll_on_key"))
		prefs.setValue("unscroll_on_key", true);
	if(!prefs.contains("unscroll_on_output"))
		prefs.setValue("unscroll_on_output", true);
	if(!prefs.contains("cursor_colour"))
		prefs.setValue("cursor_colour","white");
	if(!prefs.contains("bold_highbright"))
		prefs.setValue("bold_highbright", true);
	if(!prefs.contains("altscreen"))
		prefs.setValue("altscreen",true);
	if(!prefs.contains("scrollback_size"))
		prefs.setValue("scrollback_size", 1000);
	if(!prefs.contains("cursor_shape"))
		prefs.setValue("cursor_shape", VTERM_PROP_CURSORSHAPE_BLOCK);
	if(!prefs.contains("term_env"))
		prefs.setValue("term_env", "xterm-256color");
	if(!prefs.contains("shell"))
		prefs.setValue("shell", getenv("SHELL"));

	if(!prefs.contains("show_menubar"))
		prefs.setValue("show_menubar", true);
	if(!prefs.contains("always_show_tabbar"))
		prefs.setValue("always_show_tabbar", false);

	Quilte m(prefs);

	m.show();
	m.newTab();
	a.exec();
	return 0;
}
