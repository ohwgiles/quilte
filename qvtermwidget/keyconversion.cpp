
#include <qnamespace.h>
#include "keyconversion.hpp"

VTermKey convert_qt_keyval(int key) {
  if(key >= Qt::Key_F1 && key <= Qt::Key_F35)
	 return (VTermKey)VTERM_KEY_FUNCTION(key - Qt::Key_F1 + 1);

  switch(key) {
  case Qt::Key_Backspace: return VTERM_KEY_BACKSPACE;
  case Qt::Key_Tab: return VTERM_KEY_TAB;
  case Qt::Key_Return: return VTERM_KEY_ENTER;
  case Qt::Key_Escape: return VTERM_KEY_ESCAPE;

  case Qt::Key_Up:
	 return VTERM_KEY_UP;
  case Qt::Key_Down:
	 return VTERM_KEY_DOWN;
  case Qt::Key_Left:
	 return VTERM_KEY_LEFT;
  case Qt::Key_Right:
	 return VTERM_KEY_RIGHT;

  case Qt::Key_Insert:
	 return VTERM_KEY_INS;
  case Qt::Key_Delete:
	 return VTERM_KEY_DEL;
  case Qt::Key_Home:
	 return VTERM_KEY_HOME;
  case Qt::Key_End:
	 return VTERM_KEY_END;
  case Qt::Key_PageUp:
	 return VTERM_KEY_PAGEUP;
  case Qt::Key_PageDown:
	 return VTERM_KEY_PAGEDOWN;

  case Qt::Key_Enter:
	 return VTERM_KEY_ENTER;

  case Qt::Key_Period:
	 return VTERM_KEY_KP_PERIOD;
  case Qt::Key_Plus:
	 return VTERM_KEY_KP_PLUS;
  case Qt::Key_Equal:
	 return VTERM_KEY_KP_EQUAL;

  default:
	 return VTERM_KEY_NONE;
  }
}


VTermModifier convert_qt_modifier(int state)
{
  int mod = VTERM_MOD_NONE;
  if(state & Qt::SHIFT)
	 mod |= VTERM_MOD_SHIFT;
  if(state & Qt::CTRL)
	 mod |= VTERM_MOD_CTRL;
  if(state & Qt::ALT)
	 mod |= VTERM_MOD_ALT;

	 return (VTermModifier)mod;
}
