QT += widgets
CONFIG += c++17 release
TARGET = PenumbraTraining
SOURCES += main.cpp
win32:LIBS += -lwinmm
HEADERS += actors.inc warden.inc
