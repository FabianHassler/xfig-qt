cmake_minimum_required(VERSION 3.21)
project(xfig-qt VERSION 3.2.9 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)
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
	${CMAKE_SOURCE_DIR}/core/w_listwidget.c)	# an Xaw widget subclass
if(NOT TIFF_FOUND)
	list(REMOVE_ITEM CORE_SOURCES ${CMAKE_SOURCE_DIR}/core/f_readtif.c)
endif()

add_library(xfigcore STATIC
	${CORE_SOURCES}
	shim/xt_null.c
	shim/xt_widget.c
	shim/xt_translate.c
	shim/xpm.c
	shim/fallback.c)
target_include_directories(xfigcore PUBLIC core shim)
# USE_XPM: XPM pictures, export and the color splash, with shim/xpm.c
target_compile_definitions(xfigcore PUBLIC HAVE_CONFIG_H USE_XPM
	MAX_TOPRULER_WD=8192 MAX_SIDERULER_HT=8192)
if(TIFF_FOUND)
	target_compile_definitions(xfigcore PUBLIC HAVE_TIFF)
	target_link_libraries(xfigcore PUBLIC PkgConfig::TIFF)
endif()
target_compile_options(xfigcore PRIVATE -w)
# xfig ends with exit() from deep inside callbacks; xq_exit() ends the
# process without running Qt's teardown in the middle of event handling
target_compile_definitions(xfigcore PRIVATE $<$<COMPILE_LANGUAGE:C>:exit=xq_exit>)
# xfig's main() becomes xfig_main(), called by the Qt main()
set_source_files_properties(core/main.c PROPERTIES COMPILE_DEFINITIONS main=xfig_main)
target_link_libraries(xfigcore PUBLIC PkgConfig::FONTCONFIG PkgConfig::FREETYPE
	PkgConfig::PNG JPEG::JPEG m)
if(APPLE)
	# iconv is part of the C library on Linux, separate on macOS
	target_link_libraries(xfigcore PUBLIC iconv)
endif()

# ------------------------------------------------------------- application
qt_add_executable(xfig MACOSX_BUNDLE
	platform/xlib_qt.cpp
	platform/xt_qt.cpp
	ui/main.cpp
	ui/mainwindow.cpp
	ui/xaw.cpp
	ui/testscript.cpp
	ui/print.cpp
	ui/bridge.c)
target_include_directories(xfig PRIVATE platform ui)
# running from the build directory, xfig's data is data/ in the sources
target_compile_definitions(xfig PRIVATE
	XFIGQT_SOURCE_DATADIR="${CMAKE_SOURCE_DIR}/data")
target_link_libraries(xfig PRIVATE xfigcore Qt6::Widgets Qt6::PrintSupport)
# printing goes to CUPS (ui/print.cpp)
find_library(CUPS_LIBRARY cups REQUIRED)
target_link_libraries(xfig PRIVATE ${CUPS_LIBRARY})
set_target_properties(xfig PROPERTIES
	MACOSX_BUNDLE_BUNDLE_NAME "Xfig"
	MACOSX_BUNDLE_GUI_IDENTIFIER "org.xfig.xfig"
	MACOSX_BUNDLE_SHORT_VERSION_STRING "3.2.9a")

# --------------------------------------------------------------- install
# xfig's data: CompKeyDB, Libraries/ (object library), doc/ (help).  The
# program finds it in the app bundle (macOS) or in ../share/xfig-qt.
include(GNUInstallDirs)
set(PACKAGE_VERSION 3.2.9a)
set(PACKAGE_URL "https://github.com/FabianHassler/xfig-qt")
set(PACKAGE_BUGREPORT "${PACKAGE_URL}/issues")
set(RELEASE_DATE "December 2024")
set(RELEASE_YEAR 2024)
set(GSEXE gs)
set(gsman gs)
if(APPLE)
	set(DATADIR xfig.app/Contents/Resources)
	install(TARGETS xfig BUNDLE DESTINATION .)
else()
	set(DATADIR ${CMAKE_INSTALL_DATADIR}/xfig-qt)
	install(TARGETS xfig RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
set(COMPKEYDBDIR ${CMAKE_INSTALL_PREFIX}/${DATADIR})
set(PKGDATADIR ${CMAKE_INSTALL_PREFIX}/${DATADIR})
set(DOCDIR ${CMAKE_INSTALL_PREFIX}/${DATADIR}/doc)
set(HTMLDIR ${DOCDIR}/html)
set(APPDEFAULTDIR "(none, xfig-qt has no X resource files)")
set(ICONDIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps)
set(APPLICATIONSDIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/applications)
set(METAINFODIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/metainfo)
configure_file(data/doc/xfig.man.in xfig.1 @ONLY)
configure_file(data/doc/xfig_man.html.in xfig_man.html @ONLY)

install(FILES data/CompKeyDB DESTINATION ${DATADIR})
install(DIRECTORY data/Libraries data/doc DESTINATION ${DATADIR}
	PATTERN "*.in" EXCLUDE)
install(FILES ${CMAKE_BINARY_DIR}/xfig_man.html DESTINATION ${DATADIR}/doc)
if(UNIX AND NOT APPLE)
	install(FILES ${CMAKE_BINARY_DIR}/xfig.1
		DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
	install(FILES data/org.xfig.xfig.desktop
		DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
	install(FILES data/org.xfig.xfig.metainfo.xml
		DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
	install(FILES data/xfig.png
		DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps)
endif()
