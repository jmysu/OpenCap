/************************************************************************************/
/* An OpenCV/Qt based realtime application			                    */
/*                                                                                  */
/*                                                                                  */
/* Based on the work of                                                             */
/*      Joseph Pan      <https://github.com/wzpan/QtEVM>                            */
/*      Nick D'Ademo    <https://github.com/nickdademo/qt-opencv-multithreaded>     */
/*                                                                                  */
/* helper/ProcessingFrame.h     						    */
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
#ifndef _PROCESSINGFRAME_H
#define _PROCESSINGFRAME_H

// Qt
//#include <QtCore/QThread>
//#include <QtCore/QTime>
//#include <QtCore/QQueue>
#include "QDebug"
// OpenCV
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/mcc.hpp>
// Local
#include "main/other/Structures.h"
#include "main/other/Config.h"
#include "main/other/Buffer.h"
#include "main/helper/MatToQImage.h"
#include "main/helper/SharedImageBuffer.h"

using namespace cv;
using namespace std;

void cvProcessFrame(cv::Mat *frame, ImageProcessingFlags flags, ImageProcessingSettings settings);
void calcHistogram(cv::Mat s, cv::Mat o);
double getOrientationPCA(vector<Point> &pts, Mat &img);
void backgroundSubtrackt(cv::Mat s, cv::Mat o);

#endif // _PROCESSINGFRAME_H
