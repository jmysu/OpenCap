/************************************************************************************/
/* An OpenCV/Qt based realtime application			                    */
/*                                                                                  */
/*                                                                                  */
/* Based on the work of                                                             */
/*      Joseph Pan      <https://github.com/wzpan/QtEVM>                            */
/*      Nick D'Ademo    <https://github.com/nickdademo/qt-opencv-multithreaded>     */
/*                                                                                  */
/* ui/VideoView.cpp								    */
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

#include <QStandardItemModel>
#include <QStandardItem>
#include <QStringList>

#include "main/ui/VideoView.h"
#include "ui_VideoView.h"

VideoView::VideoView(QWidget *parent, QString filepath) :
	QWidget(parent),
	ui(new Ui::VideoView),
	codec(-1),
	useVideoCodec(true)
{
	ui->setupUi(this);

	// The FrameLabel for the original Frame
	originalFrame = new FrameLabel(this);
	originalFrame->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	originalFrame->setAutoFillBackground(true);
	originalFrame->setFrameShape(QFrame::Box);
	originalFrame->setAlignment(ui->frameLabel->alignment());
	originalFrame->setMouseTracking(true);
	originalFrame->menu->clear();
	originalFrame->menu->addActions(ui->frameLabel->menu->actions());
	ui->frameLayout->addWidget(originalFrame, 0, 0);
	originalFrame->setVisible(false);

	this->file = QFileInfo(filepath);
	filename = file.fileName();
	// Internal Flag
	isFileLoaded = false;

	// Set initial GUI state

	// Initialize ImageProcessingFlags structure
	imageProcessingFlags.grayscaleOn = false;
	imageProcessingFlags.hsvHistogramOn = false;
	//imageProcessingFlags.colorMagnifyOn = false;
	//imageProcessingFlags.laplaceMagnifyOn = false;
	//imageProcessingFlags.rieszMagnifyOn = false;

	// Connect signals/slots
	connect(ui->hideSettingsButton, SIGNAL(released()), this, SLOT(hideSettings()));
	connect(ui->frameLabel, SIGNAL(onMouseMoveEvent()), this, SLOT(updateMouseCursorPosLabel()));
	connect(originalFrame, SIGNAL(onMouseMoveEvent()), this, SLOT(updateMouseCursorPosLabelOriginalFrame()));
	connect(ui->frameLabel->menu, SIGNAL(triggered(QAction*)), this, SLOT(handleContextMenuAction(QAction*)));

	// Register type
	qRegisterMetaType<struct ThreadStatisticsData>("ThreadStatisticsData");

	// setting default
	imgSettings.hsvHueLow = 0;
	imgSettings.hsvHueHigh = 179;
	imgSettings.hsvSatLow = 0;
	imgSettings.hsvSatHigh = 255;
	imgSettings.hsvValLow = 0;
	imgSettings.hsvValHigh = 255;

	imgSettings.blurType = 0;
	imgSettings.morphOption = 0;
	imgSettings.dilateNumberOfIterations = 3;
	imgSettings.erodeNumberOfIterations = 3;

	imgSettings.cannyThreshold1 = 0;
	imgSettings.cannyThreshold2 = 255;
	imgSettings.cannyApertureSize = 3;
	imgSettings.cannyL2gradient = false;
}

VideoView::~VideoView()
{
	if (isFileLoaded) {
		// Stop PlayerThread
		if (playerThread->isRunning())
			stopPlayerThread();

		// Release File
		if (playerThread->releaseFile())
			qDebug() << "[" << filename << "] Video succesfully released.";
		else
			qDebug() << "[" << filename << "] WARNING: Video already released.";
	}
	// Delete Threads
	delete playerThread;
	delete vidSaver;
	// Delete UI integrated Pointer
	delete originalFrame;
	//delete magnifyOptionsTab;
	// Delete UI
	delete ui;
}

