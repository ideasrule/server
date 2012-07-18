// Empty project additions:
//		Added "AutoIt3.h" include
//		Added "AutoItX3.lib" to the input linker libraries
//
// AutoItX3.dll needs to be in the run path during execution

#include <Windows.h>
#include <assert.h>
#include <iostream>
#include "AutoIt3.h"
#include <Dwmapi.h>
#include <gdiplus.h>
#include <Gdipluspixelformats.h>
#include <time.h>
#include "opencv.hpp"
using namespace std;
using namespace cv;

class Bot {
	static const int WIDTH = 1264;
	static const int HEIGHT = 730;
	//int leftX;
	//int topY;
	HDC hDDC;
	HDC hCDC;
	HBITMAP oldBitmap;
	HBITMAP shot;
	HWND winHandle;
	ULONG_PTR gdiToken;
	Mat* shotAsMat;
	//bool shotLocked;
	Mat healthTemp;
	Mat hatredTemp;
	Mat disciplineTemp;
	int screenshotNum; //for debugging only

	static const int HEALTH_X = 339;
	static const int HEALTH_Y = 621;
	static const int HATRED_X = 895;
	static const int HATRED_Y = 621;
	static const int DISCIPLINE_X = 918;
	static const int DISCIPLINE_Y = 621;

	//hue difference thresholds
	static const int HEALTH_THRES = 1;
	static const int HATRED_THRES = 4;
	static const int DISCIPLINE_THRES = 12;
	//special saturation threshold for health
	static const int HEALTH_SAT_THRES = 20;
public:
	Mat debugTemp;
	Mat sqrDebugTemp;
	int nDebug;
	//339, 621, 20x99
	//895,621 (914 is right), 20x99
	//918, 621, 20x99
	/*
	const int HEALTH_X = 359;
	const int HEALTH_Y = 635;
	const int HEALTH_WIDTH = 12;
	const int HEALTH_HEIGHT = 96;

	const int HATRED_X = 895;
	const int HATRED_Y = 635;
	const int HATRED_WIDTH = 20;
	const int HATRED_HEIGHT = 96;

	const int DISCIPLINE_X = 921;
	const int DISCIPLINE_Y = 635;
	const int DISCIPLINE_WIDTH = 20;
	const int DISCIPLINE_HEIGHT = 96;
*/
//public:

	Bot(HWND handle) {
		//test(2,3,CV_8UC3);
		//shotLocked = false;
		healthTemp = imread("health_template.png",CV_LOAD_IMAGE_COLOR);
		hatredTemp = imread("hatred_template.png", CV_LOAD_IMAGE_COLOR);
		disciplineTemp = imread("discipline_template2.png", CV_LOAD_IMAGE_COLOR);
		debugTemp = Mat::zeros(99, 45, CV_32SC3);
		sqrDebugTemp = Mat::zeros(99,45,CV_32FC1);
		nDebug = 0;
		/*assert(healthTemp.rows == TEMP_HEIGHT && healthTemp.cols == TEMP_WIDTH
			&& hatredTemp.rows == TEMP_HEIGHT && hatredTemp.cols == TEMP_WIDTH
			&& disciplineTemp.rows == TEMP_HEIGHT
			&& disciplineTemp.cols == TEMP_WIDTH);*/
		cvtColor(healthTemp, healthTemp, CV_BGR2HSV);
		cvtColor(hatredTemp, hatredTemp, CV_BGR2HSV);
		cvtColor(disciplineTemp, disciplineTemp, CV_BGR2HSV);
		using namespace Gdiplus;
		GdiplusStartupInput gdiplusStartupInput;
		GdiplusStartup(&gdiToken, &gdiplusStartupInput, NULL);
		winHandle = handle;
		/*
		RECT tRect;
		HRESULT ret = DwmGetWindowAttribute(handle, DWMWA_EXTENDED_FRAME_BOUNDS, 
			&tRect, sizeof(tRect));
		if (ret != S_OK) {
			cout<<"Error getting Diablo window info"<<endl;
			return;
		}
		assert(tRect.right - tRect.left == WIDTH && tRect.bottom - tRect.top == HEIGHT);
		leftX = tRect.left;
		topY = tRect.top;*/
		//HWND hWnd = GetDesktopWindow();
		hDDC = GetDC(handle);
		hCDC = CreateCompatibleDC(hDDC);
		shot = CreateCompatibleBitmap(hDDC, WIDTH, HEIGHT);
		oldBitmap = (HBITMAP) SelectObject(hCDC, shot);
		shotAsMat = new Mat(HEIGHT, WIDTH, CV_8UC3);
		screenshotNum = 0;
	}

