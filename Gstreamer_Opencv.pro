TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG += qt

CONFIG += link_pkgconfig
PKGCONFIG += gstreamer-1.0 glib-2.0 gobject-2.0 gstreamer-app-1.0 gstreamer-pbutils-1.0
CONFIG += link_pkgconfig
PKGCONFIG += opencv


SOURCES += \
        main.cpp

HEADERS +=
