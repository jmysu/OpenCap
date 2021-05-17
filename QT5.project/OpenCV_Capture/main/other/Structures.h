/************************************************************************************/
/* An OpenCV/Qt based realtime application			                    */
/*                                                                                  */
/*                                                                                  */
/* Based on the work of                                                             */
/*      Joseph Pan      <https://github.com/wzpan/QtEVM>                            */
/*      Nick D'Ademo    <https://github.com/nickdademo/qt-opencv-multithreaded>     */
/*                                                                                  */
/* other/Structures.h      							    */
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

#ifndef STRUCTURES_H
#define STRUCTURES_H

// Qt
#include <QtCore/QRect>

struct ImageProcessingSettings {
	//double amplification;
	//double coWavelength;
	//double coLow;
	//double coHigh;
	//double chromAttenuation;
	int frameWidth;
	int frameHeight;
	double framerate;
	int levels;
	int flipcode;

	int blurType; //0:Median 1:Gaussian 2:Morph
	int morphOption; //0:Open 1:Close 2:Grident

	int hsvHueLow;
	int hsvSatLow;
	int hsvValLow;
	int hsvHueHigh;
	int hsvSatHigh;
	int hsvValHigh;

	int dilateNumberOfIterations;
	int erodeNumberOfIterations;

	double cannyThreshold1;
	double cannyThreshold2;
	int cannyApertureSize;
	bool cannyL2gradient;

	ImageProcessingSettings() :
	//amplification(0.0),
	//coWavelength(0.0),
	//coLow(0.1),
	//coHigh(0.4),
	//chromAttenuation(0.0),
	frameWidth(0),
	frameHeight(0),
	framerate(0.0),
	levels(4)
	{
	}
};

struct ImageProcessingFlags {
	bool grayscaleOn;
	bool blurOn;
	bool morphOn;
	bool dilateOn;
	bool erodeOn;
	bool flipOn;
	bool cannyOn;
	bool hsvHistogramOn;
	bool hsvEqualizeOn;
	bool pcaOn;
	bool colorcheckerOn;
	bool grabcutOn;
	bool meanshiftOn;

	ImageProcessingFlags() :
		grayscaleOn(false),
		blurOn(false),
		morphOn(false),
		dilateOn(false),
		erodeOn(false),
		flipOn(false),
		cannyOn(false),
		hsvHistogramOn(false),
		hsvEqualizeOn(false),
		pcaOn(false),
		colorcheckerOn(false),
		grabcutOn(false),
		meanshiftOn(false)
	{
	}
};

struct MouseData {
	QRect selectionBox;
	bool leftButtonRelease;
	bool rightButtonRelease;
};

struct ThreadStatisticsData {
	int averageFPS;
	double nFramesProcessed;
	double averageVidProcessingFPS;
};

#endif // STRUCTURES_H
