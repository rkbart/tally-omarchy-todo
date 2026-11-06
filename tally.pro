QT += core gui qml quick quickcontrols2 dbus
CONFIG += c++17 release
TARGET = tally
TEMPLATE = app
HEADERS += src/backend.h src/theme.h
SOURCES += src/main.cpp src/backend.cpp src/theme.cpp
RESOURCES += src/resources.qrc
