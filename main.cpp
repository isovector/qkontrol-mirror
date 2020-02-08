#include <QApplication>
#include "qkontrol.h"

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	app.setApplicationName("qKontrol");
	qkontrolWindow win;
	win.show();
	app.setQuitOnLastWindowClosed(true);

	return app.exec();
}
