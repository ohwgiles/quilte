QT += core gui widgets

TARGET = quilte
TEMPLATE = app

QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.13

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# Check that all prerequisites are present:
LIBVTERM_DIR = $$(LIBVTERM_DIR)
isEmpty(LIBVTERM_DIR) {
    error(Please set LIBVTERM_DIR environment variable.)
}

exists("$(LIBVTERM_DIR)/include") {
    INCLUDEPATH += $(LIBVTERM_DIR)/include
}
exists("$(LIBVTERM_DIR)/lib") {
    LIBS *= -L$(LIBVTERM_DIR)/lib
}
LIBS *= -lvterm -lutil

INCLUDEPATH += qvtermwidget

SOURCES += \
    main.cpp \
    prefsdialog.cpp \
    quilte.cpp \
    searchpanel.cpp \
    qvtermwidget/keyconversion.cpp \
    qvtermwidget/qvtermwidget.cpp \
    qvtermwidget/vtermcallbacks.cpp

HEADERS += \
    prefsdialog.hpp \
    quilte.hpp \
    searchpanel.hpp \
    qvtermwidget/QVTermWidget \
    qvtermwidget/keyconversion.hpp \
    qvtermwidget/qvtermwidget.hpp \
    qvtermwidget/vtermcallbacks.hpp