	void showDetectedLevel(Mat region, int row) {
		assert(region.rows > row);
		Mat temp = region.clone();
		Vec3b* p = temp.ptr<Vec3b>(row);
		for (int col = 0; col < temp.cols; col++) {
			p[col][0] = 0; //blue
			p[col][1] = 255; //green
			p[col][2] = 0; //red
		}
		imshow("Detected level", temp);
		waitKey(0);
	}

	double getLevel(Mat region, Mat temp, double hueThres=255, double satThres = 255, double valThres=255) {
		int failingLinesLim = 2; //CHANGE TO 4
		assert(region.rows == temp.rows && region.cols == temp.cols);
		//imshow("temp", temp);
		//imshow("region", region);
	//	waitKey(0);
				Mat hsvRegion;
		cvtColor(region, hsvRegion, CV_BGR2HSV);
		/*
		hsvRegion -= temp;
		vector<Mat> planes;
		split(hsvRegion, planes);
		imshow("diff", planes[0]);
		imshow("diff S", planes[1]);
		imshow("diffV", planes[2]);
		Mat dest;
		Canny(planes[2], dest, 50, 600);
		imshow("canny", dest);
		waitKey(0);*/
		
		double percentPastThres = 0;
		double percentPastThresLast = 0;
		int failingLines = 0;
		for (int i = temp.rows - 1; i >= 0; i--) {
			//cout<<i<<" ";
			Vec3b* pr = hsvRegion.ptr<Vec3b>(i);
			Vec3b* pt = temp.ptr<Vec3b>(i);
			int nPastThres = 0;
			int nTot = 0;
			double avgDiff = 0;
			for (int j = 0; j < temp.cols; j++) {
				int tempHue = pt[j][0];
				int tempSat = pt[j][1];
				int tempVal = pt[j][2];
				if (tempHue == 0 && tempSat == 0 && tempVal == 0) continue;
				int winHue = pr[j][0];
				int winVal = pr[j][2];
				int winSat = pr[j][1];
			//	int diff = (winHue - tempHue)*(winHue - tempHue) +
				//		(winSat - tempSat)*(winSat - tempSat);
				int hueDiff = abs(winHue - tempHue)%90;
				int satDiff = abs(winSat - tempSat);
				int valDiff = winVal - tempVal;
				//cout<<hueDiff<<" ";
				if (hueDiff > hueThres || satDiff > satThres || valDiff > valThres) nPastThres++;
				nTot++;
				//avgDiff += diff/temp.cols;
			}
			percentPastThres = (double) nPastThres/nTot;
			if (percentPastThres > 0.9) failingLines++;
			else failingLines = 0;
			if (failingLines == failingLinesLim) {
			//	cout << i << endl;	
				showDetectedLevel(region, i + failingLines - 1);
				//waitKey(0);
				return 1 - (double) (i + failingLines - 1)/temp.rows;
			}
			/*
			//cout<<percentPastThres<<endl;
			if (percentPastThres > 0.5 && percentPastThresLast > 0.5) {
				//cout <<percentPastThres<<" "<<percentPastThresLast<<endl;;
				showDetectedLevel(region, i+1);
				return 1 - (double) (i + 1)/temp.rows;
			}
			percentPastThresLast = percentPastThres;*/
		//	cout<<endl;
		}
		//imshow("whole region",)
		if (failingLines == 0) return 1;
		return 1 - (double) (failingLines - 1)/temp.rows;
	}

	double getHealth(Mat shot) {
		Mat region(shot, Rect(HEALTH_X, HEALTH_Y, healthTemp.cols,healthTemp.rows));
		return getLevel(region, healthTemp, HEALTH_THRES, HEALTH_SAT_THRES);
	}

	double getHatred(Mat shot) {
		Mat region(shot, Rect(HATRED_X, HATRED_Y, 
			hatredTemp.cols, hatredTemp.rows));
		return getLevel(region, hatredTemp, HATRED_THRES);
	}
	void blackOut(Mat& cannyImage, Mat temp) {
		for (int r=0; r < temp.rows; r++) {
			for (int c=0; c < temp.cols; c++) {
				Vec3b tempVal = temp.at<Vec3b>(r, c);
				if (tempVal[0]==0 && tempVal[1]==0 && tempVal[2]==0)
					cannyImage.at<uchar>(r, c) = 0;
			
			}
		}
	}

