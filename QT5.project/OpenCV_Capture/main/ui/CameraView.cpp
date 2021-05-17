/************************************************************************************/
/* An OpenCV/Qt based realtime application			                    */
/*                                                                                  */
/*                                                                                  */
/* Based on the work of                                                             */
/*      Joseph Pan      <https://github.com/wzpan/QtEVM>                            */
/*      Nick D'Ademo    <https://github.com/nickdademo/qt-opencv-multithreaded>     */
/*                                                                                  */
/* ui/CameraView.cpp								    */
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

#include "main/ui/CameraView.h"
#include "ui_CameraView.h"
#include "helper/MyUtils.h"

CameraView::CameraView(QWidget *parent, int deviceNumber, SharedImageBuffer *sharedImageBuffer) :
	QWidget(parent),
	ui(new Ui::CameraView),
	sharedImageBuffer(sharedImageBuffer),
	codec(-1)
{
	// Setup UI
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

	// Save Device Number
	this->deviceNumber = deviceNumber;
	// Initialize internal flag
	isCameraConnected = false;
	// Set initial GUI state
	ui->frameLabel->setText(tr("No camera connected."));
	ui->imageBufferBar->setValue(0);
	ui->imageBufferLabel->setText("[000/000]");
	ui->captureRateLabel->setText("");
	ui->processingRateLabel->setText("");
	ui->deviceNumberLabel->setText("");
	ui->cameraResolutionLabel->setText("");
	ui->roiLabel->setText("");
	ui->mouseCursorPosLabel->setText("");
	ui->clearImageBufferButton->setDisabled(true);

	ui->recordPathEdit->setText(MyUtils::stringMyFolder());
	ui->listViewCapture->setStyleSheet("QListView {background-color: lightGray;}");
	//Setup model for listView
	list_model = new QStandardItemModel(this);
	ui->listViewCapture->setModel(list_model);
	ui->listViewCapture->setViewMode(QListView::IconMode);
	ui->listViewCapture->setFlow(QListView::LeftToRight);
	ui->listViewCapture->setUniformItemSizes(true);
	//ui->listViewCapture->setFixedWidth(mOpenCV_videoCapture->CamWidth/2);

	// Connect signals/slots
	connect(ui->hideSettingsButton, SIGNAL(released()), this, SLOT(hideSettings()));
	connect(ui->frameLabel, SIGNAL(onMouseMoveEvent()), this, SLOT(updateMouseCursorPosLabel()));
	connect(originalFrame, SIGNAL(onMouseMoveEvent()), this, SLOT(updateMouseCursorPosLabelOriginalFrame()));
	connect(ui->clearImageBufferButton, SIGNAL(released()), this, SLOT(clearImageBuffer()));
	connect(ui->frameLabel->menu, SIGNAL(triggered(QAction*)), this, SLOT(handleContextMenuAction(QAction*)));

	// Register type
	qRegisterMetaType<struct ThreadStatisticsData>("ThreadStatisticsData");

	// Initial settings & flags
	imgProcSettings.flipcode = 1;
	imgProcSettings.blurType = 0;
	imgProcSettings.morphOption = 0;
	imgProcFlags.grayscaleOn = false;
	imgProcFlags.blurOn = false;
	imgProcFlags.flipOn = false;
	imgProcFlags.colorcheckerOn = false;
}


CameraView::~CameraView()
{
	if (isCameraConnected) {
		// Stop processing thread
		if (processingThread->isRunning())
			stopProcessingThread();
		// Stop capture thread
		if (captureThread->isRunning())
			stopCaptureThread();
		// Remove from shared buffer
		sharedImageBuffer->removeByDeviceNumber(deviceNumber);
		// Disconnect camera
		if (captureThread->disconnectCamera())
			qDebug() << "[" << deviceNumber << "] Camera successfully disconnected.";
		else
			qDebug() << "[" << deviceNumber << "] WARNING: Camera already disconnected.";
	}
	// Delete UI
	delete processingThread;
	delete captureThread;
	delete originalFrame;
	delete ui;
}

