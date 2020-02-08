CONFIG += release
TEMPLATE += app
TARGET = qkontrol 
DEPENDPATH += . widgets
INCLUDEPATH += . widgets

QT += widgets gui testlib xml

FORMS += qkontrol.ui
HEADERS += qkontrol.h widgets/qxtstringspinbox.h widgets/qxtspanslider.h widgets/qxtspanslider_p.h dropgraphicsscene.h dropgraphicsview.h
SOURCES += main.cpp qkontrol.cpp widgets/qxtstringspinbox.cpp widgets/qxtspanslider.cpp dropgraphicsscene.cpp dropgraphicsview.cpp
RESOURCES += qkontrol.qrc

!macx: LIBS += -lhidapi-libusb -lusb-1.0

macx: LIBS += -L$$PWD/../../../../usr/local/Cellar/hidapi/0.9.0/lib/ -lhidapi -L$$PWD/../../../../usr/local/Cellar/libusb/1.0.22/lib/ -lusb-1.0
macx: INCLUDEPATH += $$PWD/../../../../usr/local/Cellar/hidapi/0.9.0/include/hidapi $$PWD/../../../../usr/local/Cellar/libusb/1.0.22/include/libusb-1.0/
macx: DEPENDPATH += $$PWD/../../../../usr/local/Cellar/hidapi/0.9.0/include/hidapi $$PWD/../../../../usr/local/Cellar/libusb/1.0.22/include/libusb-1.0/
macx: PRE_TARGETDEPS += $$PWD/../../../../usr/local/Cellar/hidapi/0.9.0/lib/libhidapi.a $$PWD/../../../../usr/local/Cellar/libusb/1.0.22/lib/libusb-1.0.a





