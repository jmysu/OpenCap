/************************************************************************************/
/* An OpenCV/Qt based realtime application			                    */
/*                                                                                  */
/*                                                                                  */
/* Based on the work of                                                             */
/*      Joseph Pan      <https://github.com/wzpan/QtEVM>                            */
/*      Nick D'Ademo    <https://github.com/nickdademo/qt-opencv-multithreaded>     */
/*                                                                                  */
/* threads/ProcessingThread.cpp							    */
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

#include "main/threads/ProcessingThread.h"

ProcessingThread::ProcessingThread(SharedImageBuffer *sharedImageBuffer, int deviceNumber) : QThread(),
	sharedImageBuffer(sharedImageBuffer),
	emitOriginal(false)
{
	// Save Device Number
	this->deviceNumber = deviceNumber;
	// Initialize members
	doStop = false;
	doRecord = false;
	sampleNumber = 0;
	fpsSum = 0;
	framesWritten = 0;
	fps.clear();
	statsData.averageFPS = 0;
	statsData.nFramesProcessed = 0;
	captureOriginal = false;

	this->processingBufferLength = 2;
	//this->magnificator = Magnificator(&processingBuffer, &imgProcFlags, &imgProcSettings);
	this->output = VideoWriter();
}

// Destructor
ProcessingThread::~ProcessingThread()
{
	doStopMutex.lock();
	doStop = true;
	if (releaseCapture())
		qDebug() << "Released Capture";

	processingBuffer.clear();
	doStopMutex.unlock();
	wait();
}

// Release videoCapture if available
bool ProcessingThread::releaseCapture()
{
	if (output.isOpened()) {
		// Release Video
		output.release();
		return true;
	}
	// There was no video
	else
		return false;
}


void ProcessingThread::run()
{
	qDebug() << "Starting processing thread...";
	while (1) {
		////////////////////////// ///////
		// Stop thread if doStop=TRUE //
		////////////////////////// ///////
		doStopMutex.lock();
		if (doStop) {
			doStop = false;
			doStopMutex.unlock();
			break;
		}
		doStopMutex.unlock();
		////////////////////////// ////////
		////////////////////////// ////////

		// Save processing time
		processingTime = t.elapsed();
		// Start timer (used to calculate processing rate)
		t.start();

		processingMutex.lock();
		// Get frame from queue, store in currentFrame, set ROI
		currentFrame = Mat(sharedImageBuffer->getByDeviceNumber(deviceNumber)->get().clone(), currentROI);
		if (emitOriginal || captureOriginal)
			originalFrame = currentFrame.clone();

		/////////////////////////////////// //
		//  PERFORM IMAGE PROCESSING BELOW  //
		/////////////////////////////////// //

		cvProcessFrame(&currentFrame, imgProcFlags, imgProcSettings);

		////////////////////////// ///////// //
		// PERFORM IMAGE PROCESSING ABOVE //
		////////////////////////// ///////// //

		// Convert Mat to QImage
		frame = MatToQImage(currentFrame);

		processingMutex.unlock();

		// Save the Stream
		if (doRecord) {
			if (output.isOpened()) {
				if (captureOriginal) {
					processingMutex.lock();
					// Combine original and processed frame
					combinedFrame = combineFrames(currentFrame, originalFrame);
					processingMutex.unlock();

					output.write(combinedFrame);
				}else {
					output.write(currentFrame);
				}

				framesWritten++;
				emit frameWritten(framesWritten);
			}
		}

		// Emit the original image before frame processing
		if (emitOriginal)
			emit origFrame(MatToQImage(originalFrame));
		else
			// Inform GUI thread of new frame (QImage)
			emit newFrame(frame);
		//emit newFrame(MatToQImage(currentFrame));

		// Update statistics
		updateFPS(processingTime);
		statsData.nFramesProcessed++;
		// Inform GUI of updated statistics
		emit updateStatisticsInGUI(statsData);
	}
	qDebug() << "Stopping processing thread...";
}

void ProcessingThread::fillProcessingBuffer()
{
	processingBuffer.push_back(currentFrame);
}

bool ProcessingThread::processingBufferFilled()
{
	return(processingBuffer.size() == processingBufferLength && processingBuffer.size() > 0);
}

void ProcessingThread::getOriginalFrame(bool doEmit)
{
	emitOriginal = doEmit;
}

int ProcessingThread::getRecordFPS()
{
	return recordingFramerate;
}

int ProcessingThread::writenFrames()
{
	return framesWritten;
}

void ProcessingThread::updateFPS(int timeElapsed)
{
	// Add instantaneous FPS value to queue
	if (timeElapsed > 0) {
		fps.enqueue((int)1000 / timeElapsed);
		// Increment sample number
		sampleNumber++;
	}

	// Maximum size of queue is DEFAULT_PROCESSING_FPS_STAT_QUEUE_LENGTH
	if (fps.size() > PROCESSING_FPS_STAT_QUEUE_LENGTH)
		fps.dequeue();

	// Update FPS value every DEFAULT_PROCESSING_FPS_STAT_QUEUE_LENGTH samples
	if ((fps.size() == PROCESSING_FPS_STAT_QUEUE_LENGTH) && (sampleNumber == PROCESSING_FPS_STAT_QUEUE_LENGTH)) {
		// Empty queue and store sum
		while (!fps.empty())
			fpsSum += fps.dequeue();
		// Calculate average FPS
		statsData.averageFPS = fpsSum / PROCESSING_FPS_STAT_QUEUE_LENGTH;
		// Reset sum
		fpsSum = 0;
		// Reset sample number
		sampleNumber = 0;

		// save new fps in settings and inform magnification thread about it
		// (this is important for fps based color magnification)
		imgProcSettings.framerate = statsData.averageFPS;
	}
}

