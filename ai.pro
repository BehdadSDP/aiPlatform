QT -= gui

CONFIG += c++17 console
CONFIG -= app_bundle

# Fix MAVLink packed structure alignment warnings
QMAKE_CXXFLAGS += -Wno-address-of-packed-member
QMAKE_CXXFLAGS += -Wno-pedantic

# MAVLink specific definitions for proper alignment
DEFINES += MAVLINK_ALIGNED_FIELDS=1
DEFINES += MAVLINK_COMMAND_24BIT=1

# You can make your code fail to compile if it uses deprecated APIs.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/main.cpp \
    src/application.cpp \
    src/visualizer.cpp \
    src/failure_handler.cpp \
    src/camera_handler.cpp \
    src/video_handler.cpp \
    src/config_utils.cpp \
    src/control_unit.cpp \
    src/frame_buffer_manager.cpp \
    src/model.cpp \
    src/model_manager.cpp \
    src/resource_monitor.cpp \
    src/tracking/siamfc_pp_tracker.cpp \
    src/tracking/vittracker.cpp \
    src/tracker_manager.cpp \
    src/detection_manager.cpp \
    src/detection/yolo_detector.cpp \
    src/mavlink.cpp \
    src/navigation_unit.cpp \

# Add libcamera and dependencies
LIBS += -L/usr/lib -lcamera-base -lpisp -lcamera

# OpenCV libraries (try different combinations)
LIBS += -L/usr/local/lib \
        -lopencv_core \
        -lopencv_imgproc \
        -lopencv_highgui \
        -lopencv_videoio \
        -lopencv_imgcodecs \
        -lopencv_dnn \
        -lopencv_tracking \
        -lopencv_video \
        -lopencv_objdetect \
        -lopencv_ml \
        -lopencv_gapi

# Add filesystem library for C++17
LIBS += -lstdc++fs

INCLUDEPATH += /usr/include/libcamera
INCLUDEPATH += /usr/local/include/opencv4
INCLUDEPATH += /home/pi5/shared_folder/aiPlatform/include/Output

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

#HEADERS += \
#    cameraHandler.h

HEADERS += \
    include/application.h \
    include/visualizer.h \
    include/failure_handler.h \
    include/camera_handler.h \
    include/video_handler.h \
    include/config_utils.h \
    include/control_unit.h \
    include/frame_buffer_manager.h \
    include/model.h \
    include/model_manager.h \
    include/resource_monitor.h \
    include/tracking/siamfc_pp_tracker.h \
    include/tracking/vittracker.h \
    include/tracker_interface.h \
    include/tracker_manager.h \
    include/detection_manager.h \
    include/detection/yolo_detector.h \
    include/frame_pool.h \
    include/mavlink.h \
    include/navigation_unit.h \