bool VideoView::loadVideo(int threadPrio, int width, int height, double fps)
{
	// Set frame label text
	ui->frameLabel->setText(tr("Loading Video..."));
	QString filepath = file.absoluteFilePath();
	std::string stringFilepath = filepath.toStdString();
	// create player thread
	playerThread = new PlayerThread(stringFilepath, width, height, fps);

	if (playerThread->loadFile()) {
		ui->frameLabel->setText("Video loaded");

		// Create MagnifyOptions tab and set current
		//this->magnifyOptionsTab = new MagnifyOptions();
		//this->magnifyOptionsTab->setFPS(playerThread->getFPS());
		//this->magnifyOptionsTab->reset();
		//ui->tabWidget->insertTab(0, magnifyOptionsTab, tr("Options"));
		ui->tabWidget->setCurrentIndex(0);
		ui->InfoTab->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Ignored);

		ui->lineEditFolder->setText(MyUtils::stringMyFolder());
		ui->listViewCapture->setStyleSheet("QListView {background-color: lightGray;}");
		//Setup model for listView
		list_model = new QStandardItemModel(this);
		ui->listViewCapture->setModel(list_model);
		ui->listViewCapture->setViewMode(QListView::IconMode);
		ui->listViewCapture->setFlow(QListView::LeftToRight);
		ui->listViewCapture->setUniformItemSizes(true);

		// Setup signal/slot connections
		//connect(playerThread, SIGNAL(maxLevels(int)), magnifyOptionsTab, SLOT(setMaxLevel(int)));
		connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(handleTabChange(int)));
		connect(this, SIGNAL(newImageProcessingFlags(ImageProcessingFlags)), playerThread, SLOT(updateImageProcessingFlags(ImageProcessingFlags)));
		connect(this, SIGNAL(newProcessingSettings(ImageProcessingSettings)), playerThread, SLOT(updateProcessingSettings(ImageProcessingSettings)));

		connect(this, SIGNAL(setROI(QRect)), playerThread, SLOT(setROI(QRect)));
		//connect(playerThread, SIGNAL(maxLevels(int)), magnifyOptionsTab, SLOT(setMaxLevel(int)));
		// Setup signal/slot connections for MagnifyOptions
		//connect(magnifyOptionsTab, SIGNAL(newImageProcessingSettings(struct ImageProcessingSettings)), playerThread, SLOT(updateImageProcessingSettings(struct ImageProcessingSettings)));
		//connect(magnifyOptionsTab, SIGNAL(newImageProcessingFlags(struct ImageProcessingFlags)), playerThread, SLOT(updateImageProcessingFlags(struct ImageProcessingFlags)));
		// Setup signal/slot for PlayerThread
		connect(playerThread, SIGNAL(endOfFrame()), this, SLOT(endOfFrame_action()));
		connect(playerThread, SIGNAL(updateStatisticsInGUI(struct ThreadStatisticsData)), this, SLOT(updatePlayerThreadStats(struct ThreadStatisticsData)));
		connect(ui->PlayButton, SIGNAL(clicked()), this, SLOT(play()));
		connect(ui->StopButton, SIGNAL(clicked()), this, SLOT(stop()));
		connect(ui->TimeSlider, SIGNAL(sliderPressed()), playerThread, SLOT(pauseThread()));
		connect(ui->TimeSlider, SIGNAL(sliderReleased()), this, SLOT(setTime()));

		// Connect frames emitting
		connect(playerThread, SIGNAL(newFrame(QImage)), this, SLOT(updateFrame(QImage)));
		connect(playerThread, SIGNAL(origFrame(QImage)), this, SLOT(updateOriginalFrame(QImage)));

		// Create the SavingThread and connect buttons to it's meant functions
		vidSaver = new SavingThread();
		connect(ui->saveButton, SIGNAL(clicked()), this, SLOT(save_action()));
		connect(vidSaver, SIGNAL(updateProgress(int)), this, SLOT(updateProgressBar(int)));
		connect(vidSaver, SIGNAL(endOfSaving()), this, SLOT(endOfSaving_action()));
		ui->saveProgressBar->hide();

		connect(ui->frameLabel, SIGNAL(newMouseData(struct MouseData)), this, SLOT(newMouseData(struct MouseData)));
		connect(originalFrame, SIGNAL(newMouseData(struct MouseData)), this, SLOT(newMouseData(struct MouseData)));

		// Set initial data in player thread
		emit setROI(QRect(0, 0, playerThread->getInputSourceWidth(), playerThread->getInputSourceHeight()));
		emit newImageProcessingFlags(imageProcessingFlags);
		emit newProcessingSettings(imgSettings);

		// Start capturing frames from camera
		playerThread->start((QThread::Priority)threadPrio);

		ui->deviceNumberLabel->setText(filename);
		ui->cameraResolutionLabel->setText(QString::number(playerThread->getInputSourceWidth()) + QString("x") + QString::number(playerThread->getInputSourceHeight()));
		ui->totalTimeLabel->setText(getFormattedTime(playerThread->getInputFrameLength() / playerThread->getFPS()));
		ui->TimeSlider->setMaximum(playerThread->getInputFrameLength());
		// Set internal flag and return
		isFileLoaded = true;
		return true;
	}else
		return false;
}

void VideoView::updateOriginalFrame(const QImage &frame)
{
	// Display frame
	originalFrame->setPixmap(QPixmap::fromImage(frame).scaled(ui->frameLabel->width(), ui->frameLabel->height(), Qt::KeepAspectRatio));
}

