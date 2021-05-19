/************************************************************************************/
/* An OpenCV/Qt based realtime application			                    */
/*                                                                                  */
/*                                                                                  */
/* Based on the work of                                                             */
/*      Joseph Pan      <https://github.com/wzpan/QtEVM>                            */
/*      Nick D'Ademo    <https://github.com/nickdademo/qt-opencv-multithreaded>     */
/*                                                                                  */
/* threads/PlayerThread.cpp							    */
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

#include "main/threads/PlayerThread.h"

// Constructor
PlayerThread::PlayerThread(const std::string filepath, int width, int height, double fps)
	: QThread(),
	filepath(filepath),
	width(width),
	height(height),
	fps(fps),
	emitOriginal(false)
{
	doStop = true;
	doPause = false;
	doPlay = false;
	statsData.averageFPS = 0;
	statsData.averageVidProcessingFPS = 0;
	statsData.nFramesProcessed = 0;

	originalBuffer = std::vector<Mat>();
	processingBuffer = std::vector<Mat>();

	imgPlayerSettings.framerate = fps;

	sampleNumber = 0;
	fpsSum = 0;
	fpsQueue.clear();

	//this->magnificator = Magnificator(&processingBuffer, &imgProcFlags, &imgProcSettings);
	this->cap = VideoCapture();
	currentWriteIndex = 0;
}

// Destructor
PlayerThread::~PlayerThread()
{
	doStopMutex.lock();
	doStop = true;
	if (releaseFile())
		qDebug() << "Released File.";
	doStopMutex.unlock();
	wait();
}

// Thread
void PlayerThread::run()
{
	qDebug() << "Starting player thread...";
	// The standard delay time to keep FPS playing rate without processing time
	double delay = 1000.0 / fps;
	QTime mTime;
	/////////////////////////////////////
	/// Stop thread if doStop=TRUE /////
	///////////////////////////////////
	while (!doStop) {
		//////////////////////////////////////////////
		/// Stop thread if processed all frames /////
		////////////////////////////////////////////
		if (currentWriteIndex >= lengthInFrames) {
			endOfFrame_action();
			break;
		}

		// Save process time
		processingTime = t.elapsed();
		// Start timer
		t.start();

		/////////////////////////////////////
		/// Pause thread if doPause=TRUE ///
		///////////////////////////////////
		doStopMutex.lock();
		if (doPause) {
			doPlay = false;
			doStopMutex.unlock();
			break;
		}
		doStopMutex.unlock();



		// Start timer and capture time needed to process 1 frame here
		if (getCurrentReadIndex() == processingBufferLength - 1)
			mTime.start();
		// Switch to process images on the fly instead of processing a whole buffer, reducing MEM
		//if(imgProcFlags.colorMagnifyOn && processingBufferLength > 2 && magnificator.getBufferSize() > 2) {
		//if (imgProcFlags.colorMagnifyOn && processingBufferLength > 2) {
		//	processingBufferLength = 2;
		//}



		///////////////////////////////////
		/////////// Capturing ////////////
		/////////////////////////////////
		// Fill buffer, check if it's the start of magnification or not
		for (int i = processingBuffer.size(); i < processingBufferLength && getCurrentFramenumber() < lengthInFrames; i++) {
			processingMutex.lock();

			// Try to grab the next Frame
			if (cap.read(grabbedFrame)) {
				// Preprocessing
				// Set ROI of frame
				currentFrame = Mat(grabbedFrame.clone(), currentROI);

				/////////////////////////////////// //
				//  PERFORM IMAGE PROCESSING BELOW  //
				/////////////////////////////////// //

				cvProcessFrame(&currentFrame, imgProcFlags, imgPlayerSettings);

				/////////////////////////////////// //
				//  PERFORM IMAGE PROCESSING ABOVE  //
				/////////////////////////////////// //

				// Fill fuffer
				processingBuffer.push_back(currentFrame);
				if (emitOriginal)
					originalBuffer.push_back(currentFrame.clone());
			}
			// Wasn't able to grab frame, abort thread
			else {
				processingMutex.unlock();
				endOfFrame_action();
				break;
			}

			processingMutex.unlock();
		}
		// Breakpoint if grabbing frames wasn't succesful
		if (doStop) {
			break;
		}

		///////////////////////////////////
		/////////// Magnifying ///////////
		/////////////////////////////////
		processingMutex.lock();
/*
        if (imgProcFlags.colorMagnifyOn) {
            magnificator.colorMagnify();
            if (magnificator.hasFrame()) {
                currentFrame = magnificator.getFrameFirst();
            }
        }else if (imgProcFlags.laplaceMagnifyOn) {
            magnificator.laplaceMagnify();
            if (magnificator.hasFrame()) {
                currentFrame = magnificator.getFrameFirst();
            }
        }else if (imgProcFlags.rieszMagnifyOn) {
            magnificator.rieszMagnify();
            if (magnificator.hasFrame()) {
                currentFrame = magnificator.getFrameFirst();
            }
        }else*/{
                        // Read frames unmagnified
                        currentFrame = processingBuffer.at(getCurrentReadIndex());
                        // Erase to keep buffer size
                        processingBuffer.erase(processingBuffer.begin());
                }
                // Increase number of frames given to GUI
                currentWriteIndex++;

		frame = MatToQImage(currentFrame);
		if (emitOriginal) {
			originalFrame = MatToQImage(originalBuffer.front());
			if (!originalBuffer.empty())
				originalBuffer.erase(originalBuffer.begin());
		}

		processingMutex.unlock();

		///////////////////////////////////
		/////////// Updating /////////////
		/////////////////////////////////
		// Inform GUI thread of new frame
		emit newFrame(frame);
		// Inform GUI thread of original frame if option was set
		if (emitOriginal)
			emit origFrame(originalFrame);

		// Update statistics
		updateFPS(processingTime);
		statsData.nFramesProcessed = currentWriteIndex;
		// Inform GUI about updatet statistics
		emit updateStatisticsInGUI(statsData);

		// To keep FPS playing rate, adjust waiting time, dependent on the time that is used to process one frame
		double diff = mTime.elapsed();
		int wait = max(delay - diff, 0.0);
		this->msleep(wait);
	}
	qDebug() << "Stopping player thread...";
}

