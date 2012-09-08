#include "ItemFinder.hpp"
#include "opencv.hpp"
#include <time.h>
using namespace cv;
using namespace std;
	static bool isWhite(Vec3b pixel) {
		if (pixel[0] > 150 && pixel[1] > 150
			&& pixel[2] > 150) return true;
		return false;
	}
	static bool isBlue(Vec3b hsvPixel) {
		int diffHue = abs(hsvPixel[0] - 120);
		int diffSat = abs(hsvPixel[1] - 145);
		//2 and 8 work well
		if (diffHue <= 2 && diffSat <= 8) return true;
		return false;
	}

	static bool isBlueOrYellow(Vec3b hsvPixel) {
		//is this blue?
		int diffHue = abs(hsvPixel[0] - 120);
		int diffSat = abs(hsvPixel[1] - 145);
		if (diffHue <= 2 && diffSat <= 8) return true;
		//return false;
		//is this yellow?
		diffHue = abs(hsvPixel[0] - 30);
		diffSat = abs(hsvPixel[1] - 250);
		int diffVal = 255 - hsvPixel[2];
		//if (diffHue <= 2 && diffSat <= 6 && diffVal <= 5) return true;
		if (diffHue <= 2 && diffSat <= 6 && diffVal <= 75) return true;
		return false;
		////+- 1, +- 8
	}

	static bool isRed(Vec3b hsvPixel) {
		int diffHue = abs(hsvPixel[0] - 178);
		if (diffHue > 90) diffHue = 180 - diffHue;
		//int diffSat = abs(hsvPixel[1] - 248);
		if (diffHue <= 6 && hsvPixel[1] >= 236) return true;
		return false;
	}
	ItemFinder::ItemFinder() {
		//cout <<"hello"<<endl;
		Mat goldTempfull = imread("gold.png", CV_LOAD_IMAGE_COLOR);
		goldTempfull = applyLim(goldTempfull, isWhite);
		goldTempfull.convertTo(goldTempfull, CV_32FC3);
		vector<Mat> tempPlanes;
		split(goldTempfull, tempPlanes);
		goldTemp = tempPlanes[0].clone();

		endTemp = imread("questend.png", CV_LOAD_IMAGE_COLOR);
	//	endTemp.convertTo(endTemp, CV_32FC3);

		hateDepletedTemp = imread("hatredtemp.png", CV_LOAD_IMAGE_COLOR);
		cvtColor(hateDepletedTemp, hateDepletedTemp, CV_BGR2HSV);
		hateDepletedTemp = applyLim(hateDepletedTemp, isRed);
		//hateDepletedTemp.convertTo(hateDepletedTemp, CV_32FC3);

		boxTemp = imread("chesttemp.png", CV_LOAD_IMAGE_COLOR);
		boxTemp.convertTo(boxTemp, CV_32FC3);

		merchantTemp = imread("merchanttemp.png", CV_LOAD_IMAGE_COLOR);
		merchantTemp.convertTo(merchantTemp, CV_32FC3);

		invTemp = imread("invtemp2.png", CV_LOAD_IMAGE_COLOR);
		endmenuTemp = imread("endmenutemp.png", CV_LOAD_IMAGE_COLOR);
		//imshow("goldTemp", goldTemp);
		//waitKey(0);
	}

	bool ItemFinder::invSpotEmpty(Mat shot, int r, int c) {
		int rowLocs[] = {1, 34, 66, 98, 131, 163};
		int colLocs[] = {1, 34, 67, 99, 132, 165, 198, 231, 264, 296};
		Mat tempRegion(invTemp, Rect(colLocs[c], rowLocs[r], 27, 27));
		Mat imRegion(shot, Rect(921 + colLocs[c], 397 + rowLocs[r], 27, 27));
		double score = correlate(tempRegion, imRegion);
		//cout<<score<<endl;
	//	cout << score;
		if (score > 0.9) return true;
		return false;
	}

	Mat ItemFinder::applyLim(Mat original, bool (*isValid)(Vec3b)) {
		Mat image(original.rows, original.cols, CV_8UC3);
		//everything that isn't valid is zeroed out
		for (int r = 0; r < original.rows; r++) {
			Vec3b* ptr = original.ptr<Vec3b>(r);
			Vec3b* resultPtr = image.ptr<Vec3b>(r);
			for (int c = 0; c < original.cols; c++) {
				if (!isValid(ptr[c])) resultPtr[c] = 0;
				else resultPtr[c] = ptr[c];
			}
		}
		return image;
	}
	double ItemFinder::correlate(Mat im1, Mat im2) {
		assert(im1.rows == im2.rows && im1.cols == im2.cols);
		double firstMag = sqrt(im1.dot(im1));
		double secondMag = sqrt(im2.dot(im2));
		if (firstMag == 0 || secondMag == 0) return 0;
		return im1.dot(im2)/firstMag/secondMag;
	}
	bool ItemFinder::questEnded(Mat shot) {
		Mat finishRegion(shot, Rect(492, 507, 277, 136));
		double score = correlate(finishRegion, endTemp);
		if (score > 0.95) return true;
		return false;
	}
	bool ItemFinder::isScreenSeen(Mat shot, int what, double thres) {
		Mat region;
		Mat temp;
		switch(what) {
		case OK_SCREEN:
			region = shot(Rect(475,275,314,184));
			temp = imread("ok_screen.png");
			break;
		case GAME_START_SCREEN:
			region = shot(Rect(48, 313, 220,33));
			temp=imread("game_start_screen.png");
			break;
		case MENU_SCREEN:
			region = shot(Rect(230,460,292,64));
			temp = imread("menu_screen.png");
			break;
		case KEEP0_SCREEN:
			region = shot(Rect(240,665, 83,65));
			temp = imread("keep0_screen.png");
			break;
		case WAYPOINT_SCREEN:
			region = shot(Rect(30,110,270,210));
			temp = imread("waypoint_screen.png");
			break;
		case KEEP_LEVEL3_SCREEN:
			region = shot(Rect(1075,8,173,10));
			temp = imread("keep_level3_screen.png");
			break;
		case LARDER_SCREEN:
			region = shot(Rect(1168,8,79,10));
			temp = imread("larder_screen.png");
			break;
		case END_KEEP_SCREEN:
			region = shot(Rect(1051,8,68,10));
			temp = imread("end_keep_screen.png");
			break;
		case EARLY_ABORT_MENU_SCREEN:
			region = shot(Rect(230,415,145,107));
			temp = imread("early_abort_menu_screen.png");
			break;
		};
		double score = correlate(region, temp);
		//cout << score << endl;
		if (score > thres) return true;
		return false;
	}

	vector<Point> ItemFinder::segregateY(vector<Point> original, int minDist) {
		vector<Point> result;

		for (int i=0; i < original.size(); i++) {
			if (result.size() == 0) result.push_back(original[i]);
			else {
				Point back = result.back();
				if (abs(back.y - original[i].y) > minDist) 
					result.push_back(original[i]);
			}
		}
		return result;
	}

	vector<Point> ItemFinder::findTemp(Mat shot, Mat temp, double thres) {
		Mat floatingPoint;
		shot.convertTo(floatingPoint, CV_32FC3);
		temp.convertTo(temp, CV_32FC3);
		Mat result;
		matchTemplate(floatingPoint, temp, result, CV_TM_CCOEFF_NORMED);
		imwrite("diablo9_result.png",255*result);
	//	imshow("result", result);
		//waitKey(0);
		vector<Point> locs;
		for (int r = 0; r < result.rows; r++) {
			float* ptr = result.ptr<float>(r);
			for (int c = 0; c < result.cols; c++) {
				if (ptr[c] > thres) {
					int midMatchRow = r + temp.rows/2;
					int midMatchCol = c + temp.cols/2;
					locs.push_back(Point2i(midMatchCol, midMatchRow));
				}
			}
		}
		return locs;
	}

	bool ItemFinder::boxExists(Mat region) {
		vector<Point> points = findTemp(region, boxTemp, 0.9);
		assert(points.size() <= 1);
		return points.size() == 1;
	}

	Point* ItemFinder::nextItem(Mat shot, int startRow) {
		Mat region(shot, Rect(0, 0, shot.cols, 620));
		Mat hsvIm;
		cvtColor(region, hsvIm, CV_BGR2HSV);
		Mat binary(region.rows, region.cols, CV_8UC1);
		for (int r = 0; r < region.rows; r++) {
			Vec3b* ptr = hsvIm.ptr<Vec3b>(r);
			uchar* binPtr = binary.ptr<uchar>(r);
			for (int c = 0; c < region.cols; c++) {
				if (isBlueOrYellow(ptr[c])) binPtr[c] = 0;
				else binPtr[c] = 255;
			}
		}
		//imshow("binary", binary);
		//Mat filtered = applyLim(hsvIm, isBlueOrYellow);
		Mat distances;
		distanceTransform(binary, distances, CV_DIST_L2,  5);

	////	imwrite("distances2.png", distances);
		//imshow("distances", distances);
		//waitKey(0);

		Point* bestPoint = NULL;
		int bestCount = 0;
		for (int r = startRow; r < distances.rows; r++) {
			int count = 0;
			float* ptr = distances.ptr<float>(r);
			vector<int> pixelLocs;
			for (int c = 0; c < distances.cols; c++) {
				if (ptr[c] <= 3) {
					pixelLocs.push_back(c);
					count++;
				}
			}
			if (count > 45 && count > bestCount) {
				bestCount = count;
				int col = (int) pixelLocs.size()/2;
				bestPoint = new Point(pixelLocs[col], r);
			}
		}
		return bestPoint;
		/*
		Point* bestPoint = NULL;
		int bestCount = 0;
		for (int r = startRow; r < filtered.rows; r++) {
			int count = 0;
			Vec3b* ptr = filtered.ptr<Vec3b>(r);
			vector<int> pixelLocs;
			for (int c = 0; c < filtered.cols; c++) {
				if (ptr[c][0] != 0 || ptr[c][1] != 0 || ptr[c][2] != 0) {
					pixelLocs.push_back(c);
					count++;
				}
			}
			if (count > 40 && count > bestCount) {
				bestCount = count;
				int col = pixelLocs.size()/2;
				bestPoint = new Point(pixelLocs[col], r);
				
			}
		}
		return bestPoint;*/
	}
	bool ItemFinder::hatredDepleted(Mat shot) {
		Mat region(shot, Rect(522, 90, 218, 13));
		Mat hsvRegion;
		cvtColor(region, hsvRegion, CV_BGR2HSV);
		Mat filtered = applyLim(hsvRegion, isRed);
		double score = correlate(filtered, hateDepletedTemp);
		return (score > 0.8);
	}

	bool ItemFinder::couldNotSell(Mat shot) {
		Mat temp = imread("cannot_be_sold_temp.png");
		cvtColor(temp, temp, CV_BGR2HSV);
		temp = applyLim(temp, isRed);
		Mat region(shot,Rect(552,92,157,13));
		Mat hsvRegion;
		cvtColor(region, hsvRegion, CV_BGR2HSV);
		Mat filtered = applyLim(hsvRegion, isRed);
	//	imshow("filtered", filtered);
	//	imshow("temp", temp);
	//	cout << correlate(filtered, temp) << endl;
	//	waitKey(0);
		return correlate(filtered, temp) > 0.8;
	}

	vector<Point> ItemFinder::findAllGold(Mat shot) {
		Mat filtered = applyLim(shot, isWhite);
		Mat temp = imread("gold.png");
		temp = applyLim(temp, isWhite);
		return findTemp(shot, temp, 0.7);
	}
	/*
	Point* ItemFinder::findGold(Mat shot, double thres) {
		shot = applyLim(shot, isWhite);
		Mat floatingPoint;
		shot.convertTo(floatingPoint, CV_32FC3);
		goldTemp.convertTo(goldTemp, CV_32FC3);

		Mat searchRegion = floatingPoint(Rect(450,60,572,462));
		Mat result;
		cout<<"matching";
		matchTemplate(searchRegion, goldTemp, result, CV_TM_CCOEFF_NORMED);
		cout<<"matching end"<<endl;
		imshow("searchreg", searchRegion);
		imshow("temp", goldTemp);
		imshow("result",result);
		waitKey(0);
		Point* bestLoc = NULL;
		double bestScore = -1;

		for (int r = 0; r < result.rows; r++) {
			float* ptr = result.ptr<float>(r);
			for (int c = 0; c < result.cols; c++) {
				if (ptr[c] > thres)cout << "score" << ptr[c] << endl;
				if (ptr[c] > bestScore && ptr[c] > thres) {
					cout << bestScore << endl;
					int midMatchRow = 60 + r + goldTemp.rows/2;
					int midMatchCol = 450 + c + goldTemp.cols/2;
					bestLoc = new Point(midMatchCol, midMatchRow);
					bestScore = ptr[c];	
				}
			}
		}
		return bestLoc;
	}*/

	Point* ItemFinder::findGold(Mat shot, double thres) {
		cout<<"finding gold"<<endl;
		Mat searchRegion = shot(Rect(450, 60, 572, 462));
		Mat greyscale(462,572,CV_32FC1);
		for (int r=0; r < searchRegion.rows; r++) {
			Vec3b* ptr = searchRegion.ptr<Vec3b>(r);
			float* greyPtr = greyscale.ptr<float>(r);
			for (int c=0; c < searchRegion.cols; c++) {
				if (isWhite(ptr[c])) greyPtr[c] = ptr[c][0];
				else greyPtr[c]=0;
			}
		}
		Mat result;
		//vector<Mat> testplanes;
		matchTemplate(greyscale, goldTemp, result, CV_TM_CCOEFF_NORMED);
		Point* bestLoc = NULL;
		double bestScore = -1;

		for (int r = 0; r < result.rows; r++) {
			float* ptr = result.ptr<float>(r);
			for (int c = 0; c < result.cols; c++) {
				if (ptr[c] > bestScore && ptr[c] > thres) {
					//cout << bestScore << endl;
					int midMatchRow = 60 + r + goldTemp.rows/2;
					int midMatchCol = 450 + c + goldTemp.cols/2;
					bestLoc = new Point(midMatchCol, midMatchRow);
					bestScore = ptr[c];	
				}
			}
		}
		return bestLoc;
	}

	bool ItemFinder::exitMenuSeen(Mat shot) {
		Mat region(shot, Rect(510, 207, 245, 250));
		double score = correlate(region, endmenuTemp);
		//cout << score << endl;
	//	imshow("region", region);
		//waitKey(0);
		//cin.get();
		
		return score > 0.9;
	}

int main3(int argn, char** argv) {
	ItemFinder f;
	Mat shot = imread("diablo9.png");
	Mat temp = imread("diablo_temp.png");
	f.findTemp(shot, temp);
	cout << "started" << (double)clock()/CLOCKS_PER_SEC << endl;
	//f.findGold2(shot);
//	f.nextItem(shot);
	cout << "ended" << (double)clock()/CLOCKS_PER_SEC << endl;
//	Sleep(200000);
	/*Mat shot = imread("invtest.png");
	for (int r=0; r < 5; r++) {
		for (int c=0; c < 10; c++) {
			f.invSpotEmpty(shot,r,c);
		}
	}
	cout << f.invSpotEmpty(shot,0,1) << endl;
	Sleep(1000000);
	f.couldNotSell(shot);*/
	return 0;
}