	double leastDiff(Mat shot) {
		Mat region(shot, Rect(DISCIPLINE_X, DISCIPLINE_Y, 
			disciplineTemp.cols, disciplineTemp.rows));
		Mat hsvRegion;
		cvtColor(region, hsvRegion, CV_BGR2HSV);
		double* sumDiffs = new double[hsvRegion.rows];
		int* nums = new int[hsvRegion.rows];
		for (int r=0; r < hsvRegion.rows; r++) {
			if (r != 0) {
				sumDiffs[r] = sumDiffs[r-1];
				nums[r] = nums[r-1];
			}
			else {
				sumDiffs[0] = 0;
				nums[0] = 0;
			}
			for (int c=0; c < hsvRegion.cols; c++) {
				Vec3b tempVal = disciplineTemp.at<Vec3b>(r, c);
				Vec3b val = region.at<Vec3b>(r, c);
				if (tempVal[0]==0 && tempVal[1]==0 && tempVal[2]==0) continue;
				nums[r]++;
				double diff = abs(tempVal[2]-val[2]);
				Vec3b zero = 0;
				if (diff < 35) {
					region.at<Vec3b>(r,c)[0] = 0;
					region.at<Vec3b>(r,c)[1] = 0;
					region.at<Vec3b>(r,c)[2] = 0;
				}
				sumDiffs[r] += diff;
			}
		}
		int totNum = nums[hsvRegion.rows - 1];
		int totSum = sumDiffs[hsvRegion.rows - 1];
		double max = -1;
		double maxRow = -1;
		for (int r=0; r < hsvRegion.rows - 1; r++) {
			double topAvgDiff = sumDiffs[r]/nums[r];
			double topBotDiff = (totSum - sumDiffs[r])/(totNum - nums[r]);
			cout << topAvgDiff - topBotDiff<<endl;
			if (topAvgDiff - topBotDiff > max) {
				max = topAvgDiff - topBotDiff;
				maxRow = r;
			}
		}
		showDetectedLevel(region, maxRow);
		return maxRow;
	}