QString VideoView::getFormattedTime(int timeInSeconds)
{
	int seconds = (int)(timeInSeconds) % 60;
	int minutes = (int)((timeInSeconds / 60) % 60);
	int hours = (int)((timeInSeconds / (60 * 60)) % 24);

	QTime t(hours, minutes, seconds);
	if (hours == 0)
		return t.toString("mm:ss");
	else
		return t.toString("h:mm:ss");
}

void VideoView::endOfFrame_action()
{
	ui->TimeSlider->setValue(0);
	ui->PlayButton->setText("Play");
	playerThread->stopAction();

	if (ui->LoopCheckBox->isChecked())
		play();
}

void VideoView::stopPlayerThread()
{
	qDebug() << "[" << filename << "] About to stop player thread...";
	playerThread->stop();
	playerThread->wait();
	qDebug() << "[" << filename << "] Player thread successfully stopped.";
}

void VideoView::updatePlayerThreadStats(struct ThreadStatisticsData statData)
{
	ui->TimeSlider->setValue(statData.nFramesProcessed);
	ui->currentTimeLabel->setText(getFormattedTime(statData.nFramesProcessed / (int)playerThread->getFPS()));
	ui->captureRateLabel->setText(QString::number(statData.averageFPS));
	ui->currentFrameNumberLabel->setText(QString::number(statData.nFramesProcessed));

	// Show processing rate in processingRateLabel
	ui->processingRateLabel->setText(QString::number(statData.averageVidProcessingFPS) + " fps");
	// Show ROI information in roiLabel
	ui->roiLabel->setText(QString("(") + QString::number(playerThread->getCurrentROI().x()) + QString(",") +
			      QString::number(playerThread->getCurrentROI().y()) + QString(") ") +
			      QString::number(playerThread->getCurrentROI().width()) +
			      QString("x") + QString::number(playerThread->getCurrentROI().height()));
}

void VideoView::updateFrame(const QImage &frame)
{
	int w = ui->frameLabel->width();
	int h = ui->frameLabel->height();
	// Display frame
	ui->frameLabel->setPixmap(QPixmap::fromImage(frame).scaled(w, h, Qt::KeepAspectRatio));
}

void VideoView::updateMouseCursorPosLabel()
{
	// Update mouse cursor position in mouseCursorPosLabel
	ui->mouseCursorPosLabel->setText(QString("(") + QString::number(ui->frameLabel->getMouseCursorPos().x()) +
					 QString(",") + QString::number(ui->frameLabel->getMouseCursorPos().y()) +
					 QString(")"));

	// Show pixel cursor position if camera is connected (image is being shown)
	if (ui->frameLabel->pixmap() != 0) {
		// Scaling factor calculation depends on whether frame is scaled to fit label or not
		if (!ui->frameLabel->hasScaledContents()) {
			double xScalingFactor = ((double)ui->frameLabel->getMouseCursorPos().x() - ((ui->frameLabel->width() - ui->frameLabel->pixmap()->width()) / 2)) / (double)ui->frameLabel->pixmap()->width();
			double yScalingFactor = ((double)ui->frameLabel->getMouseCursorPos().y() - ((ui->frameLabel->height() - ui->frameLabel->pixmap()->height()) / 2)) / (double)ui->frameLabel->pixmap()->height();

			ui->mouseCursorPosLabel->setText(ui->mouseCursorPosLabel->text() +
							 QString(" [") + QString::number((int)(xScalingFactor * playerThread->getCurrentROI().width())) +
							 QString(",") + QString::number((int)(yScalingFactor * playerThread->getCurrentROI().height())) +
							 QString("]"));
		}else {
			double xScalingFactor = (double)ui->frameLabel->getMouseCursorPos().x() / (double)ui->frameLabel->width();
			double yScalingFactor = (double)ui->frameLabel->getMouseCursorPos().y() / (double)ui->frameLabel->height();

			ui->mouseCursorPosLabel->setText(ui->mouseCursorPosLabel->text() +
							 QString(" [") + QString::number((int)(xScalingFactor * playerThread->getCurrentROI().width())) +
							 QString(",") + QString::number((int)(yScalingFactor * playerThread->getCurrentROI().height())) +
							 QString("]"));
		}
	}
}

