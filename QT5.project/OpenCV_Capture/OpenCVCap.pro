QT += core gui widgets network

CONFIG += c++11

TARGET = opencvCap
TEMPLATE = app

DEFINES += APP_VERSION=\\\"1.1\\\"

Win32 {
    RC_ICONS += ./main/image/OpenCap.ico
    message("Win64 Opencv440")
    INCLUDEPATH += D:\opencv440\install\include
    LIBS += -LD:\opencv440\install\x64\mingw\bin \
       -lopencv_core440 \
       -lopencv_highgui440 \
       -lopencv_imgproc440 \
       -lopencv_features2d440 \
       -lopencv_calib3d440 \
       -lopencv_imgcodecs440 \
       -lopencv_video440 \
       -lopencv_videoio440 \
       -lopencv_mcc440
    }

macx {
    message("MacOS Opencv4 OpenCVCap")
    message(Qt version: $$[QT_VERSION])
    message(Defines: $${DEFINES})
    QT_CONFIG -= no-pkg-config
    CONFIG += link_pkgconfig
    PKGCONFIG += opencv4
    PKG_CONFIG = /usr/local/bin/pkg-config
    ICON = OpenCap.icns
    }

INCLUDEPATH += $$PWD/main \
    $$PWD/main/helper \
    $$PWD/main/other \
    $$PWD/main/threads \
    $$PWD/main/ui

SOURCES += main/main.cpp \
    main/helper/MatToQImage.cpp \
    main/helper/MyUtils.cpp \
    main/helper/RangeSlider.cpp \
    main/helper/SharedImageBuffer.cpp \
    main/helper/_ProcessingFrame.cpp \
    main/helper/tcpsendpix.cpp \
    main/threads/CaptureThread.cpp \
    main/threads/PlayerThread.cpp \
    main/threads/ProcessingThread.cpp \
    main/threads/SavingThread.cpp \
    main/ui/CameraConnectDialog.cpp \
    main/ui/CameraView.cpp \
    main/ui/FrameLabel.cpp \
    main/ui/MainWindow.cpp \
    main/ui/VideoView.cpp


HEADERS += \
    main/helper/ComplexMat.h \
    main/helper/MatToQImage.h \
    main/helper/MyUtils.h \
    main/helper/RangeSlider.h \
    main/helper/SharedImageBuffer.h \
    main/helper/_ProcessingFrame.h \
    main/helper/tcpsendpix.h \
    main/threads/CaptureThread.h \
    main/threads/PlayerThread.h \
    main/threads/ProcessingThread.h \
    main/threads/SavingThread.h \
    main/ui/CameraConnectDialog.h \
    main/ui/CameraView.h \
    main/ui/FrameLabel.h \
    main/ui/MainWindow.h \
    main/ui/VideoView.h \
    main/other/Buffer.h \
    main/other/Config.h \
    main/other/Structures.h

FORMS += \
    main/ui/MainWindow.ui \
    main/ui/CameraView.ui \
    main/ui/CameraConnectDialog.ui \
    main/ui/VideoView.ui

# Spare me those nasty C++ compiler warnings and pray instead
#QMAKE_CXXFLAGS += -W2
QMAKE_CXXFLAGS += -Wno-deprecated

RESOURCES += \
    Resources.qrc

DISTFILES += \
    OpenCap.rc \
    main/image/appbar.check.png \
    main/image/appbar.close.png \
    main/image/appbar.control.pause.png \
    main/image/appbar.control.play.png
