#-------------------------------------------------
#
# Project created by QtCreator 2014-09-16T15:43:51
#
#-------------------------------------------------

# add sql here to enable data saving feature in this project
QT       += core gui sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = parking
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp\
        uart.cpp

HEADERS  += mainwindow.h\
            uart.h

FORMS    += mainwindow.ui

# in case we want to add pictures later on
# RESOURCES     = gui_resources.qrc

#-------------------------------------------------
# This section will include QextSerialPort in
# your project:

HOMEDIR = $$(HOME)
include($$HOMEDIR/qextserialport/src/qextserialport.pri)

# Before running the project, run qmake first:
# In Qt Creator, right-click on the project
# and choose "run qmake".
# The qextserialport folder should appear in the
# directory tree.
# Now your project is ready to run.
#------------------------------------------------