void VideoView::newMouseData(struct MouseData mouseData)
{
	// Local variable(s)
	int x_temp, y_temp, width_temp, height_temp;
	QRect selectionBox;

	// Set ROI
	if (mouseData.leftButtonRelease) {
		double xScalingFactor;
		double yScalingFactor;
		double wScalingFactor;
		double hScalingFactor;

		// Selection box calculation depends on whether frame is scaled to fit label or not
		if (!ui->frameLabel->hasScaledContents()) {
			xScalingFactor = ((double)mouseData.selectionBox.x() - ((ui->frameLabel->width() - ui->frameLabel->pixmap()->width()) / 2)) / (double)ui->frameLabel->pixmap()->width();
			yScalingFactor = ((double)mouseData.selectionBox.y() - ((ui->frameLabel->height() - ui->frameLabel->pixmap()->height()) / 2)) / (double)ui->frameLabel->pixmap()->height();
			wScalingFactor = (double)playerThread->getCurrentROI().width() / (double)ui->frameLabel->pixmap()->width();
			hScalingFactor = (double)playerThread->getCurrentROI().height() / (double)ui->frameLabel->pixmap()->height();
		}else {
			xScalingFactor = (double)mouseData.selectionBox.x() / (double)ui->frameLabel->width();
			yScalingFactor = (double)mouseData.selectionBox.y() / (double)ui->frameLabel->height();
			wScalingFactor = (double)playerThread->getCurrentROI().width() / (double)ui->frameLabel->width();
			hScalingFactor = (double)playerThread->getCurrentROI().height() / (double)ui->frameLabel->height();
		}

		// Set selection box properties (new ROI)
		selectionBox.setX(xScalingFactor * playerThread->getCurrentROI().width() + playerThread->getCurrentROI().x());
		selectionBox.setY(yScalingFactor * playerThread->getCurrentROI().height() + playerThread->getCurrentROI().y());
		selectionBox.setWidth(wScalingFactor * mouseData.selectionBox.width());
		selectionBox.setHeight(hScalingFactor * mouseData.selectionBox.height());

		// Check if selection box has NON-ZERO dimensions
		if ((selectionBox.width() != 0) && ((selectionBox.height()) != 0)) {
			// Selection box can also be drawn from bottom-right to top-left corner
			if (selectionBox.width() < 0) {
				x_temp = selectionBox.x();
				width_temp = selectionBox.width();
				selectionBox.setX(x_temp + selectionBox.width());
				selectionBox.setWidth(width_temp * -1);
			}
			if (selectionBox.height() < 0) {
				y_temp = selectionBox.y();
				height_temp = selectionBox.height();
				selectionBox.setY(y_temp + selectionBox.height());
				selectionBox.setHeight(height_temp * -1);
			}

			// Check if selection box is not outside window
			if ((selectionBox.x() < 0) || (selectionBox.y() < 0) ||
			    ((selectionBox.x() + selectionBox.width()) > (playerThread->getCurrentROI().x() + playerThread->getCurrentROI().width())) ||
			    ((selectionBox.y() + selectionBox.height()) > (playerThread->getCurrentROI().y() + playerThread->getCurrentROI().height())) ||
			    (selectionBox.x() < playerThread->getCurrentROI().x()) ||
			    (selectionBox.y() < playerThread->getCurrentROI().y())) {
				// Display error message
				QMessageBox::warning(this, tr("ERROR:"), tr("Selection box outside range. Please try again."));
			}
			// Set ROI
			else
				emit setROI(selectionBox);
		}
	}
}

void VideoView::updateMouseCursorPosLabelOriginalFrame()
{
	// Update mouse cursor position in mouseCursorPosLabel
	ui->mouseCursorPosLabel->setText(QString("(") + QString::number(originalFrame->getMouseCursorPos().x()) +
					 QString(",") + QString::number(originalFrame->getMouseCursorPos().y()) +
					 QString(")"));

	// Show pixel cursor position if camera is connected (image is being shown)
	if (originalFrame->pixmap() != 0) {
		// Scaling factor calculation depends on whether frame is scaled to fit label or not
		if (!originalFrame->hasScaledContents()) {
			double xScalingFactor = ((double)originalFrame->getMouseCursorPos().x() - ((originalFrame->width() - originalFrame->pixmap()->width()) / 2)) / (double)originalFrame->pixmap()->width();
			double yScalingFactor = ((double)originalFrame->getMouseCursorPos().y() - ((originalFrame->height() - originalFrame->pixmap()->height()) / 2)) / (double)originalFrame->pixmap()->height();

			ui->mouseCursorPosLabel->setText(ui->mouseCursorPosLabel->text() +
							 QString(" [") + QString::number((int)(xScalingFactor * playerThread->getCurrentROI().width())) +
							 QString(",") + QString::number((int)(yScalingFactor * playerThread->getCurrentROI().height())) +
							 QString("]"));
		}else {
			double xScalingFactor = (double)originalFrame->getMouseCursorPos().x() / (double)originalFrame->width();
			double yScalingFactor = (double)originalFrame->getMouseCursorPos().y() / (double)originalFrame->height();

			ui->mouseCursorPosLabel->setText(ui->mouseCursorPosLabel->text() +
							 QString(" [") + QString::number((int)(xScalingFactor * playerThread->getCurrentROI().width())) +
							 QString(",") + QString::number((int)(yScalingFactor * playerThread->getCurrentROI().height())) +
							 QString("]"));
		}
	}
}