int PlayerThread::getCurrentReadIndex()
{
	return std::min(currentWriteIndex, processingBufferLength - 1);
}

double PlayerThread::getFPS()
{
	return fps;
}

void PlayerThread::getOriginalFrame(bool doEmit)
{
	QMutexLocker locker1(&doStopMutex);
	QMutexLocker locker2(&processingMutex);

	originalBuffer.clear();
	originalBuffer = processingBuffer;
	emitOriginal = doEmit;
}

// Load videofile
bool PlayerThread::loadFile()
{
	// Just in case, release file
	releaseFile();
	QString sFile = QString::fromStdString(filepath);
	cap = VideoCapture(filepath);

	// Open file
	bool openResult = isFileLoaded();
	if (!openResult)
		openResult = cap.open(filepath);

	// Set resolution
	if (width != -1)
		cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
	if (height != -1)
		cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
	if (fps == -1) {
		fps = cap.get(cv::CAP_PROP_FPS);
	}

	// OpenCV can't read all mp4s properly, fps is often false
	if (fps < 0 || !std::isfinite(fps)) {
		fps = 30;
	}

	// initialize Buffer length
	setBufferSize();

	// Write information in Settings
	statsData.averageFPS = fps;
	imgPlayerSettings.framerate = fps;
	imgPlayerSettings.frameHeight = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
	imgPlayerSettings.frameWidth = cap.get(cv::CAP_PROP_FRAME_WIDTH);

	// Save total length of video
	lengthInFrames = cap.get(cv::CAP_PROP_FRAME_COUNT);
	if (sFile.contains(".png")) lengthInFrames = 1; //Fix for MacOS OpenCV4.5.1 PNG frames issue

	return openResult;
}

// Release the file from VideoCapture
bool PlayerThread::releaseFile()
{
	// File is loaded
	if (cap.isOpened()) {
		// Release File
		cap.release();
		return true;
	}
	// File is NOT laoded
	else
		return false;
}

// Stop the thread
void PlayerThread::stop()
{
	doStop = true;
	doPause = false;
	doPlay = false;

	setBufferSize();
	releaseFile();

	currentWriteIndex = 0;
}

void PlayerThread::endOfFrame_action()
{
	doStop = true;
	stop();
	emit endOfFrame();
	statsData.nFramesProcessed = 0;
	emit updateStatisticsInGUI(statsData);
}

bool PlayerThread::isFileLoaded()
{
	return cap.isOpened();
}

