/************************************************************************************/
/* An OpenCV/Qt based realtime video application		                    */
/*                                                                                  */
/*                                                                                  */
/* Based on the work of                                                             */
/*      Joseph Pan      <https://github.com/wzpan/QtEVM>                            */
/*      Nick D'Ademo    <https://github.com/nickdademo/qt-opencv-multithreaded>     */
/*      Jens Schindel   <kontakt@jens-schindel.de> RVM                              */
/*                       https://github.com/nasafix-nasser/Qt-RangeSlider/          */
/*                                                                                  */
/* helper/ProcessingFrame.cpp							    */
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
// Qt
#include <QCoreApplication>
#include <QSettings>
//
#include "main/helper/_ProcessingFrame.h"
#include "main/helper/MeanShift.h"

/*
   void saveSettings()
   {
    QSettings mySettings(QCoreApplication::applicationDirPath() + "/Settings.ini", QSettings::IniFormat);
    mySettings.setIniCodec("UTF-8");

    mySettings.setValue("Camera/Flipcode",ui->comboBox_bauds->currentText());

    qDebug() << mySettings.fileName();
    qDebug() << mySettings.value("Camera/Flipcode");

    }

   void readSettings()
   {
   QSettings mySettings(QCoreApplication::applicationDirPath() + "/Settings.ini", QSettings::IniFormat);

   mySettings.setIniCodec("UTF-8");

   qDebug() << mySettings.fileName();
   qDebug() << mySettings.value("Camera/Flipcode");
   }
 */

void calcHistogram(cv::Mat s, cv::Mat o)
{
	//variables preparing
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	int hbins = 32;
	int channels[] = { 0 }; //Hue channel
	int histSize[] = { hbins };
	float hranges[] = { 0, 180 };
	const float* ranges[] = { hranges };

	cv::Mat patch_HSV;
	cv::MatND HistA;
	//cal histogram & normalization
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	cvtColor(s, patch_HSV, cv::COLOR_BGR2HSV);
	calcHist(&patch_HSV, 1, channels, cv::Mat(),   // do not use mask
		 //calcHist( &patch_HSV, 1, channels, mask, //use mask
		 HistA, 1, histSize, ranges,
		 true, // the histogram is uniform
		 false); // no accumulte
	normalize(HistA, HistA, 0, 255, cv::NORM_MINMAX);

	//Mat for drawing
	//cv::Mat histimg = cv::Mat::zeros(80, 320, CV_8UC3);
	cv::Mat histimg = cv::Mat::zeros(o.cols / 16, o.cols, CV_8UC3); //Fixed histogram height out/16
	//  histimg = cv::Scalar::all(255);
	//  int binW = histimg.cols / hbins;
	int binW = o.cols / hbins;
	cv::Mat buf(1, hbins, CV_8UC3);
	//Set RGB color
	for (int i = 0; i < hbins; i++)
		buf.at< cv::Vec3b>(i) = cv::Vec3b(cv::saturate_cast< uchar>(i * 180. / hbins), 255, 255);
	//cvtColor(buf, buf, cv::COLOR_HSV2RGB); //as we are out put to QT pixmap
	cvtColor(buf, buf, cv::COLOR_HSV2BGR);

	//drawing routine
	for (int i = 0; i < hbins; i++) {
		int val = cv::saturate_cast< int>(HistA.at< float>(i) * histimg.rows / 255);

		rectangle(o, cv::Point(i * binW, o.rows), cv::Point((i + 1) * binW, o.rows - val - 3),
			  cv::Scalar(buf.at< cv::Vec3b>(i)), cv::FILLED, cv::LINE_8);
		//int r,g,b;
		//b =  buf.at< cv::Vec3b>(i)[0];
		//g =  buf.at< cv::Vec3b>(i)[1];
		//r =  buf.at< cv::Vec3b>(i)[2];
		//show bin and RGB value
		//printf("[%d] r=%d, g=%d, b=%d , bins = %d \n",i , r, g, b, val);
		//QString sHistogram = QString("Histogram[%1]:%2").arg(i).arg(val);
		//ui->plainTextEdit->appendPlainText(sHistogram);
	}
	//imshow("Histogram", histimg);
}

/*  getOrientationPCA
 *
 *    calc Principal Component Analysis Orientation Angle
 *
 *    ref: https://learnopencv.com/principal-component-analysis/
 *         https://docs.opencv.org/3.4/d1/dee/tutorial_introduction_to_pca.html
 *
 */