void VideoView::newMouseDataOriginalFrame(struct MouseData mouseData)
{
	// Local variable(s)
	int x_temp, y_temp, width_temp, height_temp;
	QRect selectionBox;

	// Set ROI
	if (mouseData.leftButtonRelease) {
		double xScalingFactor;
		double yScalingFactor;
		double wScalingFactor;
		double hScalingFactor;

		// Selection box calculation depends on whether frame is scaled to fit label or not
		if (!originalFrame->hasScaledContents()) {
			xScalingFactor = ((double)mouseData.selectionBox.x() - ((originalFrame->width() - originalFrame->pixmap()->width()) / 2)) / (double)originalFrame->pixmap()->width();
			yScalingFactor = ((double)mouseData.selectionBox.y() - ((originalFrame->height() - originalFrame->pixmap()->height()) / 2)) / (double)originalFrame->pixmap()->height();
			wScalingFactor = (double)playerThread->getCurrentROI().width() / (double)originalFrame->pixmap()->width();
			hScalingFactor = (double)playerThread->getCurrentROI().height() / (double)originalFrame->pixmap()->height();
		}else {
			xScalingFactor = (double)mouseData.selectionBox.x() / (double)originalFrame->width();
			yScalingFactor = (double)mouseData.selectionBox.y() / (double)originalFrame->height();
			wScalingFactor = (double)playerThread->getCurrentROI().width() / (double)originalFrame->width();
			hScalingFactor = (double)playerThread->getCurrentROI().height() / (double)originalFrame->height();
		}

		// Set selection box properties (new ROI)
		selectionBox.setX(xScalingFactor * playerThread->getCurrentROI().width() + playerThread->getCurrentROI().x());
		selectionBox.setY(yScalingFactor * playerThread->getCurrentROI().height() + playerThread->getCurrentROI().y());
		selectionBox.setWidth(wScalingFactor * mouseData.selectionBox.width());
		selectionBox.setHeight(hScalingFactor * mouseData.selectionBox.height());

		// Check if selection box has NON-ZERO dimensions
		if ((selectionBox.width() != 0) && ((selectionBox.height()) != 0)) {
			// Selection box can also be drawn from bottom-right to top-left corner
			if (selectionBox.width() < 0) {
				x_temp = selectionBox.x();
				width_temp = selectionBox.width();
				selectionBox.setX(x_temp + selectionBox.width());
				selectionBox.setWidth(width_temp * -1);
			}
			if (selectionBox.height() < 0) {
				y_temp = selectionBox.y();
				height_temp = selectionBox.height();
				selectionBox.setY(y_temp + selectionBox.height());
				selectionBox.setHeight(height_temp * -1);
			}

			// Check if selection box is not outside window
			if ((selectionBox.x() < 0) || (selectionBox.y() < 0) ||
			    ((selectionBox.x() + selectionBox.width()) > (playerThread->getCurrentROI().x() + playerThread->getCurrentROI().width())) ||
			    ((selectionBox.y() + selectionBox.height()) > (playerThread->getCurrentROI().y() + playerThread->getCurrentROI().height())) ||
			    (selectionBox.x() < playerThread->getCurrentROI().x()) ||
			    (selectionBox.y() < playerThread->getCurrentROI().y())) {
				// Display error message
				QMessageBox::warning(this, tr("ERROR:"), tr("Selection box outside range. Please try again."));
			}
			// Set ROI
			else
				emit setROI(selectionBox);
		}
	}
}

void VideoView::handleContextMenuAction(QAction *action)
{
	if (action->text() == "Reset ROI")
		emit setROI(QRect(0, 0, playerThread->getInputSourceWidth(), playerThread->getInputSourceHeight()));
	else if (action->text() == "Scale to Fit Frame") {
		ui->frameLabel->setScaledContents(action->isChecked());
		originalFrame->setScaledContents(action->isChecked());
	}else if (action->text() == "Show Original Frame")
		handleOriginalWindow(action->isChecked());
}

void VideoView::handleOriginalWindow(bool doEmit)
{
	originalFrame->setVisible(doEmit);
	playerThread->getOriginalFrame(doEmit);
}

// Actions for Player by User Input
void VideoView::play()
{
	// Video is currently stopped or paused, so play and switch button for pausing
	if (playerThread->isStopping() || playerThread->isPausing()) {
		playerThread->playAction();
		ui->PlayButton->setText(tr("Pause"));
		ui->PlayButton->setIcon(QIcon(":/main/image/appbar.control.pause.png"));
	}
	// Video is currently beeing played, so pause it
	else if (playerThread->isPlaying()) {
		pause();
	}
}