int PlayerThread::getInputSourceWidth()
{
	return imgPlayerSettings.frameWidth;
}

int PlayerThread::getInputSourceHeight()
{
	return imgPlayerSettings.frameHeight;
}

void PlayerThread::setROI(QRect roi)
{
	QMutexLocker locker1(&doStopMutex);
	QMutexLocker locker2(&processingMutex);
	currentROI.x = roi.x();
	currentROI.y = roi.y();
	currentROI.width = roi.width();
	currentROI.height = roi.height();
	//int levels = magnificator.calculateMaxLevels(roi);
	//magnificator.clearBuffer();
	locker1.unlock();
	locker2.unlock();
	setBufferSize();
	//emit maxLevels(levels);
}

QRect PlayerThread::getCurrentROI()
{
	return QRect(currentROI.x, currentROI.y, currentROI.width, currentROI.height);
}

// Private Slots
void PlayerThread::updateImageProcessingFlags(struct ImageProcessingFlags flags)
{
	QMutexLocker locker1(&doStopMutex);
	QMutexLocker locker2(&processingMutex);

	this->imgProcFlags.grayscaleOn = flags.grayscaleOn;
	this->imgProcFlags.flipOn = flags.flipOn;
	this->imgProcFlags.blurOn = flags.blurOn;
	this->imgProcFlags.morphOn = flags.morphOn;
	//qDebug() << "PlayerProc:" << flags.blurOn;
	this->imgProcFlags.dilateOn = flags.dilateOn;
	this->imgProcFlags.erodeOn = flags.erodeOn;
	this->imgProcFlags.cannyOn = flags.cannyOn;
	this->imgProcFlags.hsvHistogramOn = flags.hsvHistogramOn;
	this->imgProcFlags.hsvEqualizeOn = flags.hsvEqualizeOn;
	this->imgProcFlags.pcaOn = flags.pcaOn;
	this->imgProcFlags.colorcheckerOn = flags.colorcheckerOn;
	this->imgProcFlags.grabcutOn = flags.grabcutOn;
	this->imgProcFlags.meanshiftOn = flags.meanshiftOn;
	this->imgProcFlags.cartoonOn = flags.cartoonOn;

	//qDebug() << this->imgProcFlags.hsvHistogramOn;

	locker1.unlock();
	locker2.unlock();

	setBufferSize();
}

void PlayerThread::updateProcessingSettings(struct ImageProcessingSettings settings)
{
	QMutexLocker locker1(&doStopMutex);
	QMutexLocker locker2(&processingMutex);
	bool resetBuffer = (imgPlayerSettings.levels != settings.levels);

	imgPlayerSettings.blurType = settings.blurType;
	imgPlayerSettings.morphOption = settings.morphOption;
	//qDebug() << "Player:" << settings.blurType;

	imgPlayerSettings.hsvHueLow = settings.hsvHueLow;
	imgPlayerSettings.hsvSatLow = settings.hsvSatLow;
	imgPlayerSettings.hsvValLow = settings.hsvValLow;
	imgPlayerSettings.hsvHueHigh = settings.hsvHueHigh;
	imgPlayerSettings.hsvSatHigh = settings.hsvSatHigh;
	imgPlayerSettings.hsvValHigh = settings.hsvValHigh;
	//qDebug() << "H" << settings.hsvHueLow << settings.hsvHueHigh;
	//qDebug() << "S" << settings.hsvSatLow << settings.hsvSatHigh;
	//qDebug() << "V" << settings.hsvValLow << settings.hsvValHigh;

	imgPlayerSettings.dilateNumberOfIterations = settings.dilateNumberOfIterations;
	imgPlayerSettings.erodeNumberOfIterations = settings.erodeNumberOfIterations;

	imgPlayerSettings.cannyThreshold1 = settings.cannyThreshold1;
	imgPlayerSettings.cannyThreshold2 = settings.cannyThreshold2;
	imgPlayerSettings.cannyApertureSize = settings.cannyApertureSize;
	imgPlayerSettings.cannyL2gradient = settings.cannyL2gradient;
	//qDebug() << "player updateSettings:" << settings.cannyApertureSize << settings.cannyL2gradient;

	//imgPlayerSettings.amplification = imgProcessingSettings.amplification;
	//imgPlayerSettings.coWavelength = imgProcessingSettings.coWavelength;
	//imgPlayerSettings.coLow = imgProcessingSettings.coLow;
	//imgPlayerSettings.coHigh = imgProcessingSettings.coHigh;
	//imgPlayerSettings.chromAttenuation = imgProcessingSettings.chromAttenuation;
	imgPlayerSettings.levels = settings.levels;

	if (resetBuffer) {
		locker1.unlock();
		locker2.unlock();
		setBufferSize();
	}
}

