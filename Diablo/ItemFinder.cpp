#include "ItemFinder.hpp"
#include "opencv.hpp"
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
		if (diffHue <= 1 && diffSat <= 5 && diffVal <= 4) return true;
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
		goldTemp = imread("gold.png", CV_LOAD_IMAGE_COLOR);
		goldTemp = applyLim(goldTemp, isWhite);
		goldTemp.convertTo(goldTemp, CV_32FC3);

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
		cout << score;
		if (score > 0.8) return true;
		return false;
	}

	void ItemFinder::checkInv(Mat shot) {
		//Mat test = imread("inv.png", CV_LOAD_IMAGE_COLOR);
		//Mat temp(test, Rect(921, 397, 330, 196));
		int leftX = 921;
		int topY = 397;

		for (int r = 0; r < 6; r++) {
			for (int c = 0; c < 10; c++) {
				Mat refRegion(invTemp, Rect(32.78*c, 32.4*r, 31, 31));

				Mat region(shot, Rect(leftX + 32.78*c, topY + 32.4*r, 31, 31));
				double score = correlate(region, refRegion);
				//cout << score << endl;
			}
		}
		//invTemp.convertTo(invTemp, CV_32FC3);
		//findTemp(shot, invTemp);
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
	//	cout << score << endl;
		if (score > 0.95) return true;
		return false;
	}

	vector<Point> ItemFinder::findTemp(Mat shot, Mat temp, double thres) {
		Mat floatingPoint;
		shot.convertTo(floatingPoint, CV_32FC3);
		temp.convertTo(temp, CV_32FC3);
		Mat result;
		//cout << "template matching started" << endl;
		matchTemplate(floatingPoint, temp, result, CV_TM_CCOEFF_NORMED);
		//cout << "template matching ended" << endl;
		//imshow("result", result);
		//imwrite("invresult.png", 255*result);
		//waitKey(0);

		//imwrite("resultgoldpos2.png", 255*result);
		//cout << shot.rows << " " << shot.cols<<endl;
		//cout << temp.rows << " " << temp.cols<<endl;
		//cout << result.rows << " " << result.cols<<endl;
		vector<Point> locs;
		for (int r = 0; r < result.rows; r++) {
			float* ptr = result.ptr<float>(r);
			for (int c = 0; c < result.cols; c++) {
				//cout << ptr[c] << endl;
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
	//	imwrite("distances2.png", distances);
		//imshow("distances", binary);
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
			if (count > 60 && count > bestCount) {
				bestCount = count;
				int col = pixelLocs.size()/2;
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
		//filtered.convertTo(filtered, CV_32FC3);
		//imshow("filtered", filtered);
		//imshow("template", hateDepletedTemp);
		//waitKey(0);
		double score = correlate(filtered, hateDepletedTemp);
		//cout << score << enl;
		//return false;
		return (score > 0.8);
	}
	vector<Point> ItemFinder::findGold(Mat shot, double thres) {
		Mat filtered = applyLim(shot, isWhite);
		return findTemp(shot, goldTemp, 0.7);
	}
	bool ItemFinder::exitMenuSeen(Mat shot) {
		Mat region(shot, Rect(510, 207, 245, 250));
		double score = correlate(region, endmenuTemp);
		return score > 0.95;
	}

	bool isSeen(Mat shot, int what) {
		Mat region;
		Mat temp;
		switch(what) {
		case 0:
			temp = imread("changequest.png", CV_LOAD_IMAGE_COLOR);
			region = shot(Rect(417, 314, temp.cols, temp.rows));
			break;
		case 1:
			temp = imread("questselect.png", CV_LOAD_IMAGE_COLOR);
			region = shot(Rect(229, 172, temp.cols, temp.rows));
			break;

		}
		return false;
	}

int main3(int argn, char** argv) {
	ItemFinder finder;
	
	Mat testim = imread("items.png", CV_LOAD_IMAGE_COLOR);
	/*for (int r = 0; r < 6; r++) {
		for (int c = 0; c < 10; c++) {
			finder.invSpotEmpty(testim, r, c);
			cout << " ";
		}
		cout << endl;
	}*/
	Point* p = finder.nextItem(testim);
	if (p != NULL) {
		cout << p->x << " " << p->y << endl;
		cout << p << endl;
	}
	//finder.hatredDepleted(testim);
	//finder.checkInv(testim);
	//Mat finishRegion(testim, Rect(492, 507, 277, 136));
	//finder.questEnded(testim);
	//return 0;
	/*
	Mat im = imread("captainbox2.png", CV_LOAD_IMAGE_COLOR);
	finder.findTemp(im, finder.boxTemp);
	return 0;
	//cvtColor(im, im, CV_BGR2HSV);
	Mat blue = finder.applyLim(im, finder.isBlueOrYellow);
	imshow("items", blue);
	waitKey(0);
	return 0;

	for (int r = 0; r < blue.rows; r++) {
		int count = 0;
		Vec3b* ptr = blue.ptr<Vec3b>(r);
		vector<int> bluePixelLocs;
		for (int c = 0; c < blue.cols; c++) {
			if (ptr[c][0] != 0 || ptr[c][1] != 0 || ptr[c][2] != 0) {
				bluePixelLocs.push_back(c);
				count++;
			}
		}
		if (count > 40) {
			Point start(bluePixelLocs.front(), r);
			Point end(bluePixelLocs.back(), r);
			line(blue, start, end, Scalar(255, 0, 0), 1);
		}
		//cout << count << " ";
	}
	imshow("result", blue);
	waitKey(0);
	return 0;

	cout << im << endl;
	Mat result = finder.applyLim(im, finder.isWhite);
	imshow("white thres", result);
	waitKey(0);
	cout <<"hello"<<endl;
	return 0;*/
	return 0;
}

