QT       += testlib
QT       -= gui

CONFIG   += qt console warn_on testcase c++17
CONFIG   -= app_bundle

TEMPLATE = app
TARGET   = tst_libfn

SOURCES += \
    tst_libfn.cpp \
    ../../libfn/libfn.c
