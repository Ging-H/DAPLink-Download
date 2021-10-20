#-------------------------------------------------
#
# Project created by QtCreator 2021-09-03T14:17:47
#
#-------------------------------------------------

QT       += core gui network uiplugin

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Daplink
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0



SOURCES += \
        main.cpp \
        daplink.cpp \
        hid.c \
        programmer.cpp \
        memory.cpp \
        hextobin.cpp \
        openocdif.cpp \
        QTelnet.cpp


HEADERS += \
        daplink.h \
        hidapi.h \
        programmer.h \
        memory.h \
        hextobin.h \
        openocdif.h \
        QTelnet.h

FORMS += \
        programmer.ui

RC_ICONS = $$PWD/logo.ico


INCLUDEPATH += $$PWD/ledplugin
DESTDIR =  $$PWD/exe
mingw {
    ledplugin.path   = $$DESTDIR
    ledplugin.files += $$INCLUDEPATH/mingw/*.dll
    INSTALLS  += ledplugin

    LIBS  +=  $$INCLUDEPATH/mingw/libledplugin.a   # MinGW编译
} else {
    ledplugin.path   = $$DESTDIR
    ledplugin.files += $$INCLUDEPATH/msvc2015/*.dll \
                       $$INCLUDEPATH/msvc2015/*.lib

    ini.path   = $$DESTDIR
    ini.files += $$PWD/device.ini\
                 $$PWD/deviceinfo.ini\
                 $$PWD/SetupAPI.Lib

    INSTALLS  += ledplugin ini
    LIBS  +=  $$INCLUDEPATH/msvc2015/ledplugin.lib    # msvc2015编译
}
message( "需要自定义进程步骤:\"nmake install %{buildDir}\" ")