// Public Slots / Video control
void PlayerThread::playAction()
{
	if (!isPlaying()) {
		if (!cap.isOpened())
			loadFile();

		if (isPausing()) {
			doStop = false;
			doPause = false;
			doPlay = true;
			cap.set(cv::CAP_PROP_POS_FRAMES, getCurrentFramenumber());
			start();
		}else if (isStopping()) {
			doStop = false;
			doPause = false;
			doPlay = true;
			start();
		}
	}
}
void PlayerThread::stopAction()
{
	stop();
}
void PlayerThread::pauseAction()
{
	doStop = false;
	doPause = true;
	doPlay = false;
}

void PlayerThread::pauseThread()
{
	pauseAction();
}

bool PlayerThread::isPlaying()
{
	return this->doPlay;
}

bool PlayerThread::isStopping()
{
	return this->doStop;
}

bool PlayerThread::isPausing()
{
	return this->doPause;
}

void PlayerThread::setCurrentFrame(int framenumber)
{
	currentWriteIndex = framenumber;
	setBufferSize();
}

void PlayerThread::setCurrentTime(int ms)
{
	if (cap.isOpened())
		cap.set(cv::CAP_PROP_POS_MSEC, ms);
}

double PlayerThread::getInputFrameLength()
{
	return lengthInFrames;
}

double PlayerThread::getInputTimeLength()
{
	return lengthInMs;
}

double PlayerThread::getCurrentFramenumber()
{
	return cap.get(cv::CAP_PROP_POS_FRAMES);
}

double PlayerThread::getCurrentPosition()
{
	return cap.get(cv::CAP_PROP_POS_MSEC);
}

void PlayerThread::updateFPS(int timeElapsed)
{
	// Add instantaneous FPS value to queue
	if (timeElapsed > 0) {
		fpsQueue.enqueue((int)1000 / timeElapsed);
		// Increment sample number
		sampleNumber++;
	}

	// Maximum size of queue is DEFAULT_PROCESSING_FPS_STAT_QUEUE_LENGTH
	if (fpsQueue.size() > PROCESSING_FPS_STAT_QUEUE_LENGTH)
		fpsQueue.dequeue();

	// Update FPS value every DEFAULT_PROCESSING_FPS_STAT_QUEUE_LENGTH samples
	if ((fpsQueue.size() == PROCESSING_FPS_STAT_QUEUE_LENGTH) && (sampleNumber == PROCESSING_FPS_STAT_QUEUE_LENGTH)) {
		// Empty queue and store sum
		while (!fpsQueue.empty())
			fpsSum += fpsQueue.dequeue();
		// Calculate average FPS
		statsData.averageVidProcessingFPS = fpsSum / PROCESSING_FPS_STAT_QUEUE_LENGTH;
		// Reset sum
		fpsSum = 0;
		// Reset sample number
		sampleNumber = 0;
	}
}

// Magnificator
void PlayerThread::fillProcessingBuffer()
{
	processingBuffer.push_back(currentFrame);
}

bool PlayerThread::processingBufferFilled()
{
	return(processingBuffer.size() == processingBufferLength);
}

void PlayerThread::setBufferSize()
{
	QMutexLocker locker1(&doStopMutex);
	QMutexLocker locker2(&processingMutex);

	processingBuffer.clear();
	originalBuffer.clear();
	//magnificator.clearBuffer();
/*
    if (imgProcFlags.colorMagnifyOn) {
        processingBufferLength = magnificator.getOptimalBufferSize(imgProcSettings.framerate);
    }else if (imgProcFlags.laplaceMagnifyOn) {
        processingBufferLength = 2;
    }else if (imgProcFlags.rieszMagnifyOn) {
        processingBufferLength = 2;
    }else */{
                processingBufferLength = 1;
        }

	if (cap.isOpened() || !doStop)
		cap.set(cv::CAP_PROP_POS_FRAMES, std::max(currentWriteIndex - processingBufferLength, 0));
}
