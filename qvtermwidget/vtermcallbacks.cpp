#include "qvtermwidget.hpp"
#include "vtermcallbacks.hpp"

struct VTermCallbacks {
	static int term_damage(VTermRect rect, void *user_data);
	static int term_prescroll(VTermRect rect, void *user_data);
	static int term_moverect(VTermRect dest, VTermRect src, void *user_data);
	static int term_movecursor(VTermPos pos, VTermPos oldpos, int visible, void *user_data);
	static int term_settermprop(VTermProp prop, VTermValue *val, void *user_data);
	static int term_setmousefunc(VTermMouseFunc func, void *data, void *user_data);
	static int term_bell(void *user_data);
};

int VTermCallbacks::term_damage(VTermRect rect, void *user_data) {
	QVTermWidget* qvt = static_cast<QVTermWidget*>(user_data);
	return qvt->damage(rect);
}

int VTermCallbacks::term_prescroll(VTermRect rect, void *user_data) {
	QVTermWidget* qvt = static_cast<QVTermWidget*>(user_data);
	return qvt->preScroll(rect);
}

int VTermCallbacks::term_moverect(VTermRect dest, VTermRect src, void *user_data) {
	QVTermWidget* qvt = static_cast<QVTermWidget*>(user_data);
	return qvt->moveRect(dest, src);
}

int VTermCallbacks::term_movecursor(VTermPos pos, VTermPos oldpos, int visible, void *user_data) {
	QVTermWidget* qvt = static_cast<QVTermWidget*>(user_data);
	return qvt->moveCursor(pos, oldpos, visible);
}

int VTermCallbacks::term_settermprop(VTermProp prop, VTermValue *val, void *user_data) {
	QVTermWidget* qvt = static_cast<QVTermWidget*>(user_data);
	return qvt->setTerminalProperty(prop, val);
}

int VTermCallbacks::term_setmousefunc(VTermMouseFunc func, void *data, void *user_data) {
	QVTermWidget* qvt = static_cast<QVTermWidget*>(user_data);
	return qvt->setMouseFunc(func, data);
}

int VTermCallbacks::term_bell(void *user_data) {
	QVTermWidget* qvt = static_cast<QVTermWidget*>(user_data);
	return qvt->bell();
}

void registerVtermCallbacks(VTermScreen* vts, QVTermWidget* owner) {

	static VTermScreenCallbacks cb = {
	  VTermCallbacks::term_damage,       //damage       = 
	  VTermCallbacks::term_prescroll,    //prescroll    = 
	  VTermCallbacks::term_moverect,     //moverect     = 
	  VTermCallbacks::term_movecursor,   //movecursor   = 
	  VTermCallbacks::term_settermprop,  //settermprop  = 
	  VTermCallbacks::term_setmousefunc, //setmousefunc = 
	  VTermCallbacks::term_bell          //bell         = 
	};


vterm_screen_set_callbacks(vts, &cb, owner);
};
