cmake_minimum_required(VERSION 3.21)
project(xfig-qt VERSION 3.2.9 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(PNG REQUIRED IMPORTED_TARGET libpng)
find_package(JPEG REQUIRED)
pkg_check_modules(TIFF IMPORTED_TARGET libtiff-4)

# ---------------------------------------------------------------- xfig core
# The original xfig sources, compiled against the X11 shim headers.
file(GLOB CORE_SOURCES ${CMAKE_SOURCE_DIR}/core/*.c)
list(REMOVE_ITEM CORE_SOURCES
	${CMAKE_SOURCE_DIR}/core/u_draw_spline.c	# #included by u_draw.c
	${CMAKE_SOURCE_DIR}/core/w_listwidget.c		# an Xaw widget subclass
	${CMAKE_SOURCE_DIR}/core/f_readxpm.c)		# needs libXpm
if(NOT TIFF_FOUND)
	list(REMOVE_ITEM CORE_SOURCES ${CMAKE_SOURCE_DIR}/core/f_readtif.c)
endif()

add_library(xfigcore STATIC
	${CORE_SOURCES}
	shim/xt_null.c
	shim/xt_widget.c
	shim/xt_translate.c
	shim/fallback.c)
target_include_directories(xfigcore PUBLIC core shim)
target_compile_definitions(xfigcore PUBLIC HAVE_CONFIG_H
	OBJLIBDIR="${CMAKE_SOURCE_DIR}/../old/xfig-3.2.9a/Libraries"
	XFIGLIBDIR="${CMAKE_SOURCE_DIR}/../old/xfig-3.2.9a"
	XFIGDOCDIR="${CMAKE_SOURCE_DIR}/../old/xfig-3.2.9a/doc"
	MAX_TOPRULER_WD=8192 MAX_SIDERULER_HT=8192)
if(TIFF_FOUND)
	target_compile_definitions(xfigcore PUBLIC HAVE_TIFF)
	target_link_libraries(xfigcore PUBLIC PkgConfig::TIFF)
endif()
target_compile_options(xfigcore PRIVATE -w)
# xfig ends with exit() from deep inside callbacks; xq_exit() ends the
# process without running Qt's teardown in the middle of event handling
target_compile_definitions(xfigcore PRIVATE exit=xq_exit)
# xfig's main() becomes xfig_main(), called by the Qt main()
set_source_files_properties(core/main.c PROPERTIES COMPILE_DEFINITIONS main=xfig_main)
target_link_libraries(xfigcore PUBLIC PkgConfig::FONTCONFIG PkgConfig::FREETYPE
	PkgConfig::PNG JPEG::JPEG m)

# ------------------------------------------------------------- application
qt_add_executable(xfig MACOSX_BUNDLE
	platform/xlib_qt.cpp
	platform/xt_qt.cpp
	ui/main.cpp
	ui/mainwindow.cpp
	ui/xaw.cpp
	ui/testscript.cpp
	ui/bridge.c)
target_include_directories(xfig PRIVATE platform ui)
target_link_libraries(xfig PRIVATE xfigcore Qt6::Widgets)
set_target_properties(xfig PROPERTIES
	MACOSX_BUNDLE_BUNDLE_NAME "Xfig"
	MACOSX_BUNDLE_GUI_IDENTIFIER "org.xfig.xfig"
	MACOSX_BUNDLE_SHORT_VERSION_STRING "3.2.9a")
