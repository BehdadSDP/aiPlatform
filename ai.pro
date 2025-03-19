QT -= gui

CONFIG += c++17 console
CONFIG -= app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    camera_handler.cpp \
    dataloader.cpp \
    frame_buffer_manager.cpp \
    main.cpp \
    model.cpp

# Add libcamera and dependencies
LIBS += -L/usr/lib -lcamera -lcamera-base -lpisp -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs -lopencv_dnn -lcamera


INCLUDEPATH += /usr/include/libcamera
INCLUDEPATH += /usr/include/opencv4

#TRANSLATIONS += \
#    aiFirmware_en_GB.ts
#CONFIG += lrelease
#CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

#HEADERS += \
#    cameraHandler.h

HEADERS += \
    camera_handler.h \
    dataloader.h \
    frame_buffer_manager.h \
    model.h