void VideoView::stop()
{
	ui->PlayButton->setText(tr("Play"));
	ui->PlayButton->setIcon(QIcon(":/main/image/appbar.control.play.png"));
	ui->TimeSlider->setValue(0);
	ui->currentTimeLabel->setText(getFormattedTime(0));
	playerThread->stopAction();
}

void VideoView::pause()
{
	ui->PlayButton->setText(tr("Play"));
	ui->PlayButton->setIcon(QIcon(":/main/image/appbar.control.play.png"));
	playerThread->pauseAction();
}

void VideoView::setTime()
{
	int val = ui->TimeSlider->value();
	playerThread->setCurrentFrame(val);

	if (ui->PlayButton->text() == "Pause") {
		playerThread->playAction();
	}
}

void VideoView::hideSettings()
{
	if (ui->tabWidget->isHidden()) {
		ui->tabWidget->show();
		ui->hideSettingsButton->setText("Hide Settings");
	}else {
		ui->tabWidget->hide();
		ui->hideSettingsButton->setText("Show Settings");
	}
}

//Save Video
void VideoView::save_action()
{
	if (vidSaver->isSaving()) {
		vidSaver->stop();
		return;
	}

	std::string source = file.absoluteFilePath().toStdString();
	QString userInput = QFileDialog::getSaveFileName(this,
							 tr("Save Capture"),
							 ".",
							 tr("Video File (*.avi *.mov *.mpeg *.mp4 *.m4v *.mkv)"));
	if (userInput.isEmpty()) {
		return;
	}
	std::string destination = userInput.toStdString();

	// First, load the file about to be processed
	if (vidSaver->loadFile(source)) {
		// Second, hand over the Settings for magnification
		//imageProcessingFlags = magnifyOptionsTab->getFlags();

		//Set Codec
		int savingCodec = (useVideoCodec) ? vidSaver->getVideoCodec() : codec;
		vidSaver->savingCodec = savingCodec;

		//vidSaver->settings(magnifyOptionsTab->getFlags(), magnifyOptionsTab->getSettings());
		// Third, start saving if destination is valid
		//if (vidSaver->saveFile(destination, playerThread->getFPS(), playerThread->getCurrentROI(), ui->saveOriginalCheckBox->checkState())) {
		if (vidSaver->saveFile(destination, playerThread->getFPS(), playerThread->getCurrentROI(), false)) {
			// Show progressbar and set max value (= nr of frames of video)
			ui->saveProgressBar->show();
			ui->saveProgressBar->setMaximum(vidSaver->getVideoLength());
			ui->saveButton->setText("Abort saving");
			ui->saveButton->setChecked(true);

			// Stop  player Thread to set memory free
			stop();
			// start saving the video
			vidSaver->start();
		}else
			QMessageBox::warning(this->parentWidget(), "WARNING:", "Please enter a valid filename and -ending or change Codec (File->Saving Codec)");
	}else
		QMessageBox::warning(this->parentWidget(), "WARNING:", "Not able to load video");
}

void VideoView::endOfSaving_action()
{
	ui->saveButton->setText("Save");
	ui->saveButton->setChecked(false);
	ui->saveProgressBar->setValue(0);
	ui->saveProgressBar->hide();
}

void VideoView::updateProgressBar(int frame)
{
	ui->saveProgressBar->setValue(frame);
}

void VideoView::handleTabChange(int index)
{
	if (index == 1)
		ui->InfoTab->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
	else
		ui->InfoTab->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Ignored);
}

void VideoView::setCodec(int codec)
{
	this->codec = codec;
}

void VideoView::set_useVideoCodec(bool use)
{
	this->useVideoCodec = use;
}

//Gray
void VideoView::on_checkBoxGrayscale_clicked(bool checked)
{
	imgProcFlags.grayscaleOn = checked;
	emit newImageProcessingFlags(imgProcFlags);
}


//hsvHistogram
void VideoView::on_checkBoxHsvHistogram_clicked(bool checked)
{
	imgProcFlags.hsvHistogramOn = checked;

	ui->tabSettings->setCurrentWidget(ui->tabHSV);
	ui->tabSettings->setEnabled(checked);

	ui->tabHSV->setEnabled(checked);
	ui->sliderHue->setEnabled(checked);
	ui->sliderSat->setEnabled(checked);
	ui->sliderVal->setEnabled(checked);
	emit newImageProcessingFlags(imgProcFlags);
}

//Blur/Smooth/Morph
void VideoView::on_checkBoxBlurMode_clicked(bool checked)
{
	imgProcFlags.blurOn = checked;

	ui->tabSettings->setCurrentWidget(ui->tabBlur);
	ui->tabSettings->setEnabled(checked);
	ui->tabBlur->setEnabled(checked);

	ui->groupBoxBlurMode->setEnabled(imgProcFlags.blurOn);
	if (imgProcFlags.blurOn && ui->buttonMorph->isChecked()) ui->groupBoxMorph->setEnabled(true);
	else ui->groupBoxMorph->setEnabled(false);
	emit newImageProcessingFlags(imgProcFlags);
}

