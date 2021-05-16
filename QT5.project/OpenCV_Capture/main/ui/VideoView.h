/************************************************************************************/
/* An OpenCV/Qt based realtime application			                    */
/*                                                                                  */
/*                                                                                  */
/* Based on the work of                                                             */
/*      Joseph Pan      <https://github.com/wzpan/QtEVM>                            */
/*      Nick D'Ademo    <https://github.com/nickdademo/qt-opencv-multithreaded>     */
/*                                                                                  */
/* ui/VideoView.h        							    */
/*                                                                                  */
/* This program is free software: you can redistribute it and/or modify             */
/* it under the terms of the GNU General Public License as published by             */
/* the Free Software Foundation, either version 3 of the License, or                */
/* (at your option) any later version.                                              */
/*                                                                                  */
/* This program is distributed in the hope that it will be useful,                  */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of                   */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                    */
/* GNU General Public License for more details.                                     */
/*                                                                                  */
/* You should have received a copy of the GNU General Public License                */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>.            */
/************************************************************************************/

#ifndef VIDEOVIEW_H
#define VIDEOVIEW_H

// Qt
#include <QFileInfo>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardItem>
// Local
//#include "main/ui/MagnifyOptions.h"
#include "main/other/Structures.h"
#include "main/threads/PlayerThread.h"
#include "main/ui/FrameLabel.h"
#include "main/threads/SavingThread.h"
#include "helper/MyUtils.h"
#include "helper/tcpsendpix.h"

namespace Ui {
class VideoView;
}

class VideoView : public QWidget
{
Q_OBJECT

public:
	explicit VideoView(QWidget *parent, QString filepath);
	~VideoView();
	bool loadVideo(int threadPrio, int width, int height, double fps);
	void setCodec(int codec);
	void set_useVideoCodec(bool use);

	QStandardItemModel *list_model;
	TcpSendPix *myTcpSendPix;

private:
	Ui::VideoView *ui;
	QFileInfo file;
	QString filename;
	PlayerThread *playerThread;
	ImageProcessingFlags imageProcessingFlags;
	bool isFileLoaded;
	//MagnifyOptions *magnifyOptionsTab;
	void stopPlayerThread();
	QString getFormattedTime(int time);
	void handleOriginalWindow(bool doEmit);
	FrameLabel *originalFrame;
	SavingThread *vidSaver;
	int codec;
	bool useVideoCodec;
	struct ImageProcessingFlags imgProcFlags;
	struct ImageProcessingSettings imgSettings;

public slots:
	void newMouseData(struct MouseData mouseData);
	void newMouseDataOriginalFrame(struct MouseData mouseData);
	void updateMouseCursorPosLabel();
	void updateMouseCursorPosLabelOriginalFrame();
	void endOfFrame_action();
	void endOfSaving_action();
	void updateProgressBar(int frame);

private slots:
	void updateFrame(const QImage &frame);
	void updateOriginalFrame(const QImage &frame);
	void updatePlayerThreadStats(struct ThreadStatisticsData statData);
	void handleContextMenuAction(QAction *action);
	void play();
	void stop();
	void pause();
	void setTime();
	void hideSettings();
	void save_action();
	void handleTabChange(int index);

	void on_checkBoxGrayscale_clicked(bool checked);
	void on_checkBoxHsvHistogram_clicked(bool checked);

	void on_checkBoxDilate_clicked(bool checked);
	void on_checkBoxErode_clicked(bool checked);
	void on_checkBoxCanny_clicked(bool checked);
	void on_checkBoxPCA_clicked(bool checked);

	void on_sliderThreshold_lowerValueChanged(int value);
	void on_sliderThreshold_upperValueChanged(int value);
	void on_spinBoxAperture_valueChanged(int arg1);
	void on_checkBoxL2Gradient_clicked(bool checked);

	void on_horizontalSliderDilate_valueChanged(int value);
	void on_horizontalSliderErode_valueChanged(int value);

	void on_sliderHue_lowerValueChanged(int value);
	void on_sliderHue_upperValueChanged(int value);
	void on_sliderSat_lowerValueChanged(int value);
	void on_sliderSat_upperValueChanged(int value);
	void on_sliderVal_lowerValueChanged(int value);
	void on_sliderVal_upperValueChanged(int value);

	void on_checkBoxBlurMode_clicked(bool checked);
	void on_buttonMedian_clicked(bool checked);
	void on_buttonGaussian_clicked(bool checked);
	void on_buttonMorph_clicked(bool checked);
	void on_checkBoxHsvEqualize_clicked(bool checked);
	void on_checkBoxGrabCut_clicked(bool checked);

	void on_pushButtonShot_clicked();
	void on_buttonMorphOpen_clicked();
	void on_buttonMorphClose_clicked();
	void on_buttonMorphGradient_clicked();
	void on_buttonShotSend_clicked();

signals:
	void newImageProcessingFlags(struct ImageProcessingFlags);
	void newProcessingSettings(struct ImageProcessingSettings);
	void setROI(QRect roi);
};

#endif // VIDEOVIEW_H