	double getDiscipline(Mat shot, int firstThres, int secThres) {
		Mat region(shot, Rect(DISCIPLINE_X, DISCIPLINE_Y, 
			disciplineTemp.cols, disciplineTemp.rows));
		Mat hsvRegion;
		cvtColor(region, hsvRegion, CV_BGR2HSV);
		
		hsvRegion -= disciplineTemp;
		vector<Mat> planes;
		split(hsvRegion, planes);
		//imshow("diff", planes[0]);
		//imshow("diff S", planes[1]);
		//imshow("diffV", planes[2]);
		Mat dest;
		Mat colorDest;
		Canny(planes[2], dest, 100, 700);
		blackOut(dest, disciplineTemp);
		//cvtColor( dest, colorDest, CV_GRAY2BGR );

		//blur(planes[2], planes[2], Size(3,3));
		vector<Vec2f> lines;
		HoughLines(dest, lines, 1, CV_PI/180, 15 );
	//	cout<<lines.size()<<endl;
		for( size_t i = 0; i < lines.size(); i++ )
		{
			float rho = lines[i][0];
			float theta = lines[i][1];
			double a = cos(theta), b = sin(theta);
			double x0 = a*rho, y0 = b*rho;
			if (abs(theta - CV_PI/2) > 0.05) {
				return 1 - y0/disciplineTemp.rows;
				
			}
			//Point pt1(cvRound(x0 + 1000*(-b)),
					//  cvRound(y0 + 1000*(a)));
		//	Point pt2(cvRound(x0 - 1000*(-b)),
					//  cvRound(y0 - 1000*(a)));
		//	line( colorDest, pt1, pt2, Scalar(0,0,255), 1, 8 );
		}


		//imshow("canny", colorDest);
		//waitKey(0);




		//blur(region, region, Size(3,3));
		/*
		Mat convertedRegion(99,45,CV_32SC3);
		region.convertTo(convertedRegion,convertedRegion.type());
		debugTemp = debugTemp + convertedRegion;

		//now do standard deviation
		for (int r=0; r < 99; r++) {
			for(int c = 0; c < 45; c++) {
				int val = region.at<Vec3b>(r,c)[0];
				sqrDebugTemp.at<float>(r,c) += val*val;
			//	cout << val<<endl;
			}
		}
		nDebug++;*/
		return -1;
		//return getLevel(region, disciplineTemp, 255, 255, 42);
	}
	double getDiscipline2(Mat shot) {
		Mat region(shot, Rect(DISCIPLINE_X, DISCIPLINE_Y, 
			disciplineTemp.cols, disciplineTemp.rows));
		Mat hsvRegion;
		cvtColor(region, hsvRegion, CV_BGR2HSV);
		//imshow("test",region);
		//waitKey(0);
		//216,150,221
		//Mat region;
		//cvtColor(originalRegion, region, CV_BGR2HSV);

		//108, 150,221
		int refBlue = 221;
		int refGreen = 144;
		int refRed = 91;
		for (int r=0; r < disciplineTemp.rows; r++) {
			int nQualifying = 0;
			int nMatchingTemp = 0;
			int nTot = 0;
			for (int c=0; c < disciplineTemp.cols; c++) {
				Vec3b val = region.at<Vec3b>(r, c);
				Vec3b hsvVal = hsvRegion.at<Vec3b>(r, c);
				Vec3b tempVal = disciplineTemp.at<Vec3b>(r, c);
				if (tempVal[0]==0 && tempVal[1]==0 && tempVal[2]==0) continue;
				int diffB = abs(val[0]-refBlue);
				int diffG = abs(val[1]-refGreen);
				int diffR = abs(val[2]-refRed);
				//cout<<(int)val[0]<<" "<<(int)val[1]<<" "<<(int)val[2]<<endl;
				//cout<<diffHue<<" "<<diffSat<<" "<<diffVal<<endl;

				int diffH = abs(hsvVal[0] - tempVal[0])%90;
				int diffS = abs(hsvVal[1] - tempVal[1]);
				int diffV = abs(hsvVal[2] - tempVal[2]);
				if (diffH < 7 && diffS < 60) nMatchingTemp++;
				if (diffB < 50 && diffG < 30 && diffR < 30)
					nQualifying++;
				nTot++;
			}
			//cout<<(double)nQualifying/nTot<<endl;
			if ((double) nQualifying/nTot > 0.5 || (double) nMatchingTemp/nTot > 0.5) {
				cout<<r<<endl;

				//cout<<nQualifying/nTot<<endl;
				return 1 - (double) r/disciplineTemp.rows;
			}
		}

		return 0;
	}
	void saveSnapshot(int sleepTime) {
		//should be used for debugging only!
		Sleep(sleepTime);
		//updateShot();
		stringstream filename;
		filename << "screenshot" << screenshotNum << ".png";
		imwrite("screenshot4.png", *shotAsMat);
	}
	void updateShot() {
		using namespace Gdiplus;
		//oldBitmap = (HBITMAP) SelectObject(hCDC, shot);
		//BitBlt(hCDC, 0, 0, WIDTH, HEIGHT, hDDC, leftX, topY, SRCCOPY);
		SelectObject(hCDC, shot);
		BitBlt(hCDC, 0, 0, WIDTH, HEIGHT, hDDC, 0, 0, SRCCOPY);
		SelectObject(hCDC, oldBitmap);
		Bitmap shotBitmap(shot, NULL);
		BitmapData source;
		Gdiplus::Rect bounds(0, 0, shotBitmap.GetWidth(), shotBitmap.GetHeight());
		Status status = shotBitmap.LockBits(&bounds, ImageLockModeRead, PixelFormat24bppRGB, &source);
		if (status != Ok) {
			cout << "error locking bitmap bits!" << endl;
			return;
		}

	//	shotAsMat = new Mat(shotBitmap.GetHeight(), shotBitmap.GetWidth(), CV_8UC3);
		BYTE* dest = (BYTE*) shotAsMat->data;
		for (int y = 0; y < source.Height; y++) {
			BYTE* src = (BYTE*) source.Scan0 + y * source.Stride;
			BYTE* dst = (BYTE*) (dest + y * shotAsMat->step);
			memcpy(dst, src, 3*source.Width);
		}
		shotBitmap.UnlockBits(&source);
		
		//double health = getHealth(*shotAsMat);
		//double hatred = getHatred(*shotAsMat);
		double discipline = leastDiff(*shotAsMat);
		//if (discipline < 1)
			//imwrite("autobot3.png", *shotAsMat);
		//cout << health << " " << hatred << " " 
		cout	<< discipline << endl;
		//imwrite("autobot2.png",*shotAsMat);
		//if (hatred < 0.9 || discipline < 0.9) {
			//imwrite("autobot2.png",*shotAsMat);
			//exit(1);
		//}
	}
	~Bot() {
		ReleaseDC(winHandle, hDDC);
		DeleteDC(hCDC);
		Gdiplus::GdiplusShutdown(gdiToken);
		delete shotAsMat;
	}
};