bool CameraView::connectToCamera(bool dropFrameIfBufferFull, int capThreadPrio, int procThreadPrio,
				 int width, int height, int fps)
{
	ui->frameLabel->setText(tr("Connecting to camera..."));

	// Create capture thread
	captureThread = new CaptureThread(sharedImageBuffer, deviceNumber, dropFrameIfBufferFull, width, height, fps);
	// Attempt to connect to camera
	if (captureThread->connectToCamera()) {
		// Create processing thread
		processingThread = new ProcessingThread(sharedImageBuffer, deviceNumber);

		// Create MagnifyOptions tab and set current
		//this->magnifyOptionsTab = new MagnifyOptions(this);
		//ui->tabWidget->insertTab(0, magnifyOptionsTab, tr("Options"));
		ui->tabWidget->setCurrentIndex(0);
		ui->InfoTab->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Ignored);

		// Setup signal/slot connections
		connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(handleTabChange(int)));
		connect(processingThread, SIGNAL(newFrame(QImage)), this, SLOT(updateFrame(QImage)));
		connect(processingThread, SIGNAL(origFrame(QImage)), this, SLOT(updateOriginalFrame(QImage)));
		connect(processingThread, SIGNAL(updateStatisticsInGUI(ThreadStatisticsData)), this, SLOT(updateProcessingThreadStats(ThreadStatisticsData)));
		connect(captureThread, SIGNAL(updateStatisticsInGUI(ThreadStatisticsData)), this, SLOT(updateCaptureThreadStats(ThreadStatisticsData)));
		connect(captureThread, SIGNAL(updateFramerate(double)), processingThread, SLOT(updateFramerate(double)));
		connect(this, SIGNAL(newImageProcessingFlags(ImageProcessingFlags)), processingThread, SLOT(updateImageProcessingFlags(ImageProcessingFlags)));
		connect(this, SIGNAL(newProcessingSettings(ImageProcessingSettings)), processingThread, SLOT(updateProcessingSettings(ImageProcessingSettings)));

		connect(this, SIGNAL(setROI(QRect)), processingThread, SLOT(setROI(QRect)));
		//connect(processingThread, SIGNAL(maxLevels(int)), magnifyOptionsTab, SLOT(setMaxLevel(int)));
		connect(ui->recordButton, SIGNAL(released()), this, SLOT(record()));
		connect(ui->recordPathButton, SIGNAL(released()), this, SLOT(selectButton_action()));
		connect(processingThread, SIGNAL(frameWritten(int)), this, SLOT(frameWritten(int)));

		// Setup signal/slot connections for MagnifyOptions
		//connect(magnifyOptionsTab, SIGNAL(newImageProcessingFlags(struct ImageProcessingFlags)), processingThread, SLOT(updateImageProcessingFlags(struct ImageProcessingFlags)));
		//connect(magnifyOptionsTab, SIGNAL(newImageProcessingSettings(struct ImageProcessingSettings)), processingThread, SLOT(updateImageProcessingSettings(struct ImageProcessingSettings)));

		connect(ui->frameLabel, SIGNAL(newMouseData(struct MouseData)), this, SLOT(newMouseData(struct MouseData)));
		connect(originalFrame, SIGNAL(newMouseData(struct MouseData)), this, SLOT(newMouseData(struct MouseData)));

		// Set initial data in processing thread
		emit setROI(QRect(0, 0, captureThread->getInputSourceWidth(), captureThread->getInputSourceHeight()));
		emit newImageProcessingFlags(imageProcessingFlags);

		// Start capturing frames from camera
		captureThread->start((QThread::Priority)capThreadPrio);
		// Start processing captured frames
		processingThread->start((QThread::Priority)procThreadPrio);

		// Setup imageBufferBar with minimum and maximum values
		ui->imageBufferBar->setMinimum(0);
		ui->imageBufferBar->setMaximum(sharedImageBuffer->getByDeviceNumber(deviceNumber)->maxSize());
		// Enable "Clear Image Buffer" push button
		ui->clearImageBufferButton->setEnabled(true);
		// Set text in labels
		ui->deviceNumberLabel->setNum(deviceNumber);
		ui->cameraResolutionLabel->setText(QString::number(captureThread->getInputSourceWidth()) + QString("x") + QString::number(captureThread->getInputSourceHeight()));
		// Set internal flag and return
		isCameraConnected = true;

		return true;
	}
	// Failed to connect to camera
	else
		return false;
}

