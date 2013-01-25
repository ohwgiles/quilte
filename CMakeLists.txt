cmake_minimum_required(VERSION 2.6)
project(quilte)
find_package(Qt4 COMPONENTS QtCore QtGui REQUIRED)
link_directories(${QT_LIBRARY_DIR})
include_directories(${QT_INCLUDES})

set(WITH_QVTERMWIDGET TRUE CACHE BOOL "Whether to build and install QVTermWidget")
set(WITH_QUILTE TRUE CACHE BOOL "Whether to build and install quilte")

add_definitions(-g -Wall -Werror)

if(WITH_QVTERMWIDGET)
   qt4_wrap_cpp(MOC qvtermwidget/qvtermwidget.hpp)
   add_library(qvtermwidget SHARED qvtermwidget/qvtermwidget.cpp qvtermwidget/keyconversion.cpp qvtermwidget/vtermcallbacks.cpp ${MOC})
   target_link_libraries(qvtermwidget vterm ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} util)
   include_directories(qvtermwidget)
   install(FILES qvtermwidget/qvtermwidget.hpp DESTINATION include)
   install(TARGETS qvtermwidget LIBRARY DESTINATION lib)
endif(WITH_QVTERMWIDGET)

if(WITH_QUILTE)
	qt4_wrap_cpp(MOC quilte.hpp)
	add_executable(quilte quilte.cpp main.cpp ${MOC})
   target_link_libraries(quilte qvtermwidget)
	install(TARGETS quilte RUNTIME DESTINATION bin)
endif(WITH_QUILTE)

