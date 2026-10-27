QT += widgets network

CONFIG += c++17

INCLUDEPATH += \
    $$PWD/third_party \
    $$PWD/third_party/lua/src

SOURCES += \
    src/host/bindings/bindapplication.cpp \
    src/host/bindings/bindfilesystem.cpp \
    src/host/bindings/bindhttp.cpp \
    src/host/bindings/bindingregistry.cpp \
    src/host/bindings/bindlogging.cpp \
    src/host/bindings/bindui.cpp \
    src/host/main.cpp \
    src/host/ui/mainwindow/mainwindow.cpp \
    src/host/ui/mainwindow/settingsdialog.cpp \
    src/host/runtime/luaerror.cpp \
    src/host/runtime/luaruntime.cpp \
    src/host/ui/mainwindow/menubar.cpp \
    src/host/ui/mainwindow/panes/editor.cpp \
    src/host/ui/mainwindow/panes/cmdconsole/cmdconsole.cpp \
    src/host/ui/mainwindow/panes/editor/mipscompleter.cpp \
    src/host/ui/mainwindow/panes/editor/mipshighlighter.cpp \
    src/host/ui/mainwindow/panes/filetree.cpp \
    src/host/ui/mainwindow/panes/assembler/assembler.cpp \
    src/host/ui/mainwindow/panes/snippets/snippets.cpp \
    src/host/ui/mainwindow/panes/searchablelisttable.cpp \
    src/host/ui/mainwindow/panes/functionview/functionview.cpp \
    src/host/ui/mainwindow/panes/ramaddressview/ramaddressview.cpp \
    src/host/ui/mainwindow/panes/structview/structview.cpp \
    src/host/ui/mainwindow/panes/marioactionsview/marioactionsview.cpp \
    src/host/ui/mainwindow/panes/eepromview/eepromview.cpp \
    src/host/ui/mainwindow/panes/luaconsole/luaconsole.cpp \
    src/host/ui/mainwindow/panes/pane.cpp \
    src/host/ui/mainwindow/splitter.cpp

HEADERS += \
    src/host/bindings/bindapplication.h \
    src/host/bindings/bindfilesystem.h \
    src/host/bindings/bindhttp.h \
    src/host/bindings/bindingregistry.h \
    src/host/bindings/bindlogging.h \
    src/host/bindings/bindui.h \
    src/host/bindings/pathresolver.h \
    src/host/ui/mainwindow/mainwindow.h \
    src/host/ui/mainwindow/settingsdialog.h \
    src/host/runtime/luaconfig.h \
    src/host/runtime/luaerror.h \
    src/host/runtime/luaruntime.h \
    src/host/ui/mainwindow/menubar.h \
    src/host/ui/mainwindow/panes/editor.h \
    src/host/ui/mainwindow/panes/cmdconsole/cmdconsole.h \
    src/host/ui/mainwindow/panes/editor/mipscompleter.h \
    src/host/ui/mainwindow/panes/editor/mipshighlighter.h \
    src/host/ui/mainwindow/panes/filetree.h \
    src/host/ui/mainwindow/panes/assembler/assembler.h \
    src/host/ui/mainwindow/panes/snippets/snippets.h \
    src/host/ui/mainwindow/panes/searchablelisttable.h \
    src/host/ui/mainwindow/panes/functionview/functionview.h \
    src/host/ui/mainwindow/panes/ramaddressview/ramaddressview.h \
    src/host/ui/mainwindow/panes/structview/structview.h \
    src/host/ui/mainwindow/panes/marioactionsview/marioactionsview.h \
    src/host/ui/mainwindow/panes/eepromview/eepromview.h \
    src/host/ui/mainwindow/panes/luaconsole/luaconsole.h \
    src/host/ui/mainwindow/panes/pane.h \
    src/host/ui/mainwindow/splitter.h

FORMS += \
    src/host/ui/mainwindow/mainwindow.ui

RESOURCES += \
    resources/logo.qrc \
    src/host/ui/mainwindow/panes/functionview/functionview.qrc \
    src/host/ui/mainwindow/panes/ramaddressview/ramaddressview.qrc \
    src/host/ui/mainwindow/panes/structview/structview.qrc \
    src/host/ui/mainwindow/panes/marioactionsview/marioactionsview.qrc \
    src/host/ui/mainwindow/panes/eepromview/eepromview.qrc \
    src/host/ui/mainwindow/panes/snippets/snippets.qrc

CONFIG(debug, debug|release) {
    DEFINES += MIPSIE_DEV_BUILD
} else {
    DEFINES += MIPSIE_RELEASE_BUILD
}

include(third_party/lua/lua.pri)

RC_FILE = src/iconBS.rc
LIBS += -ladvapi32

DISTFILES += \
    third_party/lua/lua.pri
