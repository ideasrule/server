#include <opencv.hpp>
using namespace cv;
using namespace std;
class ItemFinder {
private:
	Mat goldTemp;//floating point, 1-channel(blue)
	//to be "white", item must have R,G,B > 150
	Mat endTemp;
	Mat hateDepletedTemp; //HSV, blackened
	Mat boxTemp;
	Mat merchantTemp;
	Mat invTemp;
	Mat endmenuTemp;
	Mat applyLim(Mat original, bool (*isValid)(Vec3b));
	double correlate(Mat im1, Mat im2);

public:
	vector<Point> findTemp(Mat shot, Mat temp, double thres = 0.7);
	const static int WHITE_LIM = 150;
	vector<Point> segregateY(vector<Point> original, int minDist = 2);
	//static bool ItemFinder::isWhite(Vec3b pixel);
	bool isScreenSeen(Mat shot, int what, double thres=0.95);
//public:
	enum{OK_SCREEN, GAME_START_SCREEN, MENU_SCREEN, KEEP0_SCREEN, 
		WAYPOINT_SCREEN, KEEP_LEVEL3_SCREEN, LARDER_SCREEN, END_KEEP_SCREEN,
		EARLY_ABORT_MENU_SCREEN};
	ItemFinder();

	bool questEnded(Mat shot);
	vector<Point> findAllGold(Mat shot);
	Point* findGold(Mat shot, double thres = 0.95);

	bool boxExists(Mat region);
	Point* nextItem(Mat shot, int startRow = 80);
	bool hatredDepleted(Mat shot);
	bool couldNotSell(Mat shot);
	bool invSpotEmpty(Mat shot, int r, int c);
	bool exitMenuSeen(Mat shot);
	//return bestPoint;
};