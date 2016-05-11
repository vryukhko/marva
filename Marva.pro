######################################################################
# Automatically generated by qmake (3.0) ?? ??? 24 12:21:03 2015
######################################################################

TEMPLATE = app
TARGET = Marva
QT += widgets core gui
INCLUDEPATH += .
QMAKE_CXXFLAGS += -std=c++11
# Input
HEADERS += edge.h \
    blast_record.h \
    blast_data.h \
    main_window.h \
    tax_map.h \
    map_loader_thread.h \
    tree_loader_thread.h \
    graph_node.h \
    loader_thread.h \
    base_tax_node.h \
    ui_components/statuslistpanel.h \
    threadsafelist.h \
    ui_components/taxlistwidget.h \
    taxnodesignalsender.h \
    ui_components/leftpanel.h \
    ui_components/currenttaxnodedetails.h \
    ui_components/labeleddoublespinbox.h \
    common.h \
    taxdataprovider.h \
    chartview.h \
    datagraphicsview.h \
    graphview.h
FORMS += \
    main_window.ui \
    ui_components/taxlistwidget.ui \
    ui_components/leftpanel.ui \
    ui_components/currenttaxnodedetails.ui \
    ui_components/labeleddoublespinbox.ui
SOURCES += edge.cpp \
    blast_record.cpp \
    blast_data.cpp \
    main_window.cpp \
    tax_map.cpp \
    map_loader_thread.cpp \
    tree_loader_thread.cpp \
    graph_node.cpp \
    loader_thread.cpp \
    base_tax_node.cpp \
    ui_components/statuslistpanel.cpp \
    main.cpp \
    ui_components/taxlistwidget.cpp \
    taxnodesignalsender.cpp \
    ui_components/leftpanel.cpp \
    ui_components/currenttaxnodedetails.cpp \
    ui_components/labeleddoublespinbox.cpp \
    taxdataprovider.cpp \
    chartview.cpp \
    datagraphicsview.cpp \
    graphview.cpp