void CameraView::stopCaptureThread()
{
	qDebug() << "[" << deviceNumber << "] About to stop capture thread...";
	captureThread->stop();
	sharedImageBuffer->wakeAll(); // This allows the thread to be stopped if it is in a wait-state
	// Take one frame off a FULL queue to allow the capture thread to finish
	if (sharedImageBuffer->getByDeviceNumber(deviceNumber)->isFull())
		sharedImageBuffer->getByDeviceNumber(deviceNumber)->get();
	captureThread->wait();
	qDebug() << "[" << deviceNumber << "] Capture thread successfully stopped.";
}

void CameraView::stopProcessingThread()
{
	qDebug() << "[" << deviceNumber << "] About to stop processing thread...";
	processingThread->stop();
	sharedImageBuffer->wakeAll(); // This allows the thread to be stopped if it is in a wait-state
	processingThread->wait();
	qDebug() << "[" << deviceNumber << "] Processing thread successfully stopped.";
}

void CameraView::updateCaptureThreadStats(struct ThreadStatisticsData statData)
{
	// Show [number of images in buffer / image buffer size] in imageBufferLabel
	ui->imageBufferLabel->setText(QString("[") + QString::number(sharedImageBuffer->getByDeviceNumber(deviceNumber)->size()) +
				      QString("/") + QString::number(sharedImageBuffer->getByDeviceNumber(deviceNumber)->maxSize()) + QString("]"));
	// Show percentage of image bufffer full in imageBufferBar
	ui->imageBufferBar->setValue(sharedImageBuffer->getByDeviceNumber(deviceNumber)->size());

	// Show processing rate in captureRateLabel
	ui->captureRateLabel->setText(QString::number(statData.averageFPS) + " fps");
	// Show number of frames captured in nFramesCapturedLabel
	ui->nFramesCapturedLabel->setText(QString("[") + QString::number(statData.nFramesProcessed) + QString("]"));
}

void CameraView::updateProcessingThreadStats(struct ThreadStatisticsData statData)
{
	// Show processing rate in processingRateLabel
	ui->processingRateLabel->setText(QString::number(statData.averageFPS) + " fps");
	// Show ROI information in roiLabel
	ui->roiLabel->setText(QString("(") + QString::number(processingThread->getCurrentROI().x()) + QString(",") +
			      QString::number(processingThread->getCurrentROI().y()) + QString(") ") +
			      QString::number(processingThread->getCurrentROI().width()) +
			      QString("x") + QString::number(processingThread->getCurrentROI().height()));
	// Show number of frames processed in nFramesProcessedLabel
	ui->nFramesProcessedLabel->setText(QString("[") + QString::number(statData.nFramesProcessed) + QString("]"));
}

void CameraView::updateFrame(const QImage &frame)
{
	// Display frame
	ui->frameLabel->setPixmap(QPixmap::fromImage(frame).scaled(ui->frameLabel->width(), ui->frameLabel->height(), Qt::KeepAspectRatio));
}

void CameraView::updateOriginalFrame(const QImage &frame)
{
	// Display frame
	originalFrame->setPixmap(QPixmap::fromImage(frame).scaled(ui->frameLabel->width(), ui->frameLabel->height(), Qt::KeepAspectRatio));
}

void CameraView::handleOriginalWindow(bool doEmit)
{
	originalFrame->setVisible(doEmit);
	processingThread->getOriginalFrame(doEmit);
}

void CameraView::clearImageBuffer()
{
	if (sharedImageBuffer->getByDeviceNumber(deviceNumber)->clear())
		qDebug() << "[" << deviceNumber << "] Image buffer successfully cleared.";
	else
		qDebug() << "[" << deviceNumber << "] WARNING: Could not clear image buffer.";
}

