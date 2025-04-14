QT -= gui

CONFIG += c++17 console
CONFIG -= app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    camera_handler.cpp \
    config_utils.cpp \
    control_unit.cpp \
    frame_buffer_manager.cpp \
    main.cpp \
    model.cpp \
    shared_data.cpp \
    vittracker.cpp

# Add libcamera and dependencies
LIBS += -L/usr/lib -lcamera -lcamera-base -lpisp -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs -lopencv_dnn -lcamera

# OpenCV libraries (try different combinations)
LIBS += -L/usr/local/lib \
        -lopencv_core \
        -lopencv_imgproc \
        -lopencv_highgui \
        -lopencv_videoio \
        -lopencv_imgcodecs \
        -lopencv_dnn \
        -lopencv_tracking \
        -lopencv_video \  # Try adding this
        -lopencv_objdetect \  # Sometimes needed
        -lopencv_ml \  # Sometimes needed
        -lopencv_gapi  # Required for some newer tracking algorithms

INCLUDEPATH += /usr/include/libcamera
INCLUDEPATH += /usr/local/include/opencv4

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

#HEADERS += \
#    cameraHandler.h

HEADERS += \
    camera_handler.h \
    config_utils.h \
    control_unit.h \
    frame_buffer_manager.h \
    model.h \
    shared_data.h \
    vittracker.h