/*
void showDetectedLevel(Mat region, int row) {
	assert(region.rows > row);
	Mat temp = region.clone();
	Vec3b* p = temp.ptr<Vec3b>(row);
	for (int col = 0; col < temp.cols; col++) {
		p[col][0] = 0; //blue
		p[col][1] = 255; //green
		p[col][2] = 0; //red
	}
	imshow("Detected level", temp);
	waitKey(0);
}
double getLevel(Mat region, Mat temp, double thres) {
	assert(region.rows == temp.rows && region.cols == temp.cols);
	//imshow("temp", temp);
	//imshow("region", region);
	//waitKey(0);
	Mat hsvRegion;
	cvtColor(region, hsvRegion, CV_BGR2HSV);
	double percentPastThres = 0;
	double percentPastThresLast = 0;
	for (int i = temp.rows - 1; i >= 0; i--) {
		Vec3b* pr = hsvRegion.ptr<Vec3b>(i);
		Vec3b* pt = temp.ptr<Vec3b>(i);
		int nPastThres = 0;
		int nTot = 0;
		for (int j = 0; j < temp.cols; j++) {
			int tempHue = pt[j][0];
			int tempSat = pt[j][1];
			int tempVal = pt[j][2];
			if (tempHue == 0 && tempSat == 0 && tempVal == 0) continue;
			int winHue = pr[j][0];
		//	int winSat = pr[j][1];
		//	int diff = (winHue - tempHue)*(winHue - tempHue) +
			//		(winSat - tempSat)*(winSat - tempSat);
			int diff = abs(winHue - tempHue)%90;
			cout<<diff<<" ";
			if (diff > thres) nPastThres++;
			nTot++;
		}
		percentPastThres = (double) nPastThres/nTot;
		//cout<<percentPastThres<<endl;
		if (percentPastThres > 0.5 && percentPastThresLast > 0.5) {
			cout <<percentPastThres<<" "<<percentPastThresLast<<endl;;
			showDetectedLevel(region, i+1);
			//return 1 - (i + 1)/temp.rows;
		}
		percentPastThresLast = percentPastThres;
		cout<<endl;
	}
	//imshow("whole region",)
	return 1;
}*/