void CameraView::updateMouseCursorPosLabel()
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
							 QString(" [") + QString::number((int)(xScalingFactor * processingThread->getCurrentROI().width())) +
							 QString(",") + QString::number((int)(yScalingFactor * processingThread->getCurrentROI().height())) +
							 QString("]"));
		}else {
			double xScalingFactor = (double)ui->frameLabel->getMouseCursorPos().x() / (double)ui->frameLabel->width();
			double yScalingFactor = (double)ui->frameLabel->getMouseCursorPos().y() / (double)ui->frameLabel->height();

			ui->mouseCursorPosLabel->setText(ui->mouseCursorPosLabel->text() +
							 QString(" [") + QString::number((int)(xScalingFactor * processingThread->getCurrentROI().width())) +
							 QString(",") + QString::number((int)(yScalingFactor * processingThread->getCurrentROI().height())) +
							 QString("]"));
		}
	}
}

void CameraView::newMouseData(struct MouseData mouseData)
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
			wScalingFactor = (double)processingThread->getCurrentROI().width() / (double)ui->frameLabel->pixmap()->width();
			hScalingFactor = (double)processingThread->getCurrentROI().height() / (double)ui->frameLabel->pixmap()->height();
		}else {
			xScalingFactor = (double)mouseData.selectionBox.x() / (double)ui->frameLabel->width();
			yScalingFactor = (double)mouseData.selectionBox.y() / (double)ui->frameLabel->height();
			wScalingFactor = (double)processingThread->getCurrentROI().width() / (double)ui->frameLabel->width();
			hScalingFactor = (double)processingThread->getCurrentROI().height() / (double)ui->frameLabel->height();
		}

		// Set selection box properties (new ROI)
		selectionBox.setX(xScalingFactor * processingThread->getCurrentROI().width() + processingThread->getCurrentROI().x());
		selectionBox.setY(yScalingFactor * processingThread->getCurrentROI().height() + processingThread->getCurrentROI().y());
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
			    ((selectionBox.x() + selectionBox.width()) > (processingThread->getCurrentROI().x() + processingThread->getCurrentROI().width())) ||
			    ((selectionBox.y() + selectionBox.height()) > (processingThread->getCurrentROI().y() + processingThread->getCurrentROI().height())) ||
			    (selectionBox.x() < processingThread->getCurrentROI().x()) ||
			    (selectionBox.y() < processingThread->getCurrentROI().y())) {
				// Display error message
				QMessageBox::warning(this, tr("ERROR:"), tr("Selection box outside range. Please try again."));
			}
			// Set ROI
			else if (!processingThread->isRecording())
				emit setROI(selectionBox);
		}
	}
}

void CameraView::updateMouseCursorPosLabelOriginalFrame()
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
							 QString(" [") + QString::number((int)(xScalingFactor * processingThread->getCurrentROI().width())) +
							 QString(",") + QString::number((int)(yScalingFactor * processingThread->getCurrentROI().height())) +
							 QString("]"));
		}else {
			double xScalingFactor = (double)originalFrame->getMouseCursorPos().x() / (double)originalFrame->width();
			double yScalingFactor = (double)originalFrame->getMouseCursorPos().y() / (double)originalFrame->height();

			ui->mouseCursorPosLabel->setText(ui->mouseCursorPosLabel->text() +
							 QString(" [") + QString::number((int)(xScalingFactor * processingThread->getCurrentROI().width())) +
							 QString(",") + QString::number((int)(yScalingFactor * processingThread->getCurrentROI().height())) +
							 QString("]"));
		}
	}
}

void CameraView::newMouseDataOriginalFrame(struct MouseData mouseData)
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
			wScalingFactor = (double)processingThread->getCurrentROI().width() / (double)originalFrame->pixmap()->width();
			hScalingFactor = (double)processingThread->getCurrentROI().height() / (double)originalFrame->pixmap()->height();
		}else {
			xScalingFactor = (double)mouseData.selectionBox.x() / (double)originalFrame->width();
			yScalingFactor = (double)mouseData.selectionBox.y() / (double)originalFrame->height();
			wScalingFactor = (double)processingThread->getCurrentROI().width() / (double)originalFrame->width();
			hScalingFactor = (double)processingThread->getCurrentROI().height() / (double)originalFrame->height();
		}

		// Set selection box properties (new ROI)
		selectionBox.setX(xScalingFactor * processingThread->getCurrentROI().width() + processingThread->getCurrentROI().x());
		selectionBox.setY(yScalingFactor * processingThread->getCurrentROI().height() + processingThread->getCurrentROI().y());
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
			    ((selectionBox.x() + selectionBox.width()) > (processingThread->getCurrentROI().x() + processingThread->getCurrentROI().width())) ||
			    ((selectionBox.y() + selectionBox.height()) > (processingThread->getCurrentROI().y() + processingThread->getCurrentROI().height())) ||
			    (selectionBox.x() < processingThread->getCurrentROI().x()) ||
			    (selectionBox.y() < processingThread->getCurrentROI().y())) {
				// Display error message
				QMessageBox::warning(this, tr("ERROR:"), tr("Selection box outside range. Please try again."));
			}
			// Set ROI
			else if (!processingThread->isRecording())
				emit setROI(selectionBox);
		}
	}
}

