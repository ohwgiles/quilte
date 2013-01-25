#ifndef KEYCONVERSION_HPP
#define KEYCONVERSION_HPP

extern "C" {
#include <vterm.h>
}

VTermKey convert_qt_keyval(int key);
VTermModifier convert_qt_modifier(int state);

#endif // KEYCONVERSION_HPP