HBITMAP screenCapture(int left=0, int top=0, int right=-1, int bot=-1) {
	if (right == -1) right = GetSystemMetrics(SM_CXSCREEN);
	if (bot == -1) bot = GetSystemMetrics(SM_CYSCREEN);
	assert(right >= left);
	assert(bot >= top);
	int w = right - left + 1;
	int h = bot - top + 1;
	HWND hWnd = GetDesktopWindow();
	HDC hDDC = GetDC(hWnd);
	HDC hCDC = CreateCompatibleDC(hDDC);
	HBITMAP hBMP = CreateCompatibleBitmap(hDDC, w, h);
	SelectObject(hCDC, hBMP);
	BitBlt(hCDC, 0, 0, w, h, hDDC, left, top, SRCCOPY);
	ReleaseDC(hWnd, hDDC);
	DeleteDC(hCDC);
	return hBMP;
}
HBITMAP screenCaptureWnd(HWND hWnd, int left=0, int top=0, int right=-1, int bot=-1) {
	RECT tRect;
	HRESULT ret = DwmGetWindowAttribute(hWnd, DWMWA_EXTENDED_FRAME_BOUNDS, 
		&tRect, sizeof(tRect));
	cout<<(ret&0xffff)<<endl;
	if (ret != S_OK || abs(tRect.left) + abs(tRect.top) + 
		abs(tRect.right) + abs(tRect.bottom) == 0)
	{
		if (!GetWindowRect(hWnd, &tRect)) {
			cout << "screen capture failed!";
			return NULL;
		}
	}
	
	left += tRect.left;
	top += tRect.top;
	/*
	left = t.left
	top = t.top
	right = t.right - 1
	bot = t.bottom - 1

	*/
	if (right == -1) right = tRect.right - tRect.left - 1;
	if (bot == -1) bot = tRect.bottom - tRect.top - 1;
	right += tRect.left;
	bot += tRect.top;
	if (left > tRect.right) left = tRect.left;
	if (top > tRect.bottom) top = tRect.top;
	if (right > tRect.right) right = tRect.right;
	if (bot > tRect.bottom) bot = tRect.bottom;
	return screenCapture(left, top, right, bot);
}
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	using namespace Gdiplus;
   UINT  num = 0;          // number of image encoders
   UINT  size = 0;         // size of the image encoder array in bytes

   ImageCodecInfo* pImageCodecInfo = NULL;

   GetImageEncodersSize(&num, &size);
   if(size == 0)
      return -1;  // Failure

   pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
   if(pImageCodecInfo == NULL)
      return -1;  // Failure

   GetImageEncoders(num, size, pImageCodecInfo);

   for(UINT j = 0; j < num; ++j)
   {
      if( wcscmp(pImageCodecInfo[j].MimeType, format) == 0 )
      {
         *pClsid = pImageCodecInfo[j].Clsid;
         free(pImageCodecInfo);
         return j;  // Success
      }
   }

   free(pImageCodecInfo);
   return -1;  // Failure
}

void getAvg(Mat region, Mat temp) {
	//row -= 621;
	/*
	Vec3f total = 0;
	Vec3f totalSqr = 0;
	double n = 0;
	for(int c = 0;  c < temp.cols; c++) {
		Vec3b val = region.at<Vec3b>(row, c);
		Vec3b tempVal = temp.at<Vec3b>(row, c);
		if (tempVal[0]==0 && tempVal[1]==0 && tempVal[2]==0) continue;
		total += val;
		Vec3f squares = 0;
		squares[0] = val[0]*val[0];
		squares[1] = val[1]*val[1];
		squares[2] = val[2]*val[2];
		totalSqr += squares;
		n++;
		//if (temp.at<uchar>(row, c)) total +
	}
	total = total/n;
	totalSqr /= n;
	totalSqr[0] -= total[0]*total[0];
	totalSqr[1] -= total[1]*total[1];
	totalSqr[2] -= total[2]*total[2];
	cout<<(int)total[0]<<" "<<(int)total[1]<<" "<<(int)total[2]<<endl;
	cout<<(int)totalSqr[0]<<" "<<(int)totalSqr[1]<<" "<<(int)totalSqr[2]<<endl;*/
	//cvtColor(region, region, CV_BGR2HSV);
	//imshow("region", region);
	//waitKey(0);
	int refHue = 221;
	int refSat = 144;
	int refVal = 91;


	for (int r=0; r < temp.rows; r++) {
		int nQualifying = 0;
		int nTot = 0;
		for (int c=0; c < temp.cols; c++) {
			Vec3b val = region.at<Vec3b>(r, c);
			Vec3b tempVal = temp.at<Vec3b>(r, c);
			if (tempVal[0]==0 && tempVal[1]==0 && tempVal[2]==0) continue;
			int diffHue = abs(val[0]-refHue)%90;
			int diffSat = abs(val[1]-refSat);
			int diffVal = abs(val[2]-refVal);
			//cout<<(int)val[0]<<" "<<(int)val[1]<<" "<<(int)val[2]<<endl;
			cout<<diffHue<<" "<<diffSat<<" "<<diffVal<<endl;
			if (diffHue < 30 && diffSat < 30 && diffVal < 30)
				nQualifying++;
			nTot++;
		}
		cout<<(double)nQualifying/nTot<<endl;
		if ((double) nQualifying/nTot > 0.5) {
			cout<<r<<endl;
			return;
		}
	}
}

void saveImage(HBITMAP handle) {
	using namespace Gdiplus;
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	Bitmap* hImage = Bitmap::FromHBITMAP(handle, NULL);
	int x = hImage->GetWidth();
	int y = hImage->GetHeight();
	//Bitmap* hClone = Gdiplus::Bitmap::
	Bitmap* hClone = hImage->Clone(0, 0, x, y, PixelFormat24bppRGB);
	CLSID pngClsid;
	GetEncoderClsid(L"image/png", &pngClsid);
	//image.Save(L"Mosaic2.png", &pngClsid, NULL);
	hClone->Save(L"screenshot.png", &pngClsid, NULL);
	//Gdiplus::GdiplusStartup();
}