QString CameraView::getFormattedTime(int timeInMSeconds)
{
	int seconds = (int)(timeInMSeconds) % 60;
	int minutes = (int)((timeInMSeconds / 60) % 60);
	int hours = (int)((timeInMSeconds / (60 * 60)) % 24);

	QTime t(hours, minutes, seconds);
	if (hours == 0)
		return t.toString("mm:ss");
	else
		return t.toString("h:mm:ss");
}

void CameraView::handleContextMenuAction(QAction *action)
{
	if (action->text() == "Reset ROI" && !processingThread->isRecording())
		emit setROI(QRect(0, 0, captureThread->getInputSourceWidth(), captureThread->getInputSourceHeight()));
	else if (action->text() == "Scale to Fit Frame") {
		ui->frameLabel->setScaledContents(action->isChecked());
		originalFrame->setScaledContents(action->isChecked());
	}else if (action->text() == "Show Original Frame")
		handleOriginalWindow(action->isChecked());
}

// Hide the lower Tab (Setting and Streaminfo)
void CameraView::hideSettings()
{
	if (ui->tabWidget->isHidden()) {
		ui->tabWidget->show();
		ui->hideSettingsButton->setText(tr("Hide Settings"));
	}else {
		ui->tabWidget->hide();
		ui->hideSettingsButton->setText(tr("Show Settings"));
	}
}

// Handle functionality of recordButton
void CameraView::record()
{
	std::string recordPath = (ui->recordPathEdit->text()).toStdString();
	if (processingThread->isRecording()) {
		processingThread->stopRecord();
		ui->recordButton->setText(tr("Record"));
		//magnifyOptionsTab->toggleGrayscale(true);
		//ui->recordOriginalCheckbox->setDisabled(false);
	}else {
		if (recordPath.empty()) {
			QMessageBox::warning(this->parentWidget(), tr("WARNING:"), tr("Please enter a filepath"));
		}else {
			processingThread->savingCodec = codec;
			//if (processingThread->startRecord(recordPath, ui->recordOriginalCheckbox->isChecked())) {
			if (processingThread->startRecord(recordPath, false)) {
				ui->recordButton->setText(tr("Stop"));
				//magnifyOptionsTab->toggleGrayscale(false);
				//ui->recordOriginalCheckbox->setDisabled(true);
			}else
				QMessageBox::warning(this->parentWidget(), tr("WARNING:"), tr("Please enter a valid filename and -ending or change Codec (File->Saving Codec)"));
		}
	}
}

// Update Gui for every written frame
void CameraView::frameWritten(int frames)
{
	int currentSecond = frames / processingThread->getRecordFPS();
	ui->recordButton->setText("Stop (" + getFormattedTime(currentSecond) + ")");
}

// Action to search for file via "Open" Button
void CameraView::selectButton_action()
{
	QString fileName = QFileDialog::getSaveFileName(this,
							tr("Save Capture"),
							".",
							tr("Video File (*.avi *.mov *.mpeg *.mp4 *.mkv)"));
	if (!fileName.isEmpty()) {
		ui->recordPathEdit->setText(fileName);
	}
}

void CameraView::handleTabChange(int index)
{
	if (index == 1)
		ui->InfoTab->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
	else
		ui->InfoTab->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Ignored);
}

void CameraView::setCodec(int codec)
{
	this->codec = codec;
	processingThread->savingCodec = codec;
}

//Gray
void CameraView::on_checkBoxGrayscale_clicked(bool checked)
{
	imgProcFlags.grayscaleOn = checked;
	emit newImageProcessingFlags(imgProcFlags);
}