double getOrientationPCA(vector<Point> &pts, Mat &img)
{
	if (pts.size() == 0) return false;

	//Construct a buffer used by the pca analysis
	Mat data_pts = Mat(pts.size(), 2, CV_64FC1);
	for (int i = 0; i < data_pts.rows; ++i) {
		data_pts.at<double>(i, 0) = pts[i].x;
		data_pts.at<double>(i, 1) = pts[i].y;
	}

	//Perform PCA analysis
	PCA pca_analysis(data_pts, Mat(), cv::PCA::DATA_AS_ROW);

	//Store the position of the object
	Point pos = Point(pca_analysis.mean.at<double>(0, 0),
			  pca_analysis.mean.at<double>(0, 1));

	//Store the eigenvalues and eigenvectors
	vector<Point2d> eigen_vecs(2);
	vector<double> eigen_val(2);
	for (int i = 0; i < 2; ++i) {
		eigen_vecs[i] = Point2d(pca_analysis.eigenvectors.at<double>(i, 0),
					pca_analysis.eigenvectors.at<double>(i, 1));

		eigen_val[i] = pca_analysis.eigenvalues.at<double>(0, i);
	}

	// Draw the principal components
	circle(img, pos, 3, CV_RGB(255, 0, 255), 2);
	line(img, pos, pos + 0.02 * Point(eigen_vecs[0].x * eigen_val[0], eigen_vecs[0].y * eigen_val[0]), CV_RGB(255, 255, 0), 2); //Yellow thick:2
	line(img, pos, pos + 0.02 * Point(eigen_vecs[1].x * eigen_val[1], eigen_vecs[1].y * eigen_val[1]), CV_RGB(0, 255, 255)); //Cyan think:1

	return atan2(eigen_vecs[0].y, eigen_vecs[0].x);
}

// Make simple frame and MOG2 mask
Mat frame, MOGMask;
// Init MOG2 BackgroundSubstractor
Ptr<BackgroundSubtractor> BackgroundSubstractor;
static bool initGrabCut = true;
void cvGrabCut(cv::Mat *f, cv::Mat *o)
{
	frame = *f;
	if (frame.empty()) return;
	if (initGrabCut) {
		BackgroundSubstractor = createBackgroundSubtractorMOG2(false);
		initGrabCut = false;
	}

	// Update the background model
	BackgroundSubstractor->apply(frame, MOGMask);

	// Some works with noises on frame //
	// Blur the foreground mask to reduce the effect of noise and false positives
	// Remove the shadow parts and the noise
	medianBlur(MOGMask, MOGMask, 5);
	threshold(MOGMask, MOGMask, 20, 255, THRESH_BINARY_INV);

	// Zeros images
	cv::Mat foregroundImg((*f).size(), CV_8UC3, cv::Scalar(255, 255, 255));    //W

	// Using mask to cut foreground
	foregroundImg.copyTo(*o, MOGMask);
}
//
//https://github.com/zerenlu/cartoon
//
void cartoonifyImage(const Mat &srcColor, Mat &dst)
{
	Mat gray;
	if (srcColor.channels() >= 3) cvtColor(srcColor, gray, COLOR_BGR2GRAY);
	else gray = srcColor;

	const int MEDIAN_BLUR_FILTER_SIZE = 11;
	medianBlur(gray, gray, MEDIAN_BLUR_FILTER_SIZE);
	Mat edges;
	const int LAPLACIAN_FILTER_SIZE = 5;
	Laplacian(gray, edges, CV_8U, LAPLACIAN_FILTER_SIZE);

	Mat mask;
	//const int EDGES_THRESHOLD = 80;
	threshold(edges, mask, 80, 255, THRESH_BINARY_INV);

	Size size = srcColor.size();
	Size smallSize;
	smallSize.width = size.width >> 1;
	smallSize.height = size.height >> 1;
	Mat smallImg;
	//Mat smallImg = Mat(smallSize, CV_8UC3);
	//resize(srcColor, smallImg, smallSize, 0, 0, INTER_LINEAR);
	resize(srcColor, smallImg, smallSize, .5, .5);

	Mat tmp = Mat(smallSize, CV_8UC3);
	int repetitions = 7;//repetition for strong cartoon effect
	for (int i = 0; i < repetitions; i++) {
		int ksize = 7;   //filter size
		double sigmaColor = 12;//filter color strength
		double sigmaSpace = 8;//spatial strength
		bilateralFilter(smallImg, tmp, ksize, sigmaColor, sigmaSpace);
		bilateralFilter(tmp, smallImg, ksize, sigmaColor, sigmaSpace);
	}

	Mat bigImg;
	//resize(smallImg, bigImg, size, 0, 0, INTER_LINEAR);
	resize(smallImg, bigImg, size, 2, 2);
	dst.setTo(0);
	bigImg.copyTo(dst, mask);
}
/*  cvProcessFrame
 *
 *    main frame processing at here:
 *        Gray, Flip, Blur, Dilate, Erode, Canny, HSV segment/Histogram, MacBeth colorcheck, PCA...
 *
 *    called by threads/PlayerThread.cpp(ui/VideoView.cpp), threads/ProcessingThread.cpp(ui/CameraView.cpp)
 *
 */