void ProcessingThread::stop()
{
	QMutexLocker locker(&doStopMutex);
	releaseCapture();
	doStop = true;
}

void ProcessingThread::updateImageProcessingFlags(struct ImageProcessingFlags flags)
{
	QMutexLocker locker(&processingMutex);

	this->imgProcFlags.grayscaleOn = flags.grayscaleOn;
	this->imgProcFlags.flipOn = flags.flipOn;
	this->imgProcFlags.blurOn = flags.blurOn;
	this->imgProcFlags.morphOn = flags.morphOn;
	//qDebug() << "Morph:" << flags.morphOn << imgProcSettings.morphOption;
	this->imgProcFlags.dilateOn = flags.dilateOn;
	this->imgProcFlags.erodeOn = flags.erodeOn;
	this->imgProcFlags.cannyOn = flags.cannyOn;
	this->imgProcFlags.hsvHistogramOn = flags.hsvHistogramOn;
	this->imgProcFlags.hsvEqualizeOn = flags.hsvEqualizeOn;
	this->imgProcFlags.pcaOn = flags.pcaOn;
	this->imgProcFlags.colorcheckerOn = flags.colorcheckerOn;
	this->imgProcFlags.grabcutOn = flags.grabcutOn;
	this->imgProcFlags.meanshiftOn = flags.meanshiftOn;

	processingBuffer.clear();
	//magnificator.clearBuffer();
}

void ProcessingThread::updateProcessingSettings(struct ImageProcessingSettings settings)
{
	QMutexLocker locker(&processingMutex);
	this->imgProcSettings.blurType = settings.blurType;
	this->imgProcSettings.morphOption = settings.morphOption;

	qDebug() << "Settings morph:" << settings.morphOption;

	this->imgProcSettings.dilateNumberOfIterations = settings.dilateNumberOfIterations;
	this->imgProcSettings.erodeNumberOfIterations = settings.erodeNumberOfIterations;

	this->imgProcSettings.cannyThreshold1 = settings.cannyThreshold1;
	this->imgProcSettings.cannyThreshold2 = settings.cannyThreshold2;
	this->imgProcSettings.cannyApertureSize = settings.cannyApertureSize;
	this->imgProcSettings.cannyL2gradient = settings.cannyL2gradient;

	this->imgProcSettings.flipcode = settings.flipcode;
	//qDebug() << "flipcode" << imgProcSettings.flipcode;

	//this->imgProcSettings.amplification = imgProcessingSettings.amplification;
	//this->imgProcSettings.coWavelength = imgProcessingSettings.coWavelength;
	//this->imgProcSettings.coLow = imgProcessingSettings.coLow;
	//this->imgProcSettings.coHigh = imgProcessingSettings.coHigh;
	//this->imgProcSettings.chromAttenuation = imgProcessingSettings.chromAttenuation;
	if (this->imgProcSettings.levels != settings.levels) {
		processingBuffer.clear();
		//magnificator.clearBuffer();
	}
	this->imgProcSettings.levels = settings.levels;
}

void ProcessingThread::setROI(QRect roi)
{
	QMutexLocker locker(&processingMutex);
	currentROI.x = roi.x();
	currentROI.y = roi.y();
	currentROI.width = roi.width();
	currentROI.height = roi.height();
	processingBuffer.clear();
	//agnificator.clearBuffer();
	//int levels = 1;//magnificator.calculateMaxLevels(roi);
	locker.unlock();
	//emit maxLevels(levels);
}

QRect ProcessingThread::getCurrentROI()
{
	return QRect(currentROI.x, currentROI.y, currentROI.width, currentROI.height);
}

// Prepare videowriter to capture camera
bool ProcessingThread::startRecord(std::string filepath, bool captureOriginal)
{
	// release Video if any was made until now
	releaseCapture();

	// Initials for the VideoWriter
	// Size
	int w = (int)currentROI.width;
	int h = (int)currentROI.height;
	// Codec WATCH OUT: Not every codec is available on every PC,
	// MP4V was chosen because it's famous among various systems
	//int codec = VideoWriter::fourcc('M', 'P', '4', 'V');
	// Check if grayscale is on (or camera only captures grayscale)
	bool isColor = !((imgProcFlags.grayscaleOn) || (currentFrame.channels() == 1));
	// Capture size is doubled if original should be captured too
	Size s = captureOriginal ? Size(w * 2, h) : Size(w, h);

	bool opened = false;
	output = VideoWriter();
	opened = output.open(filepath, savingCodec, statsData.averageFPS, s, isColor);


	recordingFramerate = statsData.averageFPS;

	if (opened) {
		this->doRecord = true;
		this->captureOriginal = captureOriginal;
	}

	return opened;
}

void ProcessingThread::stopRecord()
{
	this->doRecord = false;
	framesWritten = 0;
}

bool ProcessingThread::isRecording()
{
	return this->doRecord;
}

// Combine Frames into one Frame, depending on their size
Mat ProcessingThread::combineFrames(Mat &frame1, Mat &frame2)
{
	Mat roi;
	int w = (int)currentROI.width;
	int h = (int)currentROI.height;

	Mat mergedFrame = Mat(Size(w * 2, h), frame1.type());
	roi = Mat(mergedFrame, Rect(0, 0, w, h));
	frame1.copyTo(roi);
	roi = Mat(mergedFrame, Rect(w, 0, w, h));
	frame2.copyTo(roi);

	return mergedFrame;
}

void ProcessingThread::updateFramerate(double fps)
{
	imgProcSettings.framerate = fps;
}
