#ifndef _QKONTROLWINDOW_H_
#define _QKONTROLWINDOW_H_

#include <QDir>
#include <QTemporaryFile>
#include <QTimer>
#ifdef Q_OS_MACOS
#include "/usr/local/Cellar/hidapi/0.9.0/include/hidapi/hidapi.h"
#else
#include <hidapi/hidapi.h>
#endif
#include "dropgraphicsview.h"
#include "ui_qkontrol.h"

class qkontrolWindow : public QMainWindow , protected Ui_mainwindow
{
	Q_OBJECT

	public:
		qkontrolWindow(QWidget *parent = 0, Qt::WindowFlags flags = 0);
		~qkontrolWindow();

	private:
		int res;
		int pid;
		unsigned int bPage, kPage, kontrolPage, dirCount, dirPosition;
		hid_device *handle;
		QByteArray lightArray, knobsButtons;
		QMap<QString,QColor> allColors;
		QTemporaryFile leftScreen, rightScreen;
		QTimer *hid_data;
		QString getControlName(uint8_t CC);
		QDir dirName;
		bool load(QString filename);

	private slots:
		bool save();
		void getFileName();

	protected slots:
                void drawImage(uint8_t screen, QPixmap *pixmap, ushort x = 0, ushort y = 0);
		void b_goLeft();
		void b_goRight();
		void b_setPage(int page);
		void k_goLeft();
		void k_goRight();
		void k_setPage(int page);
		void setKontrolpage(unsigned int page);
		void selectColor(QString target);
		void setSlidertextcolor();
		void setDividercolor();
		void setCCtextcolor();
		void setParametertextcolor();
		void setValuetextcolor();
		void setButtons();
		void setKeyzones();
		void updateValues();
		void updateColors();
		void updatePedalview();
		void updateWidgets();
		void zapPreset(bool direction);

	public slots:
};

#endif /*_QKONTROLWINDOW_H_*/