//hsvHistogram
void CameraView::on_checkBoxHsvHistogram_clicked(bool checked)
{
	imgProcFlags.hsvHistogramOn = checked;
	//ui->tabHSV->setEnabled(imgProcFlags.hsvHistogramOn);
	//ui->tabSettings->setCurrentWidget(ui->tabHSV);
	emit newImageProcessingFlags(imgProcFlags);
}

//Flip
void CameraView::on_checkBoxHFlip_clicked(bool checked)
{
	imgProcFlags.flipOn = checked;
	ui->groupBoxFlipMode->setEnabled(imgProcFlags.flipOn);
	emit newImageProcessingFlags(imgProcFlags);
}
void CameraView::on_flipXAxisButton_clicked()
{// Options: [x-axis=0,y-axis=1,both axes=-1]
	imgProcSettings.flipcode = 0;
	emit newProcessingSettings(imgProcSettings);
}
void CameraView::on_flipYAxisButton_clicked()
{
	imgProcSettings.flipcode = 1;
	emit newProcessingSettings(imgProcSettings);
}
void CameraView::on_flipBothAxesButton_clicked()
{
	imgProcSettings.flipcode = -1;
	emit newProcessingSettings(imgProcSettings);
}

//Macbeth colorchecker
void CameraView::on_checkBoxColorChecker_clicked(bool checked)
{
	imgProcFlags.colorcheckerOn = checked;
	emit newImageProcessingFlags(imgProcFlags);
}

//MeanShift
void CameraView::on_checkBoxMeanShift_clicked(bool checked)
{
	imgProcFlags.meanshiftOn = checked;
	emit newImageProcessingFlags(imgProcFlags);
}


void CameraView::on_checkBoxGrabCut_clicked(bool checked)
{
	imgProcFlags.grabcutOn = checked;
	emit newImageProcessingFlags(imgProcFlags);
}

//Blur/smooth/Morph
void CameraView::on_checkBoxBlurMode_clicked(bool checked)
{
	imgProcFlags.blurOn = checked;
	//ui->tabSettings->setCurrentWidget(ui->tabBlur);
	ui->groupBoxBlurMode->setEnabled(imgProcFlags.blurOn);

	if (imgProcFlags.blurOn && ui->buttonMorph->isChecked()) ui->groupBoxMorph->setEnabled(true);
	else ui->groupBoxMorph->setEnabled(false);

	emit newImageProcessingFlags(imgProcFlags);
}
void CameraView::on_buttonMedian_clicked()
{
	imgProcSettings.blurType = 0;
	ui->groupBoxMorph->setEnabled(false);
	emit newProcessingSettings(imgProcSettings);
}
void CameraView::on_buttonGaussian_clicked()
{
	imgProcSettings.blurType = 1;
	ui->groupBoxMorph->setEnabled(false);
	emit newProcessingSettings(imgProcSettings);
}
void CameraView::on_buttonMorph_clicked()
{
	imgProcSettings.blurType = 2;
	ui->groupBoxMorph->setEnabled(true);
	emit newProcessingSettings(imgProcSettings);
}


void CameraView::on_pushButtonShot_clicked()
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


void CameraView::on_buttonMorphOpen_clicked()
{
	imgProcSettings.morphOption = 0;
	emit newProcessingSettings(imgProcSettings);
}

void CameraView::on_buttonMorphClose_clicked()
{
	imgProcSettings.morphOption = 1;
	emit newProcessingSettings(imgProcSettings);
}

void CameraView::on_buttonMorphGradient_clicked()
{
	imgProcSettings.morphOption = 2;
	emit newProcessingSettings(imgProcSettings);
}

void CameraView::on_buttonShotSendCam_clicked()
{
	QPixmap pm = ui->frameLabel->pixmap(Qt::ReturnByValue);
	if (!pm.isNull()) {
		QString sIP = ui->lineEditIPCam->text();
		int iPort = ui->lineEditPortCam->text().toInt();
		if ((!sIP.isEmpty()) && (iPort > 0)) {
			myTcpSendPix = new TcpSendPix(sIP, iPort);

			myTcpSendPix->send(pm);
			qDebug() << "TcpSend:" << sIP << iPort;
		}
	}
}