void cvProcessFrame(cv::Mat *f, ImageProcessingFlags flags, ImageProcessingSettings settings)
{
	Mat gray, frameHSV, frameMask;
	vector<Mat> planes;

	// Convert to grayscale
	if (flags.grayscaleOn && ((*f).channels() >= 3)) {
		cvtColor(*f, *f, cv::COLOR_BGR2GRAY, 1);
	}
	// Flip
	if (flags.flipOn) {
		//flip(*frame, *frame, 1); //0:x-axis 1:y-axis -1:both-axis
		flip(*f, *f, settings.flipcode);
	}
	// Blur/Smooth
	if (flags.blurOn) {
		//qDebug() << "Blur:" << settings.blurType;
		switch (settings.blurType) {
		default:
		case 0: // Median
			medianBlur(*f, *f, 3);
			break;
		case 1: //Gaussian
			GaussianBlur(*f, *f, cv::Size(5, 5), 2, 2);
			break;
		case 2:  //Morph
			// Creating kernel matrix
			int morph_elem = 2;
			int morph_size = 2;
			Mat element = getStructuringElement(morph_elem, Size(2 * morph_size + 1, 2 * morph_size + 1), Point(morph_size, morph_size));
			// Applying Blur effect on the Image
			morphologyEx(*f, *f, settings.morphOption, element);
			break;
		}
	}
	// Dilate
	if (flags.dilateOn) {
		dilate(*f, *f, Mat(), Point(-1, -1), settings.dilateNumberOfIterations);
	}
	// Erode
	if (flags.erodeOn) {
		erode(*f, *f, Mat(), Point(-1, -1), settings.erodeNumberOfIterations);
	}
	// Do HSV Histogram

	if (flags.hsvHistogramOn && ((*f).channels() >= 3)) {
		// Convert from BGR to HSV colorspace
		cvtColor(*f, frameHSV, COLOR_BGR2HSV);
		// Detect the object based on HSV Range Values
		inRange(frameHSV, Scalar(settings.hsvHueLow, settings.hsvSatLow, settings.hsvValLow),
			Scalar(settings.hsvHueHigh, settings.hsvSatHigh, settings.hsvValHigh), frameMask);
		GaussianBlur(frameMask, frameMask, Size(5, 5), 2, 2);
		// Convert back BGR for HSV and Mask
		cvtColor(frameMask, frameMask, COLOR_GRAY2BGR);
		cvtColor(frameHSV, frameHSV, COLOR_HSV2BGR);
		*f = frameHSV & frameMask; //bitwise AND

		if (flags.hsvEqualizeOn) {
			//Mat HSVMat, mergeMat;
			//vector<Mat> planes;
			split(frameHSV, planes); //H,S,V
			equalizeHist(planes[2], planes[2]); //equalize V plane
			merge(planes, *f);
		}

		//Do histogram calc...
		calcHistogram(*f, *f);
	}
	// Canny edge detection
	if (flags.cannyOn) {
		gray = (*f).clone();
		if (gray.channels() >= 3) cvtColor(gray, gray, COLOR_BGR2GRAY);

		GaussianBlur(gray, gray, Size(3, 3), 0);
		Canny(gray, gray, settings.cannyThreshold1, settings.cannyThreshold2, settings.cannyApertureSize, settings.cannyL2gradient);
		if ((*f).channels() >= 3) {
			vector<Mat> channels;
			Mat z = Mat::zeros(gray.rows, gray.cols, CV_8UC1);
			split(*f, channels);//split into channels
			channels[0] = gray; //B
			channels[2] = gray; //R
			merge(channels, *f);//mergeback
		}else *f = gray;
	}
//
// sp The spatial window radius.
// sr The color window radius.
// maxLevel Maximum level of the pyramid for the segmentation.
//
//	pyrMeanShiftFiltering(*f, *f, 20, 45, 3);
//
/*
   cv::Rect rectangle(5, 5, (*f).cols - 5, (*f).rows - 5);

   cv::Mat result;         // segmentation result (4 possible values)
   cv::Mat bgModel, fgModel;         // the models (internally used)

   // GrabCut segmentation
   cv::grabCut(*f,           // input image
          result, // segmentation result
          rectangle, // rectangle containing foreground
          bgModel, fgModel, // models
          1, // number of iterations
          cv::GC_INIT_WITH_RECT); // use rectangle
   // Get the pixels marked as likely foreground
   cv::compare(result, cv::GC_PR_FGD, result, cv::CMP_EQ);
   // Generate output image
   cv::Mat foreground((*f).size(), CV_8UC3, cv::Scalar(255, 255, 255));    //Black
   //*f = foreground & result;
   foreground.copyTo(*f, result);
 */
        /*
           //Floodfill background
           Point center = Point((*f).size().width / 2, (*f).size().height / 2);
           Rect rect;
           floodFill(*f,           //input image
            center,    //Pick up point
            Scalar(0, 0, 0), //fill color
            &rect,         //Redraw the smallest rectangle, the default value is 0;
            Scalar(30, 30, 30), //Negative difference, the difference between the picked point pixel rgb and the current pixel rgb, the difference is greater than this
            Scalar(12, 12, 12));       //Positive difference, the difference between the current pixel rgb and the picked point pixel rgb, the difference is less than this
         */
/*
   //Founding largest contour------------------------------------------------
   cvtColor(*f, gray, COLOR_BGR2GRAY);
   threshold(gray, gray, 100, 255, THRESH_BINARY_INV); //Threshold the gray

   int largest_contour_index = 0;
   double largest_area;
   Rect bounding_rect;
   vector<vector<Point> > contours; // Vector for storing contour
   vector<Vec4i> hierarchy;
   findContours(gray, contours, hierarchy, RETR_CCOMP, CHAIN_APPROX_SIMPLE);
   // iterate through each contour.
   for (int i = 0; i < contours.size(); i++) {
      //  Find the area of contour
      double a = contourArea(contours[i], false);
      if (a > largest_area) {
         largest_area = a; cout << i << " area  " << a << endl;
         // Store the index of largest contour
         largest_contour_index = i;
         // Find the bounding rectangle for biggest contour
         bounding_rect = boundingRect(contours[i]);
      }
   }
   Scalar color(255, 0, 0);     // color of the contour in the
   //Draw the contour and rectangle
   drawContours(*f, contours, largest_contour_index, color, 2, LINE_8, hierarchy);
   //-----------------------------------------------------------------------------
 */
        // Do cartoon
        if (flags.cartoonOn) {
                cartoonifyImage(*f, *f);
        }

	// Do Spatial/Color meanshift
	if (((*f).channels() >= 3) && (flags.meanshiftOn)) {
		Mat Img = *f;
		cv::resize(Img, Img, cv::Size(), 0.5, 0.5);                                                                                                                                                                                                                                                           //reduce to 1/4 for faster processing
		// Convert color from BGR to Lab
		cvtColor(Img, Img, COLOR_BGR2Lab);
		// Initilize Mean Shift with spatial bandwith and color bandwith
		//MeanShift MSProc(8, 16);
		MeanShift MSProc(10, 16);
		// Filtering Process
		MSProc.MSFiltering(Img);
		// Segmentation Process include Filtering Process (Region Growing)
		//MSProc.MSSegmentation(Img);

		// Convert color from Lab to BGR
		//cv::resize(Img, Img, cv::Size(), 2, 2); //do we need to gain back the original size?
		cvtColor(Img, *f, COLOR_Lab2BGR);
	}

	// Do simple foreground grabcut
	if (flags.grabcutOn) {
		cvGrabCut(f, f);
	}


	// Do PCA
	if (flags.pcaOn) {
		if ((*f).channels() >= 3) cvtColor(*f, gray, COLOR_BGR2GRAY);
		else gray = *f;
		threshold(gray, gray, 160, 255, THRESH_BINARY);
		// Find all objects of interest
		vector<vector<Point> > contours;
		vector<Vec4i> hierarchy;
		findContours(gray, contours, hierarchy, RETR_LIST, CHAIN_APPROX_NONE);

		if ((*f).channels() == 1) cvtColor(*f, *f, COLOR_GRAY2BGR);
		// For each object
		for (size_t i = 0; i < contours.size(); ++i) {
			// Calculate its area
			double area = contourArea(contours[i]);
			// Ignore if too small or too large
			if (area < (32 * 10) || (320 * 100) < area) continue;
			// Draw the contour
			drawContours(*f, contours, i, CV_RGB(255, 0, 0), 1, LINE_8, hierarchy, 0);

			// Get the object orientation
			getOrientationPCA(contours[i], *f);
		}
	}


	// MacBeth ColorChecker
	if (flags.colorcheckerOn && ((*f).channels() >= 3)) {
		//Do Macbech colorchecker
		cv::Ptr<cv::mcc::CCheckerDetector> detector = cv::mcc::CCheckerDetector::create();
		// Marker type to detect
		if (!detector->process(*f, cv::mcc::TYPECHART(0), 1)) {
			qDebug("ChartColor not detected \n");
		}else {
			// get checker
			std::vector<cv::Ptr<cv::mcc::CChecker> > checkers = detector->getListColorChecker();
			for (cv::Ptr<cv::mcc::CChecker> checker : checkers) {
				// current checker
				cv::Ptr<cv::mcc::CCheckerDraw> cdraw = cv::mcc::CCheckerDraw::create(checker);
				cdraw->draw(*f);
			}
		}
	}
}
