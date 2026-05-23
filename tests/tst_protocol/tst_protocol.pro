QT       += testlib
QT       -= gui

CONFIG   += qt console warn_on testcase c++17
CONFIG   -= app_bundle

TEMPLATE = app
TARGET   = tst_protocol

INCLUDEPATH += ../../src

SOURCES += \
    tst_protocol.cpp

HEADERS += \
    ../../src/GraphingProtocol.h
