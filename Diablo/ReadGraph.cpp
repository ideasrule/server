#include "opencv.hpp"
using namespace cv;
using namespace std;
double xToFreq(int x) {
	return (x - 215)/10.35 + 130;
	//return (x - 189)/9.733333 + 130;
}
double yToFrac(int y) {
	//return (547 - y)/395.83;
	return (473 - y)/344.166;
	//60,473
}
int main4(int argn, char** argv) {
	Mat graph = imread(argv[1]);
	vector<int> xVals;
	vector<int> yVals;
	//Vec3b prevVal = 0;
	for (int c = 0; c < graph.cols; c++) {
		bool flag = false;
		for (int r = graph.rows - 1; r >= 0; r--) {
			Vec3b val = graph.at<Vec3b>(r, c);
			if (val[0]==255 && val[1] < 60 && val[2] < 60) {
				xVals.push_back(c);
				yVals.push_back(r);
				flag = true;
				break;
			}
		}
		if (!flag && yVals.size() != 0) {
			//cout << yVals.back() << endl;
			xVals.push_back(c);

			yVals.push_back(yVals.back());
		}
	}
	cout << "Freqs: " << endl;
	for (int i = 0; i < xVals.size(); i++) {
		cout << xToFreq(xVals[i]) << endl;
	}
	cout << "Vals: " << endl;
	for (int i = 0; i < yVals.size(); i++) {
		cout << yToFrac(yVals[i]) << endl;
	}
	return 0;
}