//Dilate
void VideoView::on_checkBoxDilate_clicked(bool checked)
{
	imgProcFlags.dilateOn = checked;
	ui->tabSettings->setCurrentWidget(ui->tabBlur);
	ui->tabSettings->setEnabled(checked);

	if (imgProcFlags.dilateOn) ui->tabBlur->setEnabled(true);
	else if (!imgProcFlags.erodeOn) ui->tabBlur->setEnabled(false);
	emit newImageProcessingFlags(imgProcFlags);
}

//Erode
void VideoView::on_checkBoxErode_clicked(bool checked)
{
	imgProcFlags.erodeOn = ui->checkBoxErode->isChecked();
	ui->tabSettings->setCurrentWidget(ui->tabBlur);
	ui->tabSettings->setEnabled(checked);

	if (imgProcFlags.erodeOn) ui->tabBlur->setEnabled(true);
	else if (!imgProcFlags.dilateOn) ui->tabBlur->setEnabled(false);
	emit newImageProcessingFlags(imgProcFlags);
}

//Canny
void VideoView::on_checkBoxCanny_clicked(bool checked)
{
	imgProcFlags.cannyOn = ui->checkBoxCanny->isChecked();
	ui->tabSettings->setCurrentWidget(ui->tabCanny);
	ui->tabSettings->setEnabled(checked);

	ui->tabCanny->setEnabled(checked);

	emit newImageProcessingFlags(imgProcFlags);
}

//PCA
void VideoView::on_checkBoxPCA_clicked(bool checked)
{
	imgProcFlags.pcaOn = checked;
	emit newImageProcessingFlags(imgProcFlags);
}

void VideoView::on_checkBoxGrabCut_clicked(bool checked)
{
	imgProcFlags.grabcutOn = checked;
	emit newImageProcessingFlags(imgProcFlags);
}
//-----------------------------------------------------------------------------
// Settings
//-----------------------------------------------------------------------------
void VideoView::on_sliderThreshold_lowerValueChanged(int value)
{
	imgSettings.cannyThreshold1 = int(value * 2.55);
	QString sThreshold = QString("Threashold\n %1~%2").arg(imgSettings.cannyThreshold1, 3).arg(imgSettings.cannyThreshold2, 3);
	ui->labelThreshold->setWordWrap(true);
	ui->labelThreshold->setText(sThreshold);

	//qDebug() << "Lower" << value;
	emit newProcessingSettings(imgSettings);
}
void VideoView::on_sliderThreshold_upperValueChanged(int value)
{
	imgSettings.cannyThreshold2 = int(value * 2.55);
	QString sThreshold = QString("Threashold\n %1~%2").arg(imgSettings.cannyThreshold1, 3).arg(imgSettings.cannyThreshold2, 3);
	ui->labelThreshold->setWordWrap(true);
	ui->labelThreshold->setText(sThreshold);

	//qDebug() << "Upper" << value;
	emit newProcessingSettings(imgSettings);
}

void VideoView::on_spinBoxAperture_valueChanged(int arg1)
{
	if ((arg1 <= 7) && (arg1 >= 3) && (arg1 % 2)) {
		imgSettings.cannyApertureSize = arg1;
		emit newProcessingSettings(imgSettings);
	}
}

void VideoView::on_checkBoxL2Gradient_clicked(bool checked)
{
	imgSettings.cannyL2gradient = checked;
	emit newProcessingSettings(imgSettings);
}

void VideoView::on_horizontalSliderDilate_valueChanged(int value)
{
	imgSettings.dilateNumberOfIterations = value;
	ui->labelDilate->setText(QString("Dilate %1").arg(value));
	emit newProcessingSettings(imgSettings);
}

void VideoView::on_horizontalSliderErode_valueChanged(int value)
{
	imgSettings.erodeNumberOfIterations = value;
	ui->labelErode->setText(QString("Erode %1").arg(value));
	emit newProcessingSettings(imgSettings);
}

void VideoView::on_sliderHue_lowerValueChanged(int value)
{
	imgSettings.hsvHueLow = int(value * 1.8);
	QString sHue = QString("Hue\n %1~%2").arg(imgSettings.hsvHueLow, 3).arg(imgSettings.hsvHueHigh, 3);
	ui->labelHue->setText(sHue);
	emit newProcessingSettings(imgSettings);
}
void VideoView::on_sliderHue_upperValueChanged(int value)
{
	imgSettings.hsvHueHigh = int(value * 1.8);
	QString sHue = QString("Hue\n %1~%2").arg(imgSettings.hsvHueLow, 3).arg(imgSettings.hsvHueHigh, 3);
	ui->labelHue->setText(sHue);
	emit newProcessingSettings(imgSettings);
}

