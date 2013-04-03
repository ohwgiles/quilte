cmake_minimum_required(VERSION 2.8.8)
project(quilte)
set(WITH_QVTERMWIDGET TRUE CACHE BOOL "Whether to build and install QVTermWidget")
set(WITH_QUILTE TRUE CACHE BOOL "Whether to build and install quilte")

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
find_package(Qt5Widgets)
#include_directories(${Qt5Widgets_INCLUDE_DIRS})
add_definitions(-Wall -g -std=c++11)


if(WITH_QVTERMWIDGET)
   #add_library(QVTermWidget SHARED
set(QVTermWidget_INTERNAL qvtermwidget/qvtermwidget.cpp qvtermwidget/keyconversion.cpp qvtermwidget/vtermcallbacks.cpp)
   #target_link_libraries(QVTermWidget vterm
# ${Qt5Widgets_LIBRARIES}
 #util)
   #include_directories(qvtermwidget)
#qt5_use_modules(QVTermWidget Widgets)
   #install(FILES qvtermwidget/qvtermwidget.hpp qvtermwidget/QVTermWidget DESTINATION include)
   #install(TARGETS QVTermWidget LIBRARY DESTINATION lib)
endif(WITH_QVTERMWIDGET)

if(WITH_QUILTE)
	set(CMAKE_CXX_FLAGS "${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
		add_executable(quilte ${QVTermWidget_INTERNAL} quilte.cpp searchpanel.cpp main.cpp prefsdialog.cpp)
qt5_use_modules(quilte Widgets)
target_link_libraries(quilte vterm ${Qt5Widgets_LIBRARIES} util)
	install(TARGETS quilte RUNTIME DESTINATION bin)
endif(WITH_QUILTE)
