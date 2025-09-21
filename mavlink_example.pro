QT -= gui

CONFIG += c++17 console
CONFIG -= app_bundle

TARGET = mavlink_example

# MAVLink example application
SOURCES += \
    src/mavlink_example.cpp \
    src/mavlink_handler.cpp

HEADERS += \
    include/mavlink_handler.h

# Include paths
INCLUDEPATH += include
INCLUDEPATH += include/Output
INCLUDEPATH += include/Output/common

# Threading support
LIBS += -lpthread

# Default rules for deployment
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