void VideoView::on_sliderSat_lowerValueChanged(int value)
{
	imgSettings.hsvSatLow = int(value * 2.55);
	QString sSat = QString("Sat\n %1~%2").arg(imgSettings.hsvSatLow, 3).arg(imgSettings.hsvSatHigh, 3);
	ui->labelSat->setText(sSat);
	emit newProcessingSettings(imgSettings);
}
void VideoView::on_sliderSat_upperValueChanged(int value)
{
	imgSettings.hsvSatHigh = int(value * 2.55);
	QString sSat = QString("Sat\n %1~%2").arg(imgSettings.hsvSatLow, 3).arg(imgSettings.hsvSatHigh, 3);
	ui->labelSat->setText(sSat);
	emit newProcessingSettings(imgSettings);
}
void VideoView::on_sliderVal_lowerValueChanged(int value)
{
	imgSettings.hsvValLow = int(value * 2.55);
	QString sVal = QString("Val\n %1~%2").arg(imgSettings.hsvValLow, 3).arg(imgSettings.hsvValHigh, 3);
	ui->labelVal->setText(sVal);
	emit newProcessingSettings(imgSettings);
}
void VideoView::on_sliderVal_upperValueChanged(int value)
{
	imgSettings.hsvValHigh = int(value * 2.55);
	QString sVal = QString("Val\n %1~%2").arg(imgSettings.hsvValLow, 3).arg(imgSettings.hsvValHigh, 3);
	ui->labelVal->setText(sVal);
	emit newProcessingSettings(imgSettings);
}

void VideoView::on_buttonMedian_clicked(bool checked)
{
	imgSettings.blurType = 0;
	emit newProcessingSettings(imgSettings);
	ui->groupBoxMorph->setEnabled(false);
}

void VideoView::on_buttonGaussian_clicked(bool checked)
{
	imgSettings.blurType = 1;
	emit newProcessingSettings(imgSettings);
	ui->groupBoxMorph->setEnabled(false);
}

void VideoView::on_buttonMorph_clicked(bool checked)
{
	imgSettings.blurType = 2;
	emit newProcessingSettings(imgSettings);
	ui->groupBoxMorph->setEnabled(true);
}

void VideoView::on_pushButtonShot_clicked()
{
	QString fname = MyUtils::stringMyFile(MyUtils::stringMyTime(), "jpg");
	qDebug() << "folder:" << MyUtils::stringMyFolder();
	qDebug() << "fname: " << fname;

	//const QPixmap *pm = ui->frameLabel->pixmap(Qt::ReturnByValueConstant); //deprecated!
	QPixmap pm = ui->frameLabel->pixmap(Qt::ReturnByValue);
	if (!pm.isNull()) {
		pm.save(fname, nullptr);

		//Append frame to listView
		QStandardItem * item = new QStandardItem();
		list_model->appendRow(item);
		QModelIndex index = list_model->indexFromItem(item);
		list_model->setData(index, QPixmap(fname).scaledToWidth(ui->listViewCapture->width() / 2), Qt::DecorationRole);
		list_model->setData(index, MyUtils::stringMyTime(), Qt::DisplayRole);
		ui->listViewCapture->scrollTo(index);
		ui->listViewCapture->setStyleSheet("QListView {background-color: white;}");
		ui->listViewCapture->setEnabled(true);
	}
}

void VideoView::on_buttonMorphOpen_clicked()
{
	imgSettings.morphOption = 0;
	emit newProcessingSettings(imgSettings);
}

void VideoView::on_buttonMorphClose_clicked()
{
	imgSettings.morphOption = 1;
	emit newProcessingSettings(imgSettings);
}

void VideoView::on_buttonMorphGradient_clicked()
{
	imgSettings.morphOption = 2;
	emit newProcessingSettings(imgSettings);
}

void VideoView::on_buttonShotSend_clicked()
{
	QPixmap pm = ui->frameLabel->pixmap(Qt::ReturnByValue);
	if (!pm.isNull()) {
		QString sIP = ui->lineEditIP->text();
		int iPort = ui->lineEditPort->text().toInt();
		if ((!sIP.isEmpty()) && (iPort > 0)) {
			myTcpSendPix = new TcpSendPix(sIP, iPort);

			myTcpSendPix->send(pm);
			qDebug() << "TcpSend:" << sIP << iPort;
		}
	}
}

void VideoView::on_checkBoxHsvEqualize_clicked(bool checked)
{
	imgProcFlags.hsvEqualizeOn = checked;

	emit newImageProcessingFlags(imgProcFlags);
}