int main2(int argn, char** argv)
{/*
	//double f;
	double theta;
	double h;
	double PI = 3.1415926;
	double squared_error = 1e10;
	cout<<sin(0.2)<<endl;
	
	for (h=0; h < 100000; h += 1) {
		for (theta=PI/2; theta > 0; theta -= 0.1) {
			//double f1 = -15.595*(-sin(theta)*(18+h*tan(theta))-h*cos(theta));
			//double f2 = -24.756*(-sin(theta)*(-17.5+h*tan(theta))-h*cos(theta));
			//double f3 = -18.738*(-sin(theta)*(-2+h*tan(theta))-h*cos(theta));
			double f1 = 15.537*(h + 17.5/2*sin(2*theta))/cos(theta);
			double f2 = 24.756*(h - 17.5/2*sin(2*theta))/cos(theta);
			double f3 = 18.738*(h - 2/2*sin(2*theta))/cos(theta);
			double diff1 = abs(f2-f1);
			double diff2 = abs(f3-f1);
			double perdiff1 = diff1/f1;
			double perdiff2 = diff2/f1;
			
			double by = -16.5;
			double y = -2;
			double f4 = by*(h*cos(theta)+(y+h*tan(theta))*sin(theta))/(-h*cos(theta)+(y+h*tan(theta))*sin(theta));
			double perdiff3 = abs(f4-f1)/f1;
			double err = perdiff1*perdiff1 + perdiff2*perdiff2 +perdiff3*perdiff3;
			if (err < squared_error) {
				squared_error = err;
				cout<<h<<" "<<theta<<" "<<sqrt(squared_error)<<" "<<f4-f1<<" "<<sqrt(squared_error)<<endl;
			}
			//if (err < squared_error) {
				//cout<<theta<<" "<<h<<" "<<err<<endl;
				//squared_error = err;
			//}
			//cout<<f1<<" "<<diff1<<" "<<diff2<<endl;
			//if (f1 != 0 && diff1/abs(f3) < 0.1 && diff2/abs(f3) < 0.1) cout<<theta<<" "<<h<<" "<<diff1/f1<<" "<<diff2/f1<<endl;
			//cout<<f2-f1<<" "<<f3-f1<<" "<<endl;
		}
	}
	*/
	/*
	Mat win = imread("C:\\Users\\Zerg Kerrigan\\test3.bmp",CV_LOAD_IMAGE_COLOR);
	Mat region(win, Rect(HEALTH_X, HEALTH_Y, HEALTH_WIDTH, HEALTH_HEIGHT));
	Mat temp = imread("C:\\Users\\Zerg Kerrigan\\health_template.png",CV_LOAD_IMAGE_COLOR);
	cvtColor(temp, temp, CV_BGR2HSV);
	cout<< getLevel(region, temp, 2) << endl;
	exit(-1);
 	// You can now call AutoIt commands, e.g. to send the keystrokes "hello"
	HMODULE autoLib = LoadLibrary("AutoItX3_x64.dll"); 
	typedef void (WINAPI *sleepPtr)(int time);
	typedef long (WINAPI *runPtr)(LPCWSTR, LPCWSTR, long);
	typedef long (WINAPI *waitPtr)(LPCWSTR, PCWSTR, long);
	typedef long (WINAPI *sendPtr)(LPCWSTR, long);
	sleepPtr WINAPI AU3_Sleep = (sleepPtr) GetProcAddress(autoLib, "AU3_Sleep");
	runPtr WINAPI AU3_Run = (runPtr) GetProcAddress(autoLib, "AU3_Run");
	waitPtr WINAPI AU3_WinWaitActive = (waitPtr) GetProcAddress(autoLib, "AU3_WinWaitActive");
	sendPtr WINAPI AU3_Send = (sendPtr) GetProcAddress(autoLib, "AU3_Send");
//	long (*AU3_WinWaitActive)(LPCWSTR, PCWSTR, long)
	AU3_Sleep(1000);
	AU3_Run(L"notepad.exe", L"", 1);
	AU3_WinWaitActive(L"Untitled -", L"", 0);
	AU3_Send(L"Hello{!}", 0);
	HBITMAP h = screenCapture();
	saveImage(h);
	cout<<"loop started"<<endl;
	//fflush(stdin);
	time_t start = time(NULL);
	for (int i=0; i < 1000; i++) {
		//cout<<"loop started";
		screenCapture();
	}
	time_t end = time(NULL);
	cout<<end - start<<endl;
	*/


	
	LPCWSTR title = L"Diablo III";
	HMODULE autoLib = LoadLibrary("AutoItX3_x64.dll"); 
	typedef long (WINAPI *waitPtr)(LPCWSTR, PCWSTR, long);
	typedef void (WINAPI *handlePtr)(LPCWSTR, LPCWSTR, LPWSTR, int);
	typedef void (WINAPI *activatePtr)(LPCWSTR, LPCWSTR);
	typedef long (WINAPI *resizePtr) (LPCWSTR, LPCWSTR, long, long, long, long);
	typedef long (WINAPI *posPtr) (LPCWSTR, LPCWSTR);

	handlePtr WinGetHandle = (handlePtr) GetProcAddress(autoLib, "AU3_WinGetHandle");
	waitPtr WinWaitActive = (waitPtr) GetProcAddress(autoLib, "AU3_WinWaitActive");
	activatePtr WinActivate = (activatePtr) GetProcAddress(autoLib, "AU3_WinActivate");
	resizePtr WinMove = (resizePtr) GetProcAddress(autoLib, "AU3_WinMove");
	posPtr WinGetPosX = (posPtr) GetProcAddress(autoLib, "AU3_WinGetPosX");
	posPtr WinGetPosY = (posPtr) GetProcAddress(autoLib, "AU3_WinGetPosY");

	//WinActivate(title, L"");
	//WinWaitActive(title, L"", 0);
	//have to get window position so that WinMove only rescales, instead of move + rescale
	long x = WinGetPosX(title, L"");
	long y = WinGetPosY(title, L"");

	WinMove(title, L"", x, y, 1280, 768);
	
	WCHAR handleArr[33];
	WinGetHandle(L"Diablo III", L"", handleArr, 32);
	HWND handle = (HWND) wcstol(handleArr, NULL, 16);
	
	Bot myBot(handle);
	//myBot.saveSnapshot(5000);
	cout<<time(NULL)<<endl;
	//myBot.updateShot();
	//myBot.updateShot("debug2_.png");
	for (int i=0; i < 1000; i++) {
		//myBot.updateShot();
	}

//	return 0;
	/*
	Mat finalTemplate(99,45,CV_8UC3);
	myBot.debugTemp = myBot.debugTemp/myBot.nDebug;
	myBot.sqrDebugTemp /= myBot.nDebug;
	//myBot.sqrDebugTemp -= myBot.debugTemp*myBot.debugTemp;

	Mat grayscale(99,45,CV_8UC1);
	for (int r = 0; r < 99; r++) {
		for (int c = 0; c < 45; c++) {
			float val = myBot.sqrDebugTemp.at<float>(r, c);
			float avg = myBot.debugTemp.at<Vec3i>(r,c)[0];
			//cout<<val<<" "<<avg<<endl;
			grayscale.at<uchar>(r,c) = sqrt(val - avg*avg);
			cout << sqrt(val-avg*avg)<<endl;
		}
	}
	myBot.debugTemp.convertTo(finalTemplate, finalTemplate.type());
	imwrite("finaltemplate.png",finalTemplate);

	imwrite("stddev.png",grayscale);
	cout<<time(NULL)<<endl;
	return 0;
	*/
	Mat testImage = imread(argv[1], CV_LOAD_IMAGE_COLOR);
//	Mat region;
	//myBot.getDiscipline(testImage,
	//Mat temp = imread("discipline_template2.png", CV_LOAD_IMAGE_COLOR);

	//Mat region(testImage, Rect(918, 621, 45, 99));
	//getAvg(region, temp);
	//cout<< myBot.getHealth(testImage) << endl;
	//cout<<myBot.getHatred(testImage)<<endl;
	
	//cout<<myBot.getDiscipline(testImage)<<endl;
	//cout<<time(NULL)<<endl;


	//cout<<handle<<endl;
	//HBITMAP shot = screenCaptureWnd(handle);
	//saveImage(shot);*/



	return 0;
}
