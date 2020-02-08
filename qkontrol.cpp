#include <QtGlobal>
#include <iostream>
#ifdef Q_OS_MACOS
#include "/usr/local/Cellar/libusb/1.0.22/include/libusb-1.0/libusb.h"
#else
#include <libusb-1.0/libusb.h>
#endif
#include <math.h>

using namespace std;
#include <QApplication>
#include <QBuffer>
#include <QColorDialog>
#include <QDebug>
#include <QDomDocument>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QRgb>
#include <QStringList>
#include <QPainter>
#include "qkontrol.h"

qkontrolWindow::qkontrolWindow(QWidget* parent /* = 0 */, Qt::WindowFlags flags /* = 0 */) : QMainWindow(parent, flags)
{
        // search for the usb device and open it
        res = hid_init();
	QList<int> pids;
	pids << 0x1610 << 0x1620 << 0x1630; // try the device ids for 49-, 61- and 88-key versions

	for(int i=0; i<pids.count(); i++)
		{
		pid = pids[i];
		handle = hid_open(0x17cc, pid, NULL);
		if(handle)
			break;
		}
	if(!handle)
		{
		QMessageBox::critical(this, "no Komplete Kontrol found", "No Komplete Kontrol MK2 keyboard could be found. Please check if the device is turned on and connected to the PC! The program will be closed now");
		exit(0);
		}

#ifdef Q_OS_MACOS
	// kill the NI services which are blocking the device
	if(QMessageBox::question(this, "service termination", "To use this software properly on macOS, it is required to terminate some system services which are used by the Komplete Kontrol software. Do you want to kill them now? You can open the Komplete Kontrol software as usual afterwhile because it restarts them when you open it. If you choose 'no', you will be still able to configure mappings and the lightguide but the two displays cannot be used then") == QMessageBox::Yes)
		{
		system("killall -9 NIHardwareAgent");
		system("killall -9 NIHostIntegrationAgent");
		system("killall -9 NTKDaemon");
		}
#endif

	hid_set_nonblocking(handle, 1);

	setupUi(this);

	// define default colors
	allColors["slider"] = QColor(255, 63, 127);
	allColors["CC"] = QColor(255, 255, 0);
	allColors["parameter"] = QColor(255, 255, 255);
	allColors["divider"] = QColor(128, 128, 255);
	allColors["value"] = QColor(0, 255, 0); 
	QPixmap pixmapsliderscolor(color_sliders->width()-4, color_sliders->height()-4);
	QPixmap pixmapCCcolor(color_CC->width()-4, color_CC->height()-4);
	QPixmap pixmapParametercolor(color_parameters->width()-4, color_parameters->height()-4);
	QPixmap pixmapDividercolor(color_dividers->width()-4, color_dividers->height()-4);
	QPixmap pixmapValuecolor(color_values->width()-4, color_values->height()-4);
	pixmapsliderscolor.fill(allColors["slider"]);
	pixmapCCcolor.fill(allColors["CC"]);
	pixmapParametercolor.fill(allColors["parameter"]);
	pixmapDividercolor.fill(allColors["divider"]);
	pixmapValuecolor.fill(allColors["value"]);
	color_sliders->setIcon(pixmapsliderscolor);
	color_CC->setIcon(pixmapCCcolor);
	color_parameters->setIcon(pixmapParametercolor);
	color_dividers->setIcon(pixmapDividercolor);
	color_values->setIcon(pixmapValuecolor);

	// default images
	graphicsViewScreen1->setImage(":/images/qkontrol.png");
	graphicsViewScreen2->setImage(":/images/background.png");

	// populate the key list spin boxes with note names
	QStringList noteNames;
	QList<QxtStringSpinBox *> allNotes = tabWidget->findChildren<QxtStringSpinBox *>(QRegExp("^key_"));
	int octave = -2;
	for(int i=0;i<10;i++)
		{
		noteNames.append("C"+QString::number(octave));
		noteNames.append("C#"+QString::number(octave));
		noteNames.append("D"+QString::number(octave));
		noteNames.append("D#"+QString::number(octave));
		noteNames.append("E"+QString::number(octave));
		noteNames.append("F"+QString::number(octave));
		noteNames.append("F#"+QString::number(octave));
		noteNames.append("G"+QString::number(octave));
		noteNames.append("G#"+QString::number(octave));
		noteNames.append("A"+QString::number(octave));
		noteNames.append("A#"+QString::number(octave));
		noteNames.append("B"+QString::number(octave));
		octave++;
		}
	noteNames.append("C"+QString::number(octave)); // last supported octave is incomplete
	noteNames.append("C#"+QString::number(octave));
	noteNames.append("D"+QString::number(octave));
	noteNames.append("D#"+QString::number(octave));
	noteNames.append("E"+QString::number(octave));
	noteNames.append("F"+QString::number(octave));
	noteNames.append("F#"+QString::number(octave));
	noteNames.append("G"+QString::number(octave));
	for(int i=0;i<=15;i++)
		{
		allNotes[i]->setStrings(noteNames);
		allNotes[i]->setValue(127);
		}

	// before a preset is loaded, there are no presets to switch
	dirCount = 0;
	dirPosition = 0;

	// make the switch / continous tab bars (pedals) invisible
	tabWidget_pedal1->findChild<QTabBar *>()->hide();
	tabWidget_pedal2->findChild<QTabBar *>()->hide();

	// switch the (now invisible>) tabs when the radio buttons are toggled
	connect(radioButton_cont_1, SIGNAL(toggled(bool)), this, SLOT(updatePedalview()));
	connect(radioButton_switch_1, SIGNAL(toggled(bool)), this, SLOT(updatePedalview()));
	connect(radioButton_cont_2, SIGNAL(toggled(bool)), this, SLOT(updatePedalview()));
	connect(radioButton_switch_2, SIGNAL(toggled(bool)), this, SLOT(updatePedalview()));


	// process incoming HID data every 10 ms
	QTimer *hid_data = new QTimer(this);
	connect(hid_data, SIGNAL(timeout()), this, SLOT(updateValues()));
	hid_data->start(10);

	// keymap slot functions
	QList<QComboBox *> allKModes = tabWidget->findChildren<QComboBox *>(QRegExp("^k_mode_"));
	for(QComboBox *cb : allKModes)
		connect(cb, SIGNAL(currentIndexChanged(int)), this, SLOT(updateWidgets()));

	// button slot functionms
	QList<QComboBox *> allBModes = tabWidget->findChildren<QComboBox *>(QRegExp("^b_mode_"));		
	for(QComboBox *cb : allBModes)
		connect(cb, SIGNAL(currentIndexChanged(int)), this, SLOT(updateWidgets()));

	// pedal slot functions
	connect(p_controlmode_tip_1, SIGNAL(currentIndexChanged(int)), this, SLOT(updateWidgets()));
	connect(p_switchmode_tip_1, SIGNAL(currentIndexChanged(int)), this, SLOT(updateWidgets()));
	connect(p_controlmode_ring_1, SIGNAL(currentIndexChanged(int)), this, SLOT(updateWidgets()));
	connect(p_switchmode_ring_1, SIGNAL(currentIndexChanged(int)), this, SLOT(updateWidgets()));

	connect(p_controlmode_tip_2, SIGNAL(currentIndexChanged(int)), this, SLOT(updateWidgets()));
        connect(p_switchmode_tip_2, SIGNAL(currentIndexChanged(int)), this, SLOT(updateWidgets()));
        connect(p_controlmode_ring_2, SIGNAL(currentIndexChanged(int)), this, SLOT(updateWidgets()));
        connect(p_switchmode_ring_2, SIGNAL(currentIndexChanged(int)), this, SLOT(updateWidgets()));

	connect(p_mode_cont_1, SIGNAL(currentIndexChanged(int)), this, SLOT(updateWidgets()));

	connect(p_mode_cont_2, SIGNAL(currentIndexChanged(int)), this, SLOT(updateWidgets()));

	// color picker
	connect(color_sliders, SIGNAL(clicked()), this, SLOT(setSlidertextcolor()));
	connect(color_CC, SIGNAL(clicked()), this, SLOT(setCCtextcolor()));
	connect(color_parameters, SIGNAL(clicked()), this, SLOT(setParametertextcolor()));
	connect(color_dividers, SIGNAL(clicked()), this, SLOT(setDividercolor()));
	connect(color_values, SIGNAL(clicked()), this, SLOT(setValuetextcolor()));

	// tool buttons for page selection
	bPage = 0;
	kPage = 0;
	kontrolPage = 0;
	connect(toolButton_b_left, SIGNAL(clicked()), this, SLOT(b_goLeft()));
	connect(toolButton_b_right, SIGNAL(clicked()), this, SLOT(b_goRight()));
	connect(toolButton_k_left, SIGNAL(clicked()), this, SLOT(k_goLeft()));
	connect(toolButton_k_right, SIGNAL(clicked()), this, SLOT(k_goRight()));

	// default button background lightning array
	lightArray = QByteArray::fromHex("80000000000000000000000000000000000000000000000000000000000000000000FF00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
	setButtons();

	// other slot functions
	connect(loadButton, SIGNAL(clicked()), this, SLOT(getFileName()));
	connect(saveButton, SIGNAL(clicked()), this, SLOT(save()));
	connect(submitButton, SIGNAL(clicked()), this, SLOT(setKeyzones()));

	// initial submit
	setKeyzones();
	updateWidgets();
}

void qkontrolWindow::updatePedalview()
{
if(radioButton_cont_1->isChecked()) tabWidget_pedal1->setCurrentIndex(0);
if(radioButton_switch_1->isChecked()) tabWidget_pedal1->setCurrentIndex(1);

if(radioButton_cont_2->isChecked()) tabWidget_pedal2->setCurrentIndex(0);
if(radioButton_switch_2->isChecked()) tabWidget_pedal2->setCurrentIndex(1);
}

void qkontrolWindow::updateWidgets()
{
	// knob page
	QList<QComboBox *> allKModes = tabWidget->findChildren<QComboBox *>(QRegExp("^k_mode_"));
	QList<QSpinBox *> allKChannels = tabWidget->findChildren<QSpinBox *>(QRegExp("^k_channel_"));
	QList<QSpinBox *> allKControls = tabWidget->findChildren<QSpinBox *>(QRegExp("^k_CC_"));
	QList<QLineEdit *> allKDescriptions = tabWidget->findChildren<QLineEdit *>(QRegExp("^k_description_"));
	for(int i=0; i<allKModes.count();i++)
		switch(allKModes[i]->currentIndex())
			{
			case 0: allKChannels[i]->setEnabled(0); allKControls[i]->setEnabled(0); allKDescriptions[i]->setEnabled(0); break;
			case 1: allKChannels[i]->setEnabled(1); allKControls[i]->setEnabled(0); allKDescriptions[i]->setEnabled(1); break;
			default: allKChannels[i]->setEnabled(1); allKControls[i]->setEnabled(1); allKDescriptions[i]->setEnabled(1); break;
			}

	// button page
	QList<QComboBox *> allBModes = tabWidget->findChildren<QComboBox *>(QRegExp("^b_mode_"));
	QList<QComboBox *> allBColors = tabWidget->findChildren<QComboBox *>(QRegExp("^b_color_"));
	QList<QSpinBox *> allBChannels = tabWidget->findChildren<QSpinBox *>(QRegExp("^b_channel_"));
	QList<QSpinBox *> allBControls = tabWidget->findChildren<QSpinBox *>(QRegExp("^b_CC_"));
	QList<QLineEdit *> allBDescriptions = tabWidget->findChildren<QLineEdit *>(QRegExp("^b_description_"));
	for(int i=0; i<allBModes.count();i++)
		switch(allBModes[i]->currentIndex())
			{
			case 0: allBChannels[i]->setEnabled(0); allBControls[i]->setEnabled(0); allBColors[i]->setEnabled(0); allBDescriptions[i]->setEnabled(0); break; 
			case 1: allBChannels[i]->setEnabled(1); allBControls[i]->setEnabled(1); allBColors[i]->setEnabled(1); allBDescriptions[i]->setEnabled(1); break;
			case 2: allBChannels[i]->setEnabled(1); allBControls[i]->setEnabled(1); allBColors[i]->setEnabled(1); allBDescriptions[i]->setEnabled(1); break;
			case 3: allBChannels[i]->setEnabled(1); allBControls[i]->setEnabled(1); allBColors[i]->setEnabled(1); allBDescriptions[i]->setEnabled(1); break;
			default: allBChannels[i]->setEnabled(1); allBControls[i]->setEnabled(1); allBColors[i]->setEnabled(1); allBDescriptions[i]->setEnabled(1); break;
			}

	// pedals page
	switch(p_mode_cont_1->currentIndex())
		{
		case 0: labelPCC_1->hide(); labelPCH_1->hide(); p_CC_cont_1->hide(); p_channel_cont_1->hide(); p_range_1->hide(); labelPRange_1->hide(); labelPMin_1->hide(); labelPMax_1->hide(); break; 
		case 1: labelPCC_1->hide(); labelPCH_1->show(); p_CC_cont_1->hide(); p_channel_cont_1->show(); p_range_1->show(); labelPRange_1->show(); labelPMin_1->show(); labelPMax_1->show(); break;
		case 2: labelPCC_1->show(); labelPCH_1->show(); p_CC_cont_1->show(); p_channel_cont_1->show(); p_range_1->show(); labelPRange_1->show(); labelPMin_1->show(); labelPMax_1->show(); break;
		}

	switch(p_controlmode_tip_1->currentIndex())
		{
		case 0: labelPCC_tip_1->hide(); labelPCH_tip_1->hide(); p_CC_tip_1->hide(); p_channel_tip_1->hide(); break;
		case 1: labelPCC_tip_1->hide(); labelPCH_tip_1->show(); p_CC_tip_1->hide(); p_channel_tip_1->show(); break;
		case 2: labelPCC_tip_1->show(); labelPCH_tip_1->show(); p_CC_tip_1->show(); p_channel_tip_1->show(); break;
		}

	switch(p_controlmode_ring_1->currentIndex())
		{
		case 0: labelPCC_ring_1->hide(); labelPCH_ring_1->hide(); p_CC_ring_1->hide(); p_channel_ring_1->hide(); break;
		case 1: labelPCC_ring_1->hide(); labelPCH_ring_1->show(); p_CC_ring_1->hide(); p_channel_ring_1->show(); break;
		case 2: labelPCC_ring_1->show(); labelPCH_ring_1->show(); p_CC_ring_1->show(); p_channel_ring_1->show(); break;
		}

	switch(p_switchmode_tip_1->currentIndex())
		{
		case 0: p_off_tip_1->show(); p_on_tip_1->show(); p_step_tip_1->hide(); p_wrap_tip_1->hide(); p_label_step_tip_1->hide(); p_label_off_tip_1->setText("off value"); p_label_on_tip_1->setText("on value");  break; // gate
		case 1: p_off_tip_1->show(); p_on_tip_1->show(); p_step_tip_1->show(); p_wrap_tip_1->show(); p_label_step_tip_1->show(); p_label_off_tip_1->setText("min"); p_label_on_tip_1->setText("max"); break; // inc
		case 2: p_off_tip_1->hide(); p_on_tip_1->show(); p_step_tip_1->hide(); p_wrap_tip_1->hide(); p_label_step_tip_1->hide(); p_label_off_tip_1->setText(""); p_label_on_tip_1->setText("value"); break; // trigger
		case 3: p_off_tip_1->show(); p_on_tip_1->show(); p_step_tip_1->hide(); p_wrap_tip_1->hide(); p_label_step_tip_1->hide(); p_label_off_tip_1->setText("off value"); p_label_on_tip_1->setText("on value"); break; // toggle
		}

	switch(p_switchmode_ring_1->currentIndex())
		{
		case 0: p_off_ring_1->show(); p_on_ring_1->show(); p_step_ring_1->hide(); p_wrap_ring_1->hide(); p_label_step_ring_1->hide(); p_label_off_ring_1->setText("off value"); p_label_on_ring_1->setText("on value");  break; // gate
		case 1: p_off_ring_1->show(); p_on_ring_1->show(); p_step_ring_1->show(); p_wrap_ring_1->show(); p_label_step_ring_1->show(); p_label_off_ring_1->setText("min"); p_label_on_ring_1->setText("max"); break; // inc
		case 2: p_off_ring_1->hide(); p_on_ring_1->show(); p_step_ring_1->hide(); p_wrap_ring_1->hide(); p_label_step_ring_1->hide(); p_label_off_ring_1->setText(""); p_label_on_ring_1->setText("value"); break; // trigger
		case 3: p_off_ring_1->show(); p_on_ring_1->show(); p_step_ring_1->hide(); p_wrap_ring_1->hide(); p_label_step_ring_1->hide(); p_label_off_ring_1->setText("off value"); p_label_on_ring_1->setText("on value"); break; // toggle
		}

        switch(p_mode_cont_2->currentIndex())
                {
                case 0: labelPCC_2->hide(); labelPCH_2->hide(); p_CC_cont_2->hide(); p_channel_cont_2->hide(); p_range_2->hide(); labelPRange_2->hide(); labelPMin_2->hide(); labelPMax_2->hide(); break;
                case 1: labelPCC_2->hide(); labelPCH_2->show(); p_CC_cont_2->hide(); p_channel_cont_2->show(); p_range_2->show(); labelPRange_2->show(); labelPMin_2->show(); labelPMax_2->show(); break;
                case 2: labelPCC_2->show(); labelPCH_2->show(); p_CC_cont_2->show(); p_channel_cont_2->show(); p_range_2->show(); labelPRange_2->show(); labelPMin_2->show(); labelPMax_2->show(); break;
                }

        switch(p_controlmode_tip_2->currentIndex())
                {
                case 0: labelPCC_tip_2->hide(); labelPCH_tip_2->hide(); p_CC_tip_2->hide(); p_channel_tip_2->hide(); break;
                case 1: labelPCC_tip_2->hide(); labelPCH_tip_2->show(); p_CC_tip_2->hide(); p_channel_tip_2->show(); break;
                case 2: labelPCC_tip_2->show(); labelPCH_tip_2->show(); p_CC_tip_2->show(); p_channel_tip_2->show(); break;
                }

        switch(p_controlmode_ring_2->currentIndex())
                {
                case 0: labelPCC_ring_2->hide(); labelPCH_ring_2->hide(); p_CC_ring_2->hide(); p_channel_ring_2->hide(); break;
                case 1: labelPCC_ring_2->hide(); labelPCH_ring_2->show(); p_CC_ring_2->hide(); p_channel_ring_2->show(); break;
                case 2: labelPCC_ring_2->show(); labelPCH_ring_2->show(); p_CC_ring_2->show(); p_channel_ring_2->show(); break;
                }

        switch(p_switchmode_tip_2->currentIndex())
                {
                case 0: p_off_tip_2->show(); p_on_tip_2->show(); p_step_tip_2->hide(); p_wrap_tip_2->hide(); p_label_step_tip_2->hide(); p_label_off_tip_2->setText("off value"); p_label_on_tip_2->setText("on value");  break; // gate
                case 1: p_off_tip_2->show(); p_on_tip_2->show(); p_step_tip_2->show(); p_wrap_tip_2->show(); p_label_step_tip_2->show(); p_label_off_tip_2->setText("min"); p_label_on_tip_2->setText("max"); break; // inc
                case 2: p_off_tip_2->hide(); p_on_tip_2->show(); p_step_tip_2->hide(); p_wrap_tip_2->hide(); p_label_step_tip_2->hide(); p_label_off_tip_2->setText(""); p_label_on_tip_2->setText("value"); break; // trigger
                case 3: p_off_tip_2->show(); p_on_tip_2->show(); p_step_tip_2->hide(); p_wrap_tip_2->hide(); p_label_step_tip_2->hide(); p_label_off_tip_2->setText("off value"); p_label_on_tip_2->setText("on value"); break; // toggle
                }

        switch(p_switchmode_ring_2->currentIndex())
                {
                case 0: p_off_ring_2->show(); p_on_ring_2->show(); p_step_ring_2->hide(); p_wrap_ring_2->hide(); p_label_step_ring_2->hide(); p_label_off_ring_2->setText("off value"); p_label_on_ring_2->setText("on value");  break; // gate
                case 1: p_off_ring_2->show(); p_on_ring_2->show(); p_step_ring_2->show(); p_wrap_ring_2->show(); p_label_step_ring_2->show(); p_label_off_ring_2->setText("min"); p_label_on_ring_2->setText("max"); break; // inc
                case 2: p_off_ring_2->hide(); p_on_ring_2->show(); p_step_ring_2->hide(); p_wrap_ring_2->hide(); p_label_step_ring_2->hide(); p_label_off_ring_2->setText(""); p_label_on_ring_2->setText("value"); break; // trigger
                case 3: p_off_ring_2->show(); p_on_ring_2->show(); p_step_ring_2->hide(); p_wrap_ring_2->hide(); p_label_step_ring_2->hide(); p_label_off_ring_2->setText("off value"); p_label_on_ring_2->setText("on value"); break; // toggle
                }

}

void qkontrolWindow::updateValues()
{
	unsigned char buf[91];
	res = 0;

	res = hid_read(handle,buf,91);

	QByteArray DATA_IN;
	DATA_IN = QByteArray(reinterpret_cast<char*>(buf), res);
	if((res == 51) && (DATA_IN[0]==char(0xaa)))
		{
		QList<QComboBox *> allModes = tabWidget->findChildren<QComboBox *>(QRegExp("^k_mode_"));
		QList<int> x, y, dis; // startpoint coordinates and destination display for CC value illustration
		x << 90 << 108 << 180 << 798 << 90 << 108 << 180 << 798;
		y << 309 << 309 << 309 << 306 << 309 << 309 << 309 << 306;
		dis << 0 << 0 << 0 << 0 << 1 << 1 << 1 << 1;

		QPixmap knobValue(32, 18);
		QPainter *knobPainter = new QPainter(&knobValue);
		knobPainter->setFont(QFont("Arial", 14, QFont::Bold));
		knobPainter->setPen(allColors["value"]);
		knobValue.fill(Qt::black);
		for(int i=0;i<=7;i++)
			if((findChild<QComboBox *>("k_mode_"+QString::number(8*kontrolPage+i+1))->currentIndex() != 0) && (DATA_IN[17+i*2] != knobsButtons[17+i*2]))
				{
				knobValue.fill(Qt::black);
				knobPainter->drawText(QRect(0, 0, 30, 18), Qt::AlignRight, QString::number(DATA_IN[17+i*2]));
				drawImage(dis[i], &knobValue, x[i], y[i]);
				}
		knobPainter->end();
		knobsButtons = DATA_IN;
		}
	if((res == 32) && (DATA_IN[0]==char(0x01)))
		{
		if(DATA_IN[2]==char(0x10))
			qDebug() << "play";
		if(DATA_IN[3]==char(0x02))
			qDebug() << "rec";
		if(DATA_IN[3]==char(0x01))
			qDebug() << "stop";
		if(DATA_IN[3]==char(0x10)) // preset up
			zapPreset(0);
		if(DATA_IN[3]==char(0x40)) // preset down
			zapPreset(1);
		if((DATA_IN[3]==char(0x80)) && kontrolPage > 0)
			setKontrolpage(kontrolPage-1);
		if((DATA_IN[3]==char(0x20)) && kontrolPage < 3)
			setKontrolpage(kontrolPage+1);
		}
}

QString qkontrolWindow::getControlName(uint8_t CC)
{

	switch(CC)
		{
		case 0: return "bank"; break;
		case 1: return "modulation"; break;
		case 2: return "breath"; break;
		case 4: return "foot"; break;
		case 5: return "portamento time"; break;
		case 7: return "volume"; break;
		case 8: return "balance"; break;
		case 10: return "panning"; break;
		case 11: return "expression"; break;
		case 12: return "effect 1"; break;
		case 13: return "effect 2"; break;
		case 64: return "pedal"; break;
		case 65: return "portamento"; break;
		case 66: return "sostenuto"; break;
		case 67: return "soft pedal"; break;
		case 68: return "legato"; break;
		case 69: return "hold 2"; break;
		case 70: return "variation"; break;
		case 71: return "resonance"; break;
		case 72: return "release"; break;
		case 73: return "attack"; break;
		case 74: return "cutoff"; break;
		default: return "custom"; break;
		}
}

void qkontrolWindow::setKeyzones()
{
	QList<QSpinBox *> allKeys = tabWidget->findChildren<QSpinBox *>(QRegExp("^key_"));
	QList<QSpinBox *> allChannels = tabWidget->findChildren<QSpinBox *>(QRegExp("^channel_"));
	QList<QCheckBox *> allOff = tabWidget->findChildren<QCheckBox *>(QRegExp("^k_off_"));
	QList<QSpinBox *> allSChannels = tabWidget->findChildren<QSpinBox *>(QRegExp("^s_channel_"));
	QList<QSpinBox *> allSControls = tabWidget->findChildren<QSpinBox *>(QRegExp("^s_CC_"));
	QList<QToolBox *> allSToolboxes = tabWidget->findChildren<QToolBox *>(QRegExp("^s_toolbox"));
	QList<QxtSpanSlider *> allRanges = tabWidget->findChildren<QxtSpanSlider *>(QRegExp("^s_minmax_"));

	QByteArray mapping, knobsAndButtons, sliders, pedals, port_1, port_2;
	QStringList sliderFunctionList;
	sliderFunctionList << "pitch wheel" << "mod wheel" << "touch strip";
	mapping.append(QByteArray::fromHex("a4"));
	for(int i=0;i<=15;i++)
		{
		mapping.append(allKeys[i]->value());
		mapping.append(QByteArray::fromHex("00"));
		mapping.append(allChannels[i]->value()-1);
		if(allOff[i]->isChecked())
			mapping.append(QByteArray::fromHex("83")); // off
		else
			{
			switch(findChild<QComboBox *>("velocity_"+QString::number(i+1))->currentIndex())
				{
				case 0: mapping.append(QByteArray::fromHex("30")); break; // soft 3
				case 1: mapping.append(QByteArray::fromHex("31")); break; // soft 2
				case 2: mapping.append(QByteArray::fromHex("32")); break; // soft 1
				case 3: mapping.append(QByteArray::fromHex("33")); break; // linear
				case 4: mapping.append(QByteArray::fromHex("34")); break; // hard 1
				case 5: mapping.append(QByteArray::fromHex("35")); break; // hard 2
				case 6: mapping.append(QByteArray::fromHex("36")); break; // hard 3
				}
			}
		switch(findChild<QComboBox *>("color_"+QString::number(i+1))->currentIndex())
			{
			case 0: mapping.append(QByteArray::fromHex("2c2e")); break; // blue
			case 1: mapping.append(QByteArray::fromHex("0406")); break; // red
			case 2: mapping.append(QByteArray::fromHex("080a")); break; // orange
			case 3: mapping.append(QByteArray::fromHex("1c1e")); break; // green
			case 4: mapping.append(QByteArray::fromHex("1416")); break; // yellow
			case 5: mapping.append(QByteArray::fromHex("2022")); break; // mint
			case 6: mapping.append(QByteArray::fromHex("383a")); break; // purple
			case 7: mapping.append(QByteArray::fromHex("2426")); break; // cyan
			case 8: mapping.append(QByteArray::fromHex("0000")); break; // black
			}
		mapping.append(QByteArray::fromHex("0000"));
		}

	res = hid_write(handle, (unsigned char*) mapping.constData(), mapping.count());
	

	knobsAndButtons.append(QByteArray::fromHex("a1"));
	for(unsigned int i=kontrolPage*8;i<=kontrolPage*8+7;i++) // buttons
		{
		switch(findChild<QComboBox *>("b_mode_"+QString::number(i+1))->currentIndex())
		{
			case 0: knobsAndButtons.append(QByteArray::fromHex("00")); break;
			case 4: knobsAndButtons.append(QByteArray::fromHex("04")); break;
			default: knobsAndButtons.append(QByteArray::fromHex("03")); break;
		}
		knobsAndButtons.append(findChild<QSpinBox *>("b_CC_"+QString::number(i+1))->value());
		knobsAndButtons.append(findChild<QSpinBox *>("b_channel_"+QString::number(i+1))->value()-1);
 		switch(findChild<QComboBox *>("b_mode_"+QString::number(i+1))->currentIndex())
			{
			case 1: knobsAndButtons.append(QByteArray::fromHex("3c")); break; // toggle
			case 3: knobsAndButtons.append(QByteArray::fromHex("3e")); break; // gate
			default: knobsAndButtons.append(QByteArray::fromHex("3d")); break; // any other
			}
		knobsAndButtons.append(QByteArray::fromHex("0000"));
		if(findChild<QComboBox *>("b_mode_"+QString::number(i+1))->currentIndex()==4)
			knobsAndButtons.append(findChild<QSpinBox *>("b_CC_"+QString::number(i+1))->value());
		else
			knobsAndButtons.append(QByteArray::fromHex("7f"));
		knobsAndButtons.append(QByteArray::fromHex("0000000000"));
		}
	for(unsigned int i=kontrolPage*8;i<=kontrolPage*8+7;i++) // knobs
		{
                switch(findChild<QComboBox *>("k_mode_"+QString::number(i+1))->currentIndex())
                        {
			case 0: knobsAndButtons.append(QByteArray::fromHex("00")); break;
			case 1: knobsAndButtons.append(QByteArray::fromHex("04")); break;
			default: knobsAndButtons.append(QByteArray::fromHex("03")); break;
                        }
		knobsAndButtons.append(findChild<QSpinBox *>("k_CC_"+QString::number(i+1))->value());
		knobsAndButtons.append(findChild<QSpinBox *>("k_channel_"+QString::number(i+1))->value()-1);
		knobsAndButtons.append(QByteArray::fromHex("3c00007f0000000000"));
		}
	for(unsigned int i=kontrolPage*8;i<=kontrolPage*8+7;i++) // background light of the buttons
		{
		switch(findChild<QComboBox *>("b_color_"+QString::number(i+1))->currentIndex())
			{
			case 0: knobsAndButtons.append(QByteArray::fromHex("00")); break; // off
			case 1: knobsAndButtons.append(QByteArray::fromHex("1f")); break; // white
			case 2: knobsAndButtons.append(QByteArray::fromHex("01")); break; // red
			case 3: knobsAndButtons.append(QByteArray::fromHex("0a")); break; // blue
			case 4: knobsAndButtons.append(QByteArray::fromHex("03")); break; // orange
			case 5: knobsAndButtons.append(QByteArray::fromHex("09")); break; // cyan
			case 6: knobsAndButtons.append(QByteArray::fromHex("07")); break; // green
			case 7: knobsAndButtons.append(QByteArray::fromHex("0c")); break; // violet
			case 8: knobsAndButtons.append(QByteArray::fromHex("05")); break; // yellow
			case 9: knobsAndButtons.append(QByteArray::fromHex("0e")); break; // magenta
			case 10: knobsAndButtons.append(QByteArray::fromHex("08")); break; // mint
			case 11: knobsAndButtons.append(QByteArray::fromHex("0d")); break; // purple
			case 12: knobsAndButtons.append(QByteArray::fromHex("10")); break; // pink
			}
		}
	knobsAndButtons.append(QByteArray::fromHex("000000")); // suffix-data, always the same
	res = hid_write(handle, (unsigned char*) knobsAndButtons.constData(), knobsAndButtons.count());

	sliders.append(QByteArray::fromHex("a2"));

	for(int i=0;i<=2;i++) // pitch, mod wheel and touchstrip
		{
		switch(allSToolboxes[i]->currentIndex())
			{
			case 0: //off
				{
				sliders.append(QByteArray::fromHex("000000000000000000000000"));
				sliderFunctionList[i].append(" is off");
				break;
				}
			case 1: // ctrl change
				{
				sliders.append(QByteArray::fromHex("03"));
				sliders.append(allSControls[i]->value());
				sliders.append(allSChannels[i]->value()-1);
				sliders.append(QByteArray::fromHex("20"));
				sliders.append(allRanges[i]->lowerValue());
				sliders.append(QByteArray::fromHex("00"));
				sliders.append(allRanges[i]->upperValue());
				sliders.append(QByteArray::fromHex("0000000000"));
				sliderFunctionList[i].append(" sends CC "+QString::number(allSControls[i]->value()));
				break;
				}
			case 2: // pitch
				{
				sliders.append(QByteArray::fromHex("0600"));
				sliders.append(allSChannels[i]->value()-1);
				sliders.append(QByteArray::fromHex("000000ff3f00000100"));
				sliderFunctionList[i].append(" sends pitch");
				break;
				}
			}
		}
	if(allSToolboxes[2]->currentIndex() == 2) // set PB range and strip LED zero point to 50% if touchstrip is used for pitching
		{
		sliders.append(8-s_range_t->value());
		sliders.append(QByteArray::fromHex("0000"));
		sliders.append(QByteArray::fromHex("02"));
		}
	else
		sliders.append(QByteArray::fromHex("00000000"));
	sliders.append(QByteArray::fromHex("00000000"));

	res = hid_write(handle, (unsigned char*) sliders.constData(), sliders.count());

	// declare which pedal hardware is connected to the pedal ports (pedals or switches?)

	// port 1
	port_1.append("f4220103");
	if(radioButton_cont_1->isChecked()) // 02=continous, 03=switch (pedal 1), 06=continous (invert), 01=continous swap t/r), 05=continous swap invert
		{
		if((!(checkBox_swap_1->isChecked())) && (!(checkBox_invert_1->isChecked())))
			port_1.append(QByteArray::fromHex("02"));
		if((checkBox_swap_1->isChecked()) && (!(checkBox_invert_1->isChecked())))
			port_1.append(QByteArray::fromHex("01"));
		if((!(checkBox_swap_1->isChecked())) && (checkBox_invert_1->isChecked()))
			port_1.append(QByteArray::fromHex("06"));
		if((checkBox_swap_1->isChecked()) && (checkBox_invert_1->isChecked()))
			port_1.append(QByteArray::fromHex("05"));
		}
	else
		port_1.append(QByteArray::fromHex("03"));
	port_1.append("00000000000000000000000000000000000000000000000000000000");

	res = hid_write(handle, (unsigned char*) port_1.constData(), port_1.count());

	// port 2
	port_2.append("f4220003");
	if(radioButton_cont_2->isChecked()) // 02=continous, 03=switch (pedal 1), 06=continous (invert), 01=continous swap t/r), 05=continous swap invert
                {
                if((!(checkBox_swap_2->isChecked())) && (!(checkBox_invert_2->isChecked())))
                        port_2.append(QByteArray::fromHex("02"));
                if((checkBox_swap_2->isChecked()) && (!(checkBox_invert_2->isChecked())))
                        port_2.append(QByteArray::fromHex("01"));
                if((!(checkBox_swap_2->isChecked())) && (checkBox_invert_2->isChecked()))
                        port_2.append(QByteArray::fromHex("06"));
                if((checkBox_swap_2->isChecked()) && (checkBox_invert_2->isChecked()))
                        port_2.append(QByteArray::fromHex("05"));
                }
        else
                port_2.append(QByteArray::fromHex("03"));
        port_2.append("00000000000000000000000000000000000000000000000000000000");

	res = hid_write(handle, (unsigned char*) port_2.constData(), port_2.count());

	// transmit the pedal and switch parameters

	pedals.append(QByteArray::fromHex("a3"));
	
	// pedal 1 continous mode
	switch(p_mode_cont_1->currentIndex())
		{
		case 0: pedals.append(QByteArray::fromHex("00")); break;
		case 1: pedals.append(QByteArray::fromHex("04")); break;
		case 2: pedals.append(QByteArray::fromHex("03")); break;
		}
	pedals.append(p_CC_cont_1->value());
	pedals.append(p_channel_cont_1->value()-1);
	pedals.append(QByteArray::fromHex("94"));
	pedals.append(p_range_1->lowerValue());
	pedals.append(QByteArray::fromHex("00"));
	pedals.append(p_range_1->upperValue());
	pedals.append(QByteArray::fromHex("00"));
	pedals.append(QByteArray::fromHex("6050f1ac"));

	// pedal 2 continous mode
	switch(p_mode_cont_2->currentIndex())
		{
		case 0: pedals.append(QByteArray::fromHex("00")); break;
                case 1: pedals.append(QByteArray::fromHex("04")); break;
                case 2: pedals.append(QByteArray::fromHex("03")); break;
       		}
	pedals.append(p_CC_cont_2->value());
	pedals.append(p_channel_cont_2->value()-1);
	pedals.append(QByteArray::fromHex("00"));
	pedals.append(p_range_2->lowerValue());
	pedals.append(QByteArray::fromHex("00"));
	pedals.append(p_range_2->upperValue());
	pedals.append(QByteArray::fromHex("00"));
	pedals.append(QByteArray::fromHex("c0600000"));

	// pedal 1 switch tip
	switch(p_controlmode_tip_1->currentIndex())
		{
		case 0: pedals.append(QByteArray::fromHex("00")); break;
		case 1: pedals.append(QByteArray::fromHex("04")); break;
		case 2: pedals.append(QByteArray::fromHex("03")); break;
		}
	pedals.append(p_CC_tip_1->value());
	pedals.append(p_channel_tip_1->value()-1);
	switch(p_switchmode_tip_1->currentIndex()) // (36=gate, 37=inc, 35=trigger, 34=toggle, 3F=inc Wrapped)
		{
		case 0: pedals.append(QByteArray::fromHex("36")); break;
		case 1:
			{
			if(p_wrap_tip_1->isChecked())
				pedals.append(QByteArray::fromHex("3f"));
			else
				pedals.append(QByteArray::fromHex("37"));
			break;
			}
		case 2: pedals.append(QByteArray::fromHex("35")); break;
		case 3: pedals.append(QByteArray::fromHex("34")); break;
		}
	pedals.append(p_off_tip_1->value());
	pedals.append(QByteArray::fromHex("00"));
	pedals.append(p_on_tip_1->value());
	pedals.append(QByteArray::fromHex("00"));
	pedals.append(QByteArray::fromHex("0000"));
	if(p_switchmode_tip_1->currentIndex()==1)
		pedals.append(p_step_tip_1->value());
	else
		pedals.append(QByteArray::fromHex("00"));
	pedals.append(QByteArray::fromHex("00"));

        // pedal 1 switch ring
        switch(p_controlmode_ring_1->currentIndex())
                {
                case 0: pedals.append(QByteArray::fromHex("00")); break;
                case 1: pedals.append(QByteArray::fromHex("04")); break;
                case 2: pedals.append(QByteArray::fromHex("03")); break;
                }
        pedals.append(p_CC_ring_1->value());
        pedals.append(p_channel_ring_1->value()-1);
        switch(p_switchmode_ring_1->currentIndex()) // (36=gate, 37=inc, 35=trigger, 34=toggle, 3F=inc Wrapped)
                {
                case 0: pedals.append(QByteArray::fromHex("36")); break;
                case 1:
                        {
                        if(p_wrap_ring_1->isChecked())
                                pedals.append(QByteArray::fromHex("3f"));
                        else
                                pedals.append(QByteArray::fromHex("37"));
                        break;
                        }
                case 2: pedals.append(QByteArray::fromHex("35")); break;
                case 3: pedals.append(QByteArray::fromHex("34")); break;
                }
        pedals.append(p_off_ring_1->value());
        pedals.append(QByteArray::fromHex("00"));
        pedals.append(p_on_ring_1->value());
        pedals.append(QByteArray::fromHex("00"));
        pedals.append(QByteArray::fromHex("0000"));
        if(p_switchmode_ring_1->currentIndex()==1)
                pedals.append(p_step_ring_1->value());
        else
                pedals.append(QByteArray::fromHex("00"));
        pedals.append(QByteArray::fromHex("00"));

        // pedal 2 switch tip
        switch(p_controlmode_tip_2->currentIndex())
                {
                case 0: pedals.append(QByteArray::fromHex("00")); break;
                case 1: pedals.append(QByteArray::fromHex("04")); break;
                case 2: pedals.append(QByteArray::fromHex("03")); break;
                }
        pedals.append(p_CC_tip_2->value());
        pedals.append(p_channel_tip_2->value()-1);
        switch(p_switchmode_tip_2->currentIndex()) // (36=gate, 37=inc, 35=trigger, 34=toggle, 3F=inc Wrapped)
                {
                case 0: pedals.append(QByteArray::fromHex("36")); break;
                case 1:
                        {
                        if(p_wrap_tip_2->isChecked())
                                pedals.append(QByteArray::fromHex("3f"));
                        else
                                pedals.append(QByteArray::fromHex("37"));
                        break;
                        }
                case 2: pedals.append(QByteArray::fromHex("35")); break;
                case 3: pedals.append(QByteArray::fromHex("34")); break;
                }
        pedals.append(p_off_tip_2->value());
        pedals.append(QByteArray::fromHex("00"));
        pedals.append(p_on_tip_2->value());
        pedals.append(QByteArray::fromHex("00"));
        pedals.append(QByteArray::fromHex("0000"));
        if(p_switchmode_tip_2->currentIndex()==1)
                pedals.append(p_step_tip_2->value());
        else
                pedals.append(QByteArray::fromHex("00"));
        pedals.append(QByteArray::fromHex("00"));

        // pedal 2 switch ring
        switch(p_controlmode_ring_2->currentIndex())
                {
                case 0: pedals.append(QByteArray::fromHex("00")); break;
                case 1: pedals.append(QByteArray::fromHex("04")); break;
                case 2: pedals.append(QByteArray::fromHex("03")); break;
                }
        pedals.append(p_CC_ring_2->value());
        pedals.append(p_channel_ring_2->value()-1);
        switch(p_switchmode_ring_2->currentIndex()) // (36=gate, 37=inc, 35=trigger, 34=toggle, 3F=inc Wrapped)
                {
                case 0: pedals.append(QByteArray::fromHex("36")); break;
                case 1:
                        {
                        if(p_wrap_ring_2->isChecked())
                                pedals.append(QByteArray::fromHex("3f"));
                        else
                                pedals.append(QByteArray::fromHex("37"));
                        break;
                        }
                case 2: pedals.append(QByteArray::fromHex("35")); break;
                case 3: pedals.append(QByteArray::fromHex("34")); break;
                }
        pedals.append(p_off_ring_2->value());
        pedals.append(QByteArray::fromHex("00"));
        pedals.append(p_on_ring_2->value());
        pedals.append(QByteArray::fromHex("00"));
        pedals.append(QByteArray::fromHex("0000"));
        if(p_switchmode_ring_2->currentIndex()==1)
                pedals.append(p_step_ring_2->value());
        else
		pedals.append(QByteArray::fromHex("00"));
	pedals.append(QByteArray::fromHex("00"));

	res = hid_write(handle, (unsigned char*) pedals.constData(), pedals.count());

	// print the selected Midi CC numbers on the screens
	QPixmap screen1(480, 272);
	QPixmap screen2(480, 272);
	QPainter *image1 = new QPainter(&screen1); 
	QPainter *image2 = new QPainter(&screen2);
	QVector<QPainter*> image;
	image.push_back(image1);
	image.push_back(image2);

	screen1.fill(Qt::black);
	screen2.fill(Qt::black);

	if(QFile::exists(graphicsViewScreen1->currentFile))
		image1->drawImage(0, 65, QImage(graphicsViewScreen1->currentFile).scaled(480, 140));
	if(QFile::exists(graphicsViewScreen2->currentFile))
		image2->drawImage(0, 65, QImage(graphicsViewScreen2->currentFile).scaled(480, 140));
	if(p_ScreenCC->currentIndex() == 1)
		{
		image1->setFont(QFont("Arial", 16, QFont::Bold));
		image1->setPen(allColors["slider"]);
		image1->drawText(QPoint(30,110), sliderFunctionList[0]);
		image1->drawText(QPoint(30,140), sliderFunctionList[1]);
		image1->drawText(QPoint(30,170), sliderFunctionList[2]);
		image1->drawText(QPoint(370, 140), "page "+QString::number(kontrolPage+1)+"/4");
		}
	if(p_ScreenCC->currentIndex() == 2)
		{
		image2->setFont(QFont("Arial", 16, QFont::Bold));
		image2->setPen(allColors["slider"]);
		image2->drawText(QPoint(30,110), sliderFunctionList[0]);
		image2->drawText(QPoint(30,140), sliderFunctionList[1]);
		image2->drawText(QPoint(30,170), sliderFunctionList[2]);
		image2->drawText(QPoint(370, 140), "page "+QString::number(kontrolPage+1)+"/4");
		}

	image1->setFont(QFont("Arial", 13, QFont::Bold));
	image1->setPen(allColors["CC"]);
	image2->setFont(QFont("Arial", 13, QFont::Bold));
	image2->setPen(allColors["CC"]);

	QList<int> y;
	y << 10 << 130 << 250 << 370 << 10 << 130 << 250 << 370;
	
	for(int i=0;i<=7;i++)
		{
		switch(findChild<QComboBox *>("b_mode_"+QString::number(8*kontrolPage+i+1))->currentIndex())
			{
			case 0: image[floor(i/4)]->drawText(QRect(y[i], 10, 100, 27), Qt::AlignCenter, "OFF"); break;
			case 4: image[floor(i/4)]->drawText(QRect(y[i], 10, 100, 27), Qt::AlignCenter, "PRG "+QString::number(findChild<QSpinBox *>("b_CC_"+QString::number(8*kontrolPage+i+1))->value())); break;
			default: image[floor(i/4)]->drawText(QRect(y[i], 10, 100, 27), Qt::AlignCenter, "CC "+QString::number(findChild<QSpinBox *>("b_CC_"+QString::number(8*kontrolPage+i+1))->value())); break;
			}
		}

	image1->setFont(QFont("Arial", 10));
	image2->setFont(QFont("Arial", 10));

	for(int i=0;i<=7;i++)
		switch(findChild<QComboBox *>("k_mode_"+QString::number(8*kontrolPage+i+1))->currentIndex())
			{
			case 0: image[floor(i/4)]->drawText(QPoint(y[i], 245), "OFF"); break;
			case 1: image[floor(i/4)]->drawText(QPoint(y[i], 245), "PRESET"); break;
			default: image[floor(i/4)]->drawText(QPoint(y[i], 245), "CC "+QString::number(findChild<QSpinBox *>("k_CC_"+QString::number(8*kontrolPage+i+1))->value())); break;
			}

	image1->setFont(QFont("Arial", 9));
	image2->setFont(QFont("Arial", 9));

	image1->setPen(allColors["parameter"]);
	image2->setPen(allColors["parameter"]);

	for(int i=0;i<=7;i++)
		{
		if(findChild<QComboBox *>("k_mode_"+QString::number(8*kontrolPage+i+1))->currentIndex() == 2)
			image[floor(i/4)]->drawText(QPoint(y[i], 263), findChild<QLineEdit *>("k_description_"+QString::number(8*kontrolPage+i+1))->text());
		if(findChild<QComboBox *>("b_mode_"+QString::number(8*kontrolPage+i+1))->currentIndex() != 0)
			image[floor(i/4)]->drawText(QRect(y[i], 32, 100, 13), Qt::AlignCenter, findChild<QLineEdit *>("b_description_"+QString::number(8*kontrolPage+i+1))->text());
		}

	image1->setPen(allColors["divider"]);
	image2->setPen(allColors["divider"]);

	for(int i=0;i<=7;i++)
		image[floor(i/4)]->drawRect(y[i], 10, 100, 36);

	image1->drawLine(120, 225, 120, 272);
	image1->drawLine(240, 225, 240, 272);
	image1->drawLine(360, 225, 360, 272);

	image2->drawLine(120, 225, 120, 272);
	image2->drawLine(240, 225, 240, 272);
	image2->drawLine(360, 225, 360, 272);


	drawImage(0, &screen1);
	drawImage(1, &screen2);

	image1->end();
	image2->end();
}


void qkontrolWindow::drawImage(uint8_t screen, QPixmap *pixmap, ushort x, ushort y)
{
	QByteArray tux;

	QImage image = pixmap->toImage();
	image = image.convertToFormat(QImage::Format_RGB16);
	ushort *swappedData = reinterpret_cast<ushort *>(image.bits());
	tux.append(QByteArray::fromHex("8400"));
	tux.append(screen);
	tux.append(QByteArray::fromHex("6000000000"));
	tux.append(QByteArray::fromHex(QByteArray::number(x).rightJustified(4,'0')));
	tux.append(QByteArray::fromHex(QByteArray::number(y).rightJustified(4,'0')));
	tux.append(QByteArray::fromHex(QByteArray::number(image.width(),16).rightJustified(4,'0')));
	tux.append(QByteArray::fromHex(QByteArray::number(image.height(),16).rightJustified(4,'0')));

	tux.append(QByteArray::fromHex("020000000000"));
	tux.append(QByteArray::fromHex(QByteArray::number(image.width()*image.height()/2,16).rightJustified(4,'0')));
	for(int i=0;i<(image.width()*image.height());i++)
		tux.append(QByteArray::fromHex(QByteArray::number(swappedData[i],16).rightJustified(4,'0')));
	tux.append(QByteArray::fromHex("020000000300000040000000"));

//	libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices
        libusb_device_handle *dev_handle; //a device handle
        libusb_context *ctx = NULL; //a libusb session
        int r = 0; // any error will lower this value
//        ssize_t cnt; //holding number of devices in list
        r -= libusb_init(&ctx); //initialize the library for the session we just declared

//        cnt = libusb_get_device_list(ctx, &devs); //get the list of devices
//        if(cnt < 0) {
//                cout<<"Get Device Error"<<endl; //there was an error
//        }
//        cout<<cnt<<" Devices in list."<<endl;

	dev_handle = libusb_open_device_with_vid_pid(ctx, 0x17cc, pid); // vendor ID 0x17cc = Native Instruments, product ID was probed at startup
        if(dev_handle == NULL)
                cout<<"Cannot open device"<<endl;
//        libusb_free_device_list(devs, 1); //free the list, unref the devices in it

        int actual; //used to find out how many bytes were written
//        if(libusb_kernel_driver_active(dev_handle, 0) == 1) { //find out if kernel driver is attached
//                cout<<"Kernel Driver Active"<<endl;
//                if(libusb_detach_kernel_driver(dev_handle, 0) == 0) //detach it
//                        cout<<"Kernel Driver Detached!"<<endl;
//        }
        r -= libusb_claim_interface(dev_handle, 3); //claim interface 0 (the first) of device (mine had jsut 1)

	r -= libusb_bulk_transfer(dev_handle, 3, (unsigned char*) tux.constData(), tux.count(), &actual, 0);

        r -= libusb_release_interface(dev_handle, 0); //release the claimed interface
        if(r<0)
		QMessageBox::critical(this, "communication error", "The USB transmission of the bitmap data failed. Please restart your Komplete Kontrol device and try again");

        libusb_close(dev_handle); //close the device we opened
        libusb_exit(ctx); //needs to be called to end the
}

void qkontrolWindow::selectColor(QString target)
{
	QColor selection = QColorDialog::getColor();
	if(selection.isValid())
		allColors[target] = selection;
	updateColors();
}

// trampoline slots for color buttons
void qkontrolWindow::setSlidertextcolor() { selectColor("slider"); }
void qkontrolWindow::setCCtextcolor() { selectColor("CC"); }
void qkontrolWindow::setParametertextcolor() { selectColor("parameter"); }
void qkontrolWindow::setDividercolor() { selectColor("divider"); }
void qkontrolWindow::setValuetextcolor() { selectColor("value"); }

void qkontrolWindow::updateColors()
{
	QPixmap pixmapColors(color_CC->width()-4, color_CC->height()-4);
	pixmapColors.fill(allColors["CC"]);
	color_CC->setIcon(pixmapColors);
	pixmapColors.fill(allColors["slider"]);
	color_sliders->setIcon(pixmapColors);
	pixmapColors.fill(allColors["parameter"]);
	color_parameters->setIcon(pixmapColors);
	pixmapColors.fill(allColors["divider"]);
	color_dividers->setIcon(pixmapColors);
	pixmapColors.fill(allColors["value"]);
	color_values->setIcon(pixmapColors);
}

qkontrolWindow::~qkontrolWindow()
{
	res = hid_exit();
}

// save XML file of all the needed form values into the user directory
bool qkontrolWindow::save() 
	{ 
	// find out what necessary values do exist... if you want to use new widgets in the UI, you need to modify this function
	QList<QSpinBox *> allSpinBoxes = this->findChildren<QSpinBox *>();
	QList<QCheckBox *> allCheckBoxes = this->findChildren<QCheckBox *>();
	QList<QRadioButton *> allRadioButtons = this->findChildren<QRadioButton *>();
	QList<QSlider *> allSliders = this->findChildren<QSlider *>();
	QList<QxtSpanSlider *> allSpansliders = this->findChildren<QxtSpanSlider *>();
	QList<QComboBox *> allComboboxes = this->findChildren<QComboBox *>();
	QList<QToolBox *> allToolboxes = this->findChildren<QToolBox *>();
	QList<QTabWidget *> allTabwidgets = this->findChildren<QTabWidget *>();
	QList<QLineEdit *> allLineedits = this->findChildren<QLineEdit *>(QRegExp("_description_"));

	// create template XML file (actually static, may be dynamized in future versions)
	QFile file(QFileDialog::getSaveFileName(this, "choose a place to save this configuration!",QDir::homePath(),"QCP files (*.qcp)",0));
	if(QFileInfo(file).suffix() != "qcp")
		file.setFileName(file.fileName()+".qcp");
	file.open(QIODevice::WriteOnly);
	
	// write XML header
	file.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
 	file.write("<qkontrol VERSION=\"1.0\">\n");

	// create arrays with widget contents for each widget type
	// -> spinboxes with integer values
	file.write("\t<SpinBoxes>\n");
	for(int ii = 0; ii < allSpinBoxes.size(); ++ii)
		file.write(QByteArray().append("\t\t<"+allSpinBoxes[ii]->objectName()+">"+QString("%1").arg(allSpinBoxes[ii]->value(),0,10)+"</"+allSpinBoxes[ii]->objectName()+">\n"));
	file.write("\t</SpinBoxes>\n");
	// -> checkboxes
	file.write("\t<CheckBoxes>\n");
	for(int ii = 0; ii < allCheckBoxes.size(); ++ii)
		{
		file.write(QByteArray().append("\t\t<"+allCheckBoxes[ii]->objectName()+">"));
		if(allCheckBoxes[ii]->isChecked())
			file.write("true");
		else
			file.write("false");
		file.write(QByteArray().append("</"+allCheckBoxes[ii]->objectName()+">\n"));
		}
	file.write("\t</CheckBoxes>\n");
	// -> radio buttons
	file.write("\t<RadioButtons>\n");
	for(int ii = 0; ii < allRadioButtons.size(); ++ii)
		{
		file.write(QByteArray().append("\t\t<"+allRadioButtons[ii]->objectName()+">"));
		if(allRadioButtons[ii]->isChecked())
			file.write("true");
		else
			file.write("false");
		file.write(QByteArray().append("</"+allRadioButtons[ii]->objectName()+">\n"));
		}
	file.write("\t</RadioButtons>\n");
	// -> sliders
	file.write("\t<Sliders>\n");
	for(int ii = 0; ii < allSliders.size(); ++ii)
		file.write(QByteArray().append("\t\t<"+allSliders[ii]->objectName()+">"+QString("%1").arg(allSliders[ii]->value(),0,10)+"</"+allSliders[ii]->objectName()+">\n"));
	file.write("\t</Sliders>\n");
	// -> spansliders
	file.write("\t<Spansliders>\n");
	for(int ii = 0; ii < allSpansliders.size(); ++ii)
		file.write(QByteArray().append("\t\t<"+allSpansliders[ii]->objectName()+">"+QString("%1").arg(allSpansliders[ii]->lowerValue(),0,10)+":"+QString("%1").arg(allSpansliders[ii]->upperValue(),0,10)+"</"+allSpansliders[ii]->objectName()+">\n"));
	file.write("\t</Spansliders>\n");	
	// -> comboboxes
	file.write("\t<Comboboxes>\n");
	for(int ii = 0; ii < allComboboxes.size(); ++ii)
		file.write(QByteArray().append("\t\t<"+allComboboxes[ii]->objectName()+">"+QString("%1").arg(allComboboxes[ii]->currentIndex(),0,10)+"</"+allComboboxes[ii]->objectName()+">\n"));
	file.write("\t</Comboboxes>\n");
	// -> toolboxes
	file.write("\t<Toolboxes>\n");
	for(int ii = 0; ii < allToolboxes.size(); ++ii)
		file.write(QByteArray().append("\t\t<"+allToolboxes[ii]->objectName()+">"+QString("%1").arg(allToolboxes[ii]->currentIndex(),0,10)+"</"+allToolboxes[ii]->objectName()+">\n"));
	file.write("\t</Toolboxes>\n");
	// -> tabwidgets
	file.write("\t<Tabwidgets>\n");
	for(int ii = 0; ii < allTabwidgets.size(); ++ii)
		file.write(QByteArray().append("\t\t<"+allTabwidgets[ii]->objectName()+">"+QString("%1").arg(allTabwidgets[ii]->currentIndex(),0,10)+"</"+allTabwidgets[ii]->objectName()+">\n"));
	file.write("\t</Tabwidgets>\n");
	// -> colors
	file.write("\t<Colors>\n");
	file.write(QByteArray().append("\t\t<CC>"+allColors["CC"].name()+"</CC>\n"));
	file.write(QByteArray().append("\t\t<slider>"+allColors["slider"].name()+"</slider>\n"));
	file.write(QByteArray().append("\t\t<parameter>"+allColors["parameter"].name()+"</parameter>\n"));
	file.write(QByteArray().append("\t\t<divider>"+allColors["divider"].name()+"</divider>\n"));
	file.write(QByteArray().append("\t\t<value>"+allColors["value"].name()+"</value>\n"));
	file.write("\t</Colors>\n");
	// -> left screen
	file.write("\t<LeftBitmap>\n");
	QByteArray leftBa;
	QBuffer leftBuffer(&leftBa);
	QImage(graphicsViewScreen1->currentFile).scaled(480, 140).save(&leftBuffer, "PNG");
	file.write(leftBa.toBase64());
	file.write("\n");
	file.write("\t</LeftBitmap>\n");
	// -> right screen
	file.write("\t<RightBitmap>\n");
	QByteArray rightBa;
	QBuffer rightBuffer(&rightBa);
	QImage(graphicsViewScreen2->currentFile).scaled(480, 140).save(&rightBuffer, "PNG");
	file.write(rightBa.toBase64());
	file.write("\n");
	file.write("\t</RightBitmap>\n");
	// -> line edits
	file.write("\t<Lineedits>\n");
	for(int ii = 0; ii < allLineedits.size(); ++ii)
		file.write(QByteArray().append("\t\t<"+allLineedits[ii]->objectName()+">"+allLineedits[ii]->text()+"</"+allLineedits[ii]->objectName()+">\n"));
	file.write("\t</Lineedits>\n");
	// close and save the configuration file
	file.write("</qkontrol>\n");
	file.close();	

        // set window title
	this->setWindowTitle(QFileInfo(file).fileName()+" - qKontrol");
	return true;
	}


// page button proxy functions
void qkontrolWindow::b_goLeft() { b_setPage(bPage-1); }
void qkontrolWindow::b_goRight() { b_setPage(bPage+1); }
void qkontrolWindow::k_goLeft() { k_setPage(kPage-1); }
void qkontrolWindow::k_goRight() { k_setPage(kPage+1); }

// page switching functions
void qkontrolWindow::b_setPage(int page)
	{
	// return if the requested page is out of range
	if((page < 0) || (page > 3))
		return;

	// toggle visibility of the left / right buttons depending on the selected page
	toolButton_b_left->setEnabled(true);
	toolButton_b_right->setEnabled(true);
	if(page == 0)
		toolButton_b_left->setEnabled(false);
	if(page == 3)
		toolButton_b_right->setEnabled(false);

	// update page index and set the requested index
	labelbPage->setText("page "+QString::number(page+1)+"/4");
	bPage = page;
	stackedWidget_b->setCurrentIndex(page);
	}	

void qkontrolWindow::k_setPage(int page)
	{
	// return if the requested page is out of range
	if((page < 0) || (page > 3))
		return;

	// toggle visibility of the left / right buttons depending on the selected page
	toolButton_k_left->setEnabled(true);
	toolButton_k_right->setEnabled(true);
	if(page == 0)
		toolButton_k_left->setEnabled(false);
	if(page == 3)
		toolButton_k_right->setEnabled(false);
        
	// update page index and set the requested index
	labelkPage->setText("page "+QString::number(page+1)+"/4");
	kPage = page;
	stackedWidget_k->setCurrentIndex(page);
	}

// switch knob and button layout on keyboard if requested with the HID keys
void qkontrolWindow::setKontrolpage(unsigned int page)
	{
	// switch the button background light depending on the page
	if(page == 0)
		lightArray.replace(33,1,QByteArray::fromHex("00"));
	else
		lightArray.replace(33,1,QByteArray::fromHex("FF"));
	if(page == 3)
		lightArray.replace(34,1,QByteArray::fromHex("00"));
	else
		lightArray.replace(34,1,QByteArray::fromHex("FF"));
	kontrolPage = page;

	// update the button background lightning
	setButtons();

	// update key functions and screen information
	setKeyzones();
	}

// when a preset button is clicked, then load another preset file from the same directory if existing
void qkontrolWindow::zapPreset(bool direction)
	{
	// check if the zapping request is out of reach first
	if(dirCount==0)
		return;
	if((direction==0) && (dirPosition==0))
		return;
	if((direction==1) && (dirPosition==dirCount-1))
		return;
	// change the dir position based on request
	if(direction==0)
		dirPosition -= 1;
	else
		dirPosition += 1;
	QStringList dirList = dirName.entryList(QStringList() << "*.qcp");

	load(dirName.absolutePath()+"/"+dirList.at(dirPosition));
	}
// function to toggle the background lightning of the HID buttons
void qkontrolWindow::setButtons()
	{
	res = hid_write(handle, (unsigned char*) lightArray.constData(), lightArray.count());
	}	

// function to fetch a filename to load
void qkontrolWindow::getFileName()
	{
	QFile file(QFileDialog::getOpenFileName(this, "choose a qKontrol preset file!",QDir::homePath(),"QCP files (*.qcp)",0));
	if(file.exists())
		load(QFileInfo(file).absoluteFilePath());
	}

// function to load a settings file
bool qkontrolWindow::load(QString filename)
	{
	// find out what necessary values do exist... if you want to use new widgets in the UI, you need to modify this function
	QList<QSpinBox *> allSpinBoxes = this->findChildren<QSpinBox *>();
	QList<QCheckBox *> allCheckBoxes = this->findChildren<QCheckBox *>();
	QList<QRadioButton *> allRadioButtons = this->findChildren<QRadioButton *>();
	QList<QSlider *> allSliders = this->findChildren<QSlider *>();
	QList<QxtSpanSlider *> allSpansliders = this->findChildren<QxtSpanSlider *>();
	QList<QComboBox *> allComboboxes = this->findChildren<QComboBox *>();
	QList<QToolBox *> allToolboxes = this->findChildren<QToolBox *>();
	QList<QTabWidget *> allTabwidgets = this->findChildren<QTabWidget *>();
	QList<QLineEdit *> allLineedits = this->findChildren<QLineEdit *>(QRegExp("_description_"));

	// inspect the document with XML parser functions
	QFile file(filename);
	QDomDocument doc;
	doc.setContent(&file);

	// set window title
	this->setWindowTitle(QFileInfo(file).fileName()+" - qKontrol");

	// examine directory structure to permit browsing using the preset keys
	dirName = QFileInfo(file).dir();
	QStringList dirList = dirName.entryList(QStringList() << "*.qcp");
	dirCount = dirList.count();
	for(unsigned int i=0; i< dirCount; i++)
		if(QFileInfo(file).fileName() == dirList[i])
			dirPosition = i;
	if(dirPosition == 0)
		lightArray.replace(23,1,QByteArray::fromHex("00"));
	else
		lightArray.replace(23,1,QByteArray::fromHex("FF"));
	if(dirPosition == dirCount-1)
		lightArray.replace(28,1,QByteArray::fromHex("00"));
	else
		lightArray.replace(28,1,QByteArray::fromHex("FF"));
	if(dirCount < 2)
		{
		lightArray.replace(23,1,QByteArray::fromHex("00"));
		lightArray.replace(28,1,QByteArray::fromHex("00"));
		}
	setButtons();

	// find the root tag (in this case: <qkontrol>)
	QDomElement root = doc.documentElement();
	
	// dive down into subtags - SAME CAT ORDER LIKE IN THE SAVE() FUNCTION
	QDomNode m = root.firstChild();
	QDomNode n = m.firstChild();

	// process integer spin boxes
	while(!n.isNull())
		{
		QDomElement isb = n.toElement();
		if(!isb.isNull())
			{
			for (int i=0;i<allSpinBoxes.size();i++)
				{ 
				if(isb.tagName() == QString(allSpinBoxes[i]->objectName()))
					allSpinBoxes[i]->setValue(isb.text().toInt());
				} 
			}
		n = n.nextSibling();
		}
	// process checkboxes
	m = m.nextSibling();
	n = m.firstChild();
	while(!n.isNull())
		{
		QDomElement chb = n.toElement();
		if(!chb.isNull())
			{
			for (int i=0;i<allCheckBoxes.size();i++)
				{ 
				if(chb.tagName() == QString(allCheckBoxes[i]->objectName()))
					allCheckBoxes[i]->setChecked(chb.text().contains("true"));
				} 
			}
		n = n.nextSibling();
		}	
	// process radio buttons
	m = m.nextSibling();
	n = m.firstChild();
	while(!n.isNull())
		{
		QDomElement rdb = n.toElement();
		if(!rdb.isNull())
			{
			for (int i=0;i<allRadioButtons.size();i++)
				{
				if(rdb.tagName() == QString(allRadioButtons[i]->objectName()))
					allRadioButtons[i]->setChecked(rdb.text().contains("true"));
				}
			}
		n = n.nextSibling();
		}
	// process sliders
	m = m.nextSibling();
	n = m.firstChild();
	while(!n.isNull())
		{
		QDomElement sld = n.toElement();
		if(!sld.isNull())
			{
			for (int i=0;i<allSliders.size();i++)
				{ 
				if(sld.tagName() == QString(allSliders[i]->objectName()))
					allSliders[i]->setValue(sld.text().toInt());
				} 
			}
		n = n.nextSibling();
		}
	// process spansliders
	m = m.nextSibling();
	n = m.firstChild();
	while(!n.isNull())
		{
		QDomElement sps = n.toElement();
		if(!sps.isNull())
			{
			for (int i=0;i<allSpansliders.size();i++)
				{ 
				if(sps.tagName() == QString(allSpansliders[i]->objectName()))
					allSpansliders[i]->setSpan(sps.text().split(':')[0].toInt(),sps.text().split(':')[1].toInt());
				} 
			}
		n = n.nextSibling();
		}
	// process comboboxes
	m = m.nextSibling();
	n = m.firstChild();
	while(!n.isNull())
		{
		QDomElement cmb = n.toElement();
		if(!cmb.isNull())
			{
			for (int i=0;i<allComboboxes.size();i++)
				{ 
				if(cmb.tagName() == QString(allComboboxes[i]->objectName()))
					allComboboxes[i]->setCurrentIndex(cmb.text().toInt());
				} 
			}
		n = n.nextSibling();
		}
	// process toolboxes
	m = m.nextSibling();
	n = m.firstChild();
	while(!n.isNull())
		{
		QDomElement tlb = n.toElement();
		if(!tlb.isNull())
			{
			for (int i=0;i<allToolboxes.size();i++)
				{
				if(tlb.tagName() == QString(allToolboxes[i]->objectName()))
					allToolboxes[i]->setCurrentIndex(tlb.text().toInt());
				}
			}
		n = n.nextSibling();
		}
	// process tabwidgets
	m = m.nextSibling();
	n = m.firstChild();
	while(!n.isNull())
		{
		QDomElement tbw = n.toElement();
		if(!tbw.isNull())
			{
			for (int i=0;i<allTabwidgets.size();i++)
				{
				if(tbw.tagName() == QString(allTabwidgets[i]->objectName()))
					allTabwidgets[i]->setCurrentIndex(tbw.text().toInt());
				}
			}
		n = n.nextSibling();
		}
	// process colors
	m = m.nextSibling();
	n = m.firstChild();
	while(!n.isNull())
		{
		QDomElement color = n.toElement();
		if(!color.isNull())
			allColors[color.tagName()].setNamedColor(color.text());
		n = n.nextSibling();
		}
	updateColors();
	// process left display
	m = m.nextSibling();
	QDomElement bitmap = m.toElement();
	leftScreen.open();
	QByteArray data = QByteArray::fromBase64(QByteArray().append(bitmap.text()));
	leftScreen.write(data);
	leftScreen.close();
	graphicsViewScreen1->setImage(leftScreen.fileName());
	// process right display
	m = m.nextSibling();
	bitmap = m.toElement();
	rightScreen.open();
	data = QByteArray::fromBase64(QByteArray().append(bitmap.text()));
	rightScreen.write(data);
	rightScreen.close();
	graphicsViewScreen2->setImage(rightScreen.fileName());
	// process line edits
	m = m.nextSibling();
	n = m.firstChild();
	while(!n.isNull())
		{
		QDomElement lne = n.toElement();
		if(!lne.isNull())
			{
			for (int i=0;i<allLineedits.size();i++)
				{
				if(lne.tagName() == QString(allLineedits[i]->objectName()))
					allLineedits[i]->setText(lne.text());
				}
			}
		n = n.nextSibling();
		}		
	// file content is read now... we can close it
	file.close();
	setKeyzones();
	return true;
	}

