#-------------------------------------------------
#
# Project created by QtCreator 2022-04-05T13:50:09
#
#-------------------------------------------------

QT += core gui widgets

TARGET = ComputerStatusNeo
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
#DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++20

SOURCES += \
        main.cpp \
        mainwindow.cpp \
        switchuser.hpp

HEADERS += \
        CComPtr.h \
        doublehop.hpp \
        mainwindow.h

FORMS += \
        mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += assets.qrc \
    qt.qrc

win32:RC_ICONS += computer_on_fire.ico

LIBS += \
        "C:/Windows/System32/credui.dll" \
        "C:/Windows/System32/ole32.dll" \
        "C:/Windows/System32/propsys.dll" \
        "C:/Windows/System32/taskschd.dll"

INCLUDEPATH += "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.34.31933/atlmfc/include"
