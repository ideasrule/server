#include <Windows.h>
#include <assert.h>
#include <iostream>
#include "AutoIt3.h"
#include <Dwmapi.h>
#include <gdiplus.h>
#include <Gdipluspixelformats.h>
#include <time.h>
#include "opencv.hpp"
#include <fstream>
#include <signal.h>
#include "ItemFinder.hpp"
using namespace std;
using namespace cv;

bool flag = false;

	HMODULE autoLib = LoadLibrary("AutoItX3_x64.dll"); 
	typedef long (WINAPI *waitPtr)(LPCWSTR, PCWSTR, long);
	typedef void (WINAPI *handlePtr)(LPCWSTR, LPCWSTR, LPWSTR, int);
	typedef void (WINAPI *activatePtr)(LPCWSTR, LPCWSTR);
	typedef long (WINAPI *resizePtr) (LPCWSTR, LPCWSTR, long, long, long, long);
	typedef long (WINAPI *posPtr) (LPCWSTR, LPCWSTR);
	typedef long (WINAPI *movePtr)(long, long, long);
	typedef void (WINAPI *downPtr) (LPCWSTR);
	typedef void (WINAPI *upPtr) (LPCWSTR);
	typedef long (WINAPI *clickPtr) (LPCWSTR, long, long, long, long nSpeed);
	typedef long (WINAPI *setPtr) (LPCWSTR, long);
	typedef void (WINAPI *sendPtr) (LPCWSTR, long);
	typedef void (WINAPI *mouseWheelPtr) (LPCWSTR szDirection, long nClicks);

	handlePtr WinGetHandle = (handlePtr) GetProcAddress(autoLib, "AU3_WinGetHandle");
	waitPtr WinWaitActive = (waitPtr) GetProcAddress(autoLib, "AU3_WinWaitActive");
	activatePtr WinActivate = (activatePtr) GetProcAddress(autoLib, "AU3_WinActivate");
	resizePtr WinMove = (resizePtr) GetProcAddress(autoLib, "AU3_WinMove");
	posPtr WinGetPosX = (posPtr) GetProcAddress(autoLib, "AU3_WinGetPosX");
	posPtr WinGetPosY = (posPtr) GetProcAddress(autoLib, "AU3_WinGetPosY");
	movePtr MouseMove = (movePtr) GetProcAddress(autoLib, "AU3_MouseMove");
	downPtr MouseDown = (downPtr) GetProcAddress(autoLib, "AU3_MouseDown");
	upPtr MouseUp = (upPtr) GetProcAddress(autoLib, "AU3_MouseUp");
	clickPtr MouseClick = (clickPtr) GetProcAddress(autoLib, "AU3_MouseClick");
	setPtr AutoItSetOption = (setPtr) GetProcAddress(autoLib, "AU3_AutoItSetOption");
	sendPtr Send = (sendPtr) GetProcAddress(autoLib, "AU3_Send");
	mouseWheelPtr MouseWheel = (mouseWheelPtr) GetProcAddress(autoLib, "AU3_MouseWheel");
class Bot {
	//height and width of window, not including menubar or sides
	static const int WIDTH = 1264;
	static const int HEIGHT = 730;
	HDC hDDC; //dev context of Diablo III window
	HDC hCDC; //memory dev context holding screenshot
	HBITMAP oldBitmap; //old bitmap in hCDC, before first screenshot is taken
	HBITMAP shot; //bitmap screenshot associated with hCDC
	HWND winHandle; //handle of Diablo III window
	ULONG_PTR gdiToken; //used to shut down GDI+
	Mat* shotAsMat; //most recent screenshot. This should be used for 
	//all image processing
	Mat healthTemp; //health template, in HSV format
	Mat hatredTemp; //hatred template
	Mat disciplineTemp; //discipline template

	Mat goldTemp; //3-channel floating-point gold template

	//(x,y) of upper-left of where templates correspond to
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

	ItemFinder f;
public:
	bool* isColorInDisc;


	int* colorFreq;
	int numUsed;
	CvKNearest finder;
//public:
	Bot(HWND handle) {
		srand(time(NULL));
		//vector<int> colors;
		colorFreq = (int*) calloc(256*256*256, sizeof(int));
		numUsed = 0;
		isColorInDisc = (bool*) calloc(256*256*256, sizeof(bool));
	//	loadMask("color.txt");
		healthTemp = imread("health_template.png",CV_LOAD_IMAGE_COLOR);
		hatredTemp = imread("hatred_template.png", CV_LOAD_IMAGE_COLOR);
		disciplineTemp = imread("discipline_template2.png", CV_LOAD_IMAGE_COLOR);
		goldTemp = imread("gold.png", CV_LOAD_IMAGE_COLOR);
		goldTemp.convertTo(goldTemp, CV_32FC3);
		cvtColor(healthTemp, healthTemp, CV_BGR2HSV);
		cvtColor(hatredTemp, hatredTemp, CV_BGR2HSV);
		//cvtColor(disciplineTemp, disciplineTemp, CV_BGR2HSV);
		using namespace Gdiplus;
		GdiplusStartupInput gdiplusStartupInput;
		GdiplusStartup(&gdiToken, &gdiplusStartupInput, NULL);
		winHandle = handle;
		
		hDDC = GetDC(handle);
		hCDC = CreateCompatibleDC(hDDC);
		shot = CreateCompatibleBitmap(hDDC, WIDTH, HEIGHT);
		oldBitmap = (HBITMAP) SelectObject(hCDC, shot);
		shotAsMat = new Mat(HEIGHT, WIDTH, CV_8UC3);
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
		int failingLinesLim = 4;
		assert(region.rows == temp.rows && region.cols == temp.cols);
		Mat hsvRegion;
		cvtColor(region, hsvRegion, CV_BGR2HSV);
		
		double percentPastThres = 0;
		double percentPastThresLast = 0;
		int failingLines = 0;
		for (int i = temp.rows - 1; i >= 0; i--) {
			Vec3b* pr = region.ptr<Vec3b>(i);
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
				int freq = colorFreq[winHue*256*256 + winSat*256 + winVal];
				if (freq == 0) numUsed++;
				colorFreq[winHue*256*256 + winSat*256 + winVal]++;
				int hueDiff = abs(winHue - tempHue)%90;
				int satDiff = abs(winSat - tempSat);
				int valDiff = winVal - tempVal;
				if (hueDiff > hueThres || satDiff > satThres || valDiff > valThres) nPastThres++;
				nTot++;
			}
			percentPastThres = (double) nPastThres/nTot;
			if (percentPastThres > 0.5) failingLines++;
			else failingLines = 0;
			if (failingLines == failingLinesLim) {
				//showDetectedLevel(region, i + failingLines - 1);
				//uncomment previous line to show result
				//return 1 - (double) (i + failingLines - 1)/temp.rows;
			}
			
		}
		cout<<numUsed<<endl;
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

	double getDiscipline(Mat shot) {
		Mat region(shot, Rect(DISCIPLINE_X, DISCIPLINE_Y, 
				disciplineTemp.cols, disciplineTemp.rows));
		for (int i = disciplineTemp.rows - 1; i >= 0; i--) {
			Vec3b* pr = region.ptr<Vec3b>(i);
			Vec3b* pt = disciplineTemp.ptr<Vec3b>(i);
			int nPastThres = 0;
			int nTot = 0;
			for (int j = 0; j < disciplineTemp.cols; j++) {
				int tempHue = pt[j][0];
				int tempSat = pt[j][1];
				int tempVal = pt[j][2];
				if (tempHue == 0 && tempSat == 0 && tempVal == 0) continue;
				
				int winB = pr[j][0];
				int winG = pr[j][1];
				int winR = pr[j][2];
				
				if (!isColorInDisc[winR*256*256 + winG*256 + winB]) 
					nPastThres++;
				nTot++;
			}
			if ((double) nPastThres/nTot > 0.5) {
				//showDetectedLevel(region, i);
				return 1 - (double) i/disciplineTemp.rows;
			}
			
		}
		return 1;
	}

	void saveSnapshot(string filename, int sleepTime) {
		/*Calls updateShot(), after sleepTime milliseconds, and saves the
		resulting snapshot as screenshot.png */
		Sleep(sleepTime);
		updateShot();
		//stringstream filename;
		imwrite(filename, *shotAsMat);
	}
	void updateShot() {
		/*Takes a screenshot and saves it into shotAsMat*/
		using namespace Gdiplus;
					cout << "end sleep1" << endl;
			cout << getTime() << endl;
		SelectObject(hCDC, shot);
					cout << "end sleep2" << endl;
			cout << getTime() << endl;
		BitBlt(hCDC, 0, 0, WIDTH, HEIGHT, hDDC, 0, 0, SRCCOPY);
					cout << "end sleep3" << endl;
			cout << getTime() << endl;
		SelectObject(hCDC, oldBitmap);
					cout << "end sleep4" << endl;
			cout << getTime() << endl;
		Bitmap shotBitmap(shot, NULL);
					cout << "end sleep5" << endl;
			cout << getTime() << endl;
		BitmapData source;
					cout << "end sleep6" << endl;
			cout << getTime() << endl;
		Gdiplus::Rect bounds(0, 0, shotBitmap.GetWidth(), shotBitmap.GetHeight());
					cout << "end sleep7" << endl;
			cout << getTime() << endl;
		Status status = shotBitmap.LockBits(&bounds, ImageLockModeRead, PixelFormat24bppRGB, &source);
					cout << "end sleep8" << endl;
			cout << getTime() << endl;
		if (status != Ok) {
			cout << "error locking bitmap bits!" << endl;
			return;
		}
		BYTE* dest = (BYTE*) shotAsMat->data;
					cout << "end sleep9" << endl;
			cout << getTime() << endl;
		for (int y = 0; y < source.Height; y++) {
			BYTE* src = (BYTE*) source.Scan0 + y * source.Stride;
			BYTE* dst = (BYTE*) (dest + y * shotAsMat->step);
			memcpy(dst, src, 3*source.Width);
		}
					cout << "end sleepA" << endl;
			cout << getTime() << endl;
		shotBitmap.UnlockBits(&source);
					cout << "end sleepB" << endl;
			cout << getTime() << endl;
	}



	vector<Point> findGold(Mat shot, float thres = 0.6) {
		Mat floatingPoint;
		shot.convertTo(floatingPoint, CV_32FC3);
		Mat result;
		matchTemplate(floatingPoint, goldTemp, result, CV_TM_CCOEFF_NORMED);
		imwrite("resultgoldpos.png", 255*result);
		vector<Point> goldLocs;
		for (int r = 0; r < HEIGHT; r++) {
			float* ptr = result.ptr<float>(r);
			for (int c = 0; c < WIDTH; c++) {
				if (ptr[c] > thres) goldLocs.push_back(Point2i(r, c));
			}
		}
		return goldLocs;
	}

	int randInt(int lower, int upper) {
		int range = upper - lower + 1;
		return rand()%range + lower;
	}

	void moveAndClick(int leftx, int topy, int rightx, int boty, 
		double holdMin, double holdMax, LPCWSTR button = L"left") {
		int x = randInt(leftx, rightx);
		int y = randInt(topy, boty);
		int moveSpeed = randInt(9, 13);
		MouseClick(button, x, y, 1, moveSpeed);
		int waitTime = randInt(holdMin, holdMax);
		Sleep(waitTime);
	}

	void moveAndHold(int leftx, int topy, int rightx, int boty, 
		double holdMin, double holdMax, bool mouseUp = true, 
		LPCWSTR button = L"left") {
			int x = randInt(leftx, rightx);
			int y = randInt(topy, boty);
			int moveSpeed = randInt(10, 15);
			MouseMove(x, y, moveSpeed);
			MouseDown(button);
			int waitTime = randInt(holdMin, holdMax);
			Sleep(waitTime);
			if (mouseUp) MouseUp(button);
	}

	bool SpiralSearch(int x0, int y0, double b = 2) {
		//true on successful find of box, false otherwise
		for (double theta = 0; theta < 3*2*CV_PI; theta += 0.2) {
			double r = b*theta;
			double x = x0 + r*cos(theta);
			double y = y0 + r*sin(theta);
			int speed = randInt(4, 8);
			MouseMove(x, y, speed);
			updateShot();
			int regionLeftX = x - 160;
			if (regionLeftX < 0) regionLeftX = 0;
			int regionRightX = x + 160;
			if (regionRightX >= WIDTH) regionRightX = WIDTH - 1;
			int regionTopY = y - 65; 
			if (regionTopY < 0) regionTopY = 0;
			Mat region(*shotAsMat, Rect(regionLeftX, regionTopY, 
				regionRightX - regionLeftX + 1, y - regionTopY + 1));
			if (f.boxExists(region)) {
				//MouseUp(L"left");
			//	cout <<"found in spiral"<<endl;
				MouseClick(L"left", x, y, 1, 0);
				Sleep(5000);
				return true;
			}
		}
		return false;
	}

	bool goToSupplyChest() {
		//return true if successful, false if not
		//ItemFinder f;
		int x = randInt(59, 69);
		int y = randInt(240, 249);
		int moveSpeed = randInt(10, 15);
		MouseMove(x, y, moveSpeed);
		MouseDown(L"left");
		
		double startTime = (double) clock()/CLOCKS_PER_SEC;
		cout << "startime is" << startTime << endl;
		//bool found = true;
		while((double) clock()/CLOCKS_PER_SEC - startTime < 4) {
		//	cout << clock() << endl;
			updateShot();
			Mat region(*shotAsMat, Rect(0, 140, 220, 100));
			if (f.boxExists(region)) {
		//		cout <<"box exists" << endl;
				MouseUp(L"left");
				MouseClick(L"left", x, y, 1, 1);
				Sleep(3000);
				return true;
			}
		}

		MouseUp(L"left");
		//return false;
		//MouseUp(
		return SpiralSearch(177, 250);
		//moveAndHold(59, 240, 69, 249, 500, 700);
		//return false;
	}

	double getTime() {
		return (double) clock()/CLOCKS_PER_SEC;
	}

bool recover() {
		//true on success, false on failure
		updateShot();
		Send(L"{ESC down}", 0);
		Sleep(randInt(200, 300));
		Send(L"{ESC up}", 0);
		Sleep(randInt(1000, 1600));
		updateShot();
		if (!f.exitMenuSeen(*shotAsMat)) exit(-1);
		moveAndClick(623, 386, 689, 397, 4500, 6000); //click "leave game"
		moveAndClick(95, 320, 236, 336, 200, 250); //change quest
		//try to exit and recover
		selectQuest(22, 2);
	}


	void killGhom() {
		ItemFinder f;
		moveAndClick(95, 320, 236, 336, 200, 250); //change quest
		moveAndClick(291, 438, 424, 442, 200, 250); //breached keep
		moveAndClick(280, 517, 384, 520, 200, 250); //kill ghom

		moveAndClick(845, 586, 845, 586, 200, 250); //click "Select Quest"

		moveAndClick(533, 423, 580, 431, 300, 350); //click OK
		moveAndClick(179, 278, 233, 289, 8000, 9000); //click "Start Game"

		moveAndClick(418, 349, 445, 368, 1300, 1420); //click on waypoint
		moveAndClick(122, 291, 222, 299, 2200, 2500); //go to keep 3

		moveAndClick(1062, 154, 1082, 197, 2000, 2500); //enter larder
		moveAndHold(918, 130, 923, 145, 7000, 9000); //walk in larder
		//click escape
		Send(L"{SPACE down}", 0);
		Sleep(randInt(50, 100));
		Send(L"{SPACE up}", 0);
		moveAndClick(541, 245, 584, 256, 1000, 1900); //click "OK"
		moveAndHold(913, 133, 934, 152, 0, 0, false, L"right"); //right click on Ghom
		double endTime = getTime() + 15;
		//cout << "Start time: " << startTime << endl;
		while (getTime() < endTime) {
			bool finished = false;
			MouseDown(L"right");
			while(!f.hatredDepleted(*shotAsMat) && 
				getTime() < endTime) {
				updateShot();
				if (f.questEnded(*shotAsMat)) {
					finished = true;
					break;
				}
			}
			Sleep(randInt(50, 100));
			MouseUp(L"right");
			if (finished) break;
			//cout << getTime() << endl;
			double leftClickEndTime = getTime() + randInt(60, 100)/10.0;
			leftClickEndTime = min(leftClickEndTime, endTime);
			//double leftClickTime = (double) randInt(60, 100)/10;
			Send(L"{LSHIFT down}", 0);
			Sleep(randInt(50,100));
			moveAndHold(738, 223, 761, 242, 0, 0, false);
			while(getTime() < leftClickEndTime) {
				updateShot();
				if (f.questEnded(*shotAsMat)) {
					finished = true;
					break;
				}
			}
			MouseUp(L"left");
			if (finished) break;
			Sleep(randInt(50,100));
		}
		//if (!f.questEnded(*shotAsMat)) exit(-1);
		Send(L"{LSHIFT up}", 0);
		updateShot();
		if (!f.questEnded(*shotAsMat)) recover(); return;
		moveAndClick(646, 620, 704, 631, 200, 300);
		//don't fool around picking things up for more than 10 seconds
		double startPickTime = getTime();
		Point* p;
		int n=0;
		
		do {
			updateShot();
			//stringstream ss;
			//ss << "itemsave" << n << ".png";
			//n++;
			//imwrite("items.png", *shotAsMat);
			//exit(-1);
			p = f.nextItem(*shotAsMat);
			if (p != NULL) {
				int leftx = p->x - 6;
				int topy = p->y - 3;
				assert(leftx > 0 && topy > 0);
				moveAndClick(leftx, topy, leftx + 12, topy + 3, 300, 350);
			}
		} while(p != NULL && getTime() - startPickTime < 10);
		//now pick up gold
		vector<Point> goldLocs;
		do {
			updateShot();
			goldLocs = f.findGold(*shotAsMat);
			if (goldLocs.size() != 0) {
				Point p = goldLocs[0];
				int leftx = p.x - 7;
				int topy = p.y - 4;
				assert(leftx > 0 && topy > 0);
				moveAndClick(leftx, topy, leftx + 14, topy + 8, 100, 250);
			}
		} while(goldLocs.size() != 0 &&
			getTime() - startPickTime < 10);
		Send(L"{t down}", 0);
		Sleep(randInt(100, 160));
		Send(L"{t up}", 0);
		Sleep(randInt(8500, 9500));
		bool success = goToSupplyChest();
		if (!success) recover(); return;
		moveAndClick(457, 299, 471, 315, 700, 800);
		//sell items
		Sleep(randInt(600,1000));
		updateShot();
		//imwrite("inv3.png", *shotAsMat);
		//exit(-1);
		for (int r = 0; r < 1; r++) {
			for (int c = 0; c < 10; c++) {
				if (r==0 && c==0) continue;
				if (!f.invSpotEmpty(*shotAsMat, r, c)) {
					//updateShot();
					int leftx = 921 + 32.78*c + 5;
					int topy = 397 + 32.4*r + 5;
					moveAndClick(leftx, topy, leftx + 20, topy + 20,
						100, 200, L"right");
				}
			}
		}
		Send(L"{ESC down}", 0);
		Sleep(randInt(150, 200));
		Send(L"{ESC up}", 0);
		updateShot();
		if (!f.exitMenuSeen(*shotAsMat)) {
			//otherwise, something went wrong
			Send(L"{ESC down}", 0);
			Sleep(randInt(50, 100));
			Send(L"{ESC up}", 0);
		}
		else recover(); return;
		moveAndClick(623, 386, 689, 397, 4500, 6000); //click "leave game"
		//Sleep(2000);

	}



	string itoa(int integer){
		stringstream ss;
		ss << integer;
		return ss.str();
	}


	bool checkMouseWheel(Rect rectangle, LPCWSTR direction, int clicks){
		if (clicks ==0){
			return true;	
		}
		Sleep(50);
		updateShot();
		Mat regionBefore(*shotAsMat,rectangle);
		regionBefore = regionBefore.clone();
		MouseWheel(direction, clicks);
		Sleep(randInt(50,150));
		updateShot();
		Mat regionAfter(*shotAsMat,rectangle);
		vector<Point> matches = f.findTemp(regionBefore, regionAfter);
		assert ((matches.size() == 1) || (matches.size() == 0)); 
		if (matches.size() == 1){
			return false;
		}
		else {
			return true;
		}
	}


	void scrollQuestMenu(LPCWSTR direction, long clicks){
		int questRegionX = 235;
		int questRegionY = 176;
		int questRegionXSize = 218;
		int questRegionYSize = 347;
		Rect questRegionRect = Rect(questRegionX, questRegionY, questRegionXSize, questRegionYSize);
		bool scrolled = checkMouseWheel(questRegionRect, direction, clicks);
		if (!scrolled){
			moveAndClick(530, 511, 540, 520, 300,800 );
			scrolled = checkMouseWheel(questRegionRect, direction, clicks);
			assert (scrolled);
		}
	}


	bool selectQuest(int quest_number, int subquest_number){
		cout << "select quest started" << endl;
		cout << getTime() << endl;
		int questRegionX = 235;
		int questRegionY = 176;
		int questRegionXSize = 218;
		int questRegionYSize = 347;
		Rect questRegionRect = Rect(questRegionX, questRegionY, questRegionXSize, questRegionYSize);
		vector<Point> desired_matches;
		bool quest_found = false;
		vector<Mat> quests;
		string directory_name = "quests";
		Mat bullet_point = imread (directory_name + "/bullet_point.png");
		Mat trophy = imread (directory_name+ "/trophy.png");
		imshow("a " ,trophy);
		waitKey(0);
		for (int i =1; i <=10; i++){
			string quest_name = directory_name + "/1_" + itoa(i) + "_*";
			WIN32_FIND_DATA file;
			HANDLE filehandle = FindFirstFile(quest_name.c_str(), &file);
			quests.push_back( imread (directory_name + "/" +file.cFileName));
		}
		for (int i =1; i <=10; i++){
			string quest_name = directory_name + "/2_" + itoa(i) + "_*";
			WIN32_FIND_DATA file;
			HANDLE filehandle = FindFirstFile(quest_name.c_str(), &file);
			quests.push_back(imread (directory_name + "/" +file.cFileName));
		}	
		for (int i =1; i <=7; i++){
			string quest_name = directory_name + "/3_" + itoa(i) + "_*";
			WIN32_FIND_DATA file;
			HANDLE filehandle = FindFirstFile(quest_name.c_str(), &file);
			quests.push_back(imread (directory_name + "/" +file.cFileName));
		}
		for (int i =1; i <=4; i++){
			string quest_name = directory_name + "/4_" + itoa(i) + "_*";
			WIN32_FIND_DATA file;
			HANDLE filehandle = FindFirstFile(quest_name.c_str(), &file);
			quests.push_back(imread (directory_name + "/" +file.cFileName));
		}
		cout << "loaded" << endl;
		cout << getTime() << endl;
		while (!quest_found){
									cout << "loop iteration" << endl;
			cout << getTime() << endl;
						Mat questRegion(*shotAsMat,questRegionRect);

			Sleep(randInt(200,800));
			cout << "end sleep" << endl;
			cout << getTime() << endl;
			updateShot();
						cout << "end update" << endl;
			cout << getTime() << endl;
			desired_matches = f.findTemp(questRegion, quests[quest_number]);
			cout << "first match" << endl;
			cout << getTime() << endl;
			if (desired_matches.size() >= 1){
				quest_found = true;
				break;
			}
			bool match_found =false;
			vector<Point> matches;
			int i;
			for(i = 3; match_found==false && i<quests.size(); i+=3){
				matches = f.findTemp(questRegion, quests[i]);
				cout << "loop 2" << endl;
				cout << getTime() << endl;
				if (matches.size() >= 1){

					match_found = true;
					cout <<"true" << i <<endl;
					break;
				}
			}
			assert (match_found);
			assert (matches.size()==1);
			int relative_loc = quest_number - i;
			int moves = relative_loc + randInt(-3,3);
			if (moves < 0){
				scrollQuestMenu(L"up", abs(moves));
			}
			else{
				scrollQuestMenu(L"down", abs(moves));
			}
		}
		assert(desired_matches.size() ==1);
		if (subquest_number ==0){
			MouseClick(L"left", questRegionX+desired_matches[0].x, questRegionY+desired_matches[0].y, 1, randInt(1,9));
		}
		else if (subquest_number >0 && quest_number <quests.size()-1){
			Sleep(randInt(50,200));
			updateShot();
			Mat questRegion(*shotAsMat, questRegionRect);
			vector<Point> next_quest_matches = f.findTemp(questRegion, quests[quest_number+1]);
			assert (next_quest_matches.size()<=1);
			if (next_quest_matches.size()==0){
				scrollQuestMenu(L"down", randInt(2,4));
				Sleep(randInt(50,100));
				updateShot();
				questRegion = Mat(*shotAsMat,Rect(235,176,218,347));
				next_quest_matches = f.findTemp(questRegion, quests[quest_number+1]);
				assert (next_quest_matches.size()==1);
			}
			vector<Point> desired_quest_matches = f.findTemp(questRegion, quests[quest_number]);
			assert(desired_quest_matches.size()==1);
			if ((next_quest_matches[0].y - desired_quest_matches[0].y) <18){
				cout<< "erorroror";	
				Sleep(222222);
				exit(1);
			}
			else{
				int subquest_x = questRegionX;
				int subquest_y = questRegionY+desired_quest_matches[0].y;
				int subquest_x_size = 35;
				int subquest_y_size = next_quest_matches[0].y - desired_quest_matches[0].y;
				Sleep(randInt(50,100));
				updateShot();
				Mat subquest_region(*shotAsMat, Rect(subquest_x, subquest_y, subquest_x_size, subquest_y_size));
				vector<Point> bullet_point_matches = f.findTemp(subquest_region, bullet_point);
				bullet_point_matches = f.segregateY(bullet_point_matches);
				vector<Point> trophyMatches = f.findTemp(subquest_region, trophy);
				trophyMatches = f.segregateY(trophyMatches);
				cout <<trophyMatches.size();
				assert (trophyMatches.size() <=1);
				if (bullet_point_matches.size()==0 && trophyMatches.size() ==0){
					MouseClick(L"left", questRegionX+desired_quest_matches[0].x, questRegionY+desired_quest_matches[0].y, 1, 9);

					Sleep(randInt(25,100));
					updateShot();
					questRegion = Mat(*shotAsMat,questRegionRect);
					
					next_quest_matches = f.findTemp(questRegion, quests[quest_number+1]);
					if (next_quest_matches.size()==0){
						scrollQuestMenu(L"down", randInt(2,4));
					}

					Sleep(randInt(50,100));
					updateShot();
					questRegion = Mat(*shotAsMat,questRegionRect);
					
					next_quest_matches = f.findTemp(questRegion, quests[quest_number+1]);
					assert (next_quest_matches.size()==1);
					desired_quest_matches = f.findTemp(questRegion, quests[quest_number]);
					assert(desired_quest_matches.size()==1);
					subquest_y = questRegionY+desired_quest_matches[0].y;
					subquest_y_size = next_quest_matches[0].y - desired_quest_matches[0].y;
					subquest_region=Mat(*shotAsMat, Rect(subquest_x, subquest_y, subquest_x_size, subquest_y_size));
					bullet_point_matches = f.findTemp(subquest_region, bullet_point);
					bullet_point_matches = f.segregateY(bullet_point_matches);
					trophyMatches = f.findTemp(subquest_region, trophy);
					trophyMatches = f.segregateY(trophyMatches);
					assert (trophyMatches.size() <=1);
				}
				if (trophyMatches.size() == 1 && bullet_point_matches.size() == 0 ){
					bullet_point_matches = trophyMatches;
				}
				else if (trophyMatches.size() == 1){
					bool inserted = false;
					for (int i =0; i<bullet_point_matches.size(); i++){
						if (trophyMatches[0].y < bullet_point_matches[i].y){
							vector<Point>::iterator bulletPointMatchesBegin = bullet_point_matches.begin();
							bullet_point_matches.insert(bulletPointMatchesBegin+i,trophyMatches[0]);
							inserted = true;
							break;
						}
					}
					if (inserted == false){
						bullet_point_matches.push_back(trophyMatches[0]);
					}
				}
				if (bullet_point_matches.size() < subquest_number+1){
					cout <<bullet_point_matches.size();
					cout << subquest_number+1;
					cout<< "not enough lumber";
					Sleep(222222);
					exit(2);
				}
				MouseClick(L"left", subquest_x+bullet_point_matches[subquest_number].x+30, subquest_y+bullet_point_matches[subquest_number].y, 1, 9);
			}
		}
		else if(subquest_number >0 && quest_number ==quests.size()-1){
			Sleep(randInt(50,150));
			updateShot();
			Mat questRegion(*shotAsMat,questRegionRect);
			Mat subquest_region;
			vector<Point> desired_quest_matches = f.findTemp(questRegion, quests[quest_number]);
			cout << desired_quest_matches.size();
			assert(desired_quest_matches.size()==1);
			int subquest_x = questRegionX;
			int subquest_y = questRegionY+desired_quest_matches[0].y;
			int subquest_x_size = 35;
			int subquest_y_size = questRegionYSize - desired_quest_matches[0].y;
			vector<Point> bullet_point_matches;
			vector<Point> trophyMatches;
			if (subquest_y_size < 18){
				MouseClick(L"left", 235+desired_matches[0].x, 176+desired_matches[0].y, 1, 9);
			}
			else{
				subquest_region =Mat(*shotAsMat, Rect(subquest_x, subquest_y, subquest_x_size, subquest_y_size));
				bullet_point_matches = f.findTemp(subquest_region, bullet_point);
				bullet_point_matches = f.segregateY(bullet_point_matches);;
				trophyMatches = f.findTemp(subquest_region, trophy);
				trophyMatches = f.segregateY(trophyMatches);
				assert (trophyMatches.size() <=1);
				if (bullet_point_matches.size() ==0 && trophyMatches.size() ==0){
					MouseClick(L"left", questRegionX+desired_matches[0].x, questRegionY+desired_matches[0].y, 1, 9);
				}
			}
			Sleep(randInt(50,150));
			updateShot();
			questRegion= Mat(*shotAsMat,questRegionRect);
			desired_quest_matches = f.findTemp(questRegion, quests[quest_number]);
			cout << desired_quest_matches.size();
			assert(desired_quest_matches.size()==1);
			subquest_x = 235;
			subquest_y = 176+desired_quest_matches[0].y;
			subquest_x_size = 35;
			subquest_y_size = 347 - desired_quest_matches[0].y;
			subquest_region = Mat(*shotAsMat, Rect(subquest_x, subquest_y, subquest_x_size, subquest_y_size));
			bullet_point_matches = f.findTemp(subquest_region, bullet_point);
			bullet_point_matches = f.segregateY(bullet_point_matches);
			
			trophyMatches = f.findTemp(subquest_region, trophy);
			trophyMatches = f.segregateY(trophyMatches);
			assert (trophyMatches.size() <=1);
			if (trophyMatches.size() == 1 && bullet_point_matches.size() == 0 ){
				bullet_point_matches = trophyMatches;
			}
			else if (trophyMatches.size() == 1){
				bool inserted = false;
				for (int i =0; i<bullet_point_matches.size(); i++){
					if (trophyMatches[0].y < bullet_point_matches[i].y){
						vector<Point>::iterator bulletPointMatchesBegin = bullet_point_matches.begin();
						bullet_point_matches.insert(bulletPointMatchesBegin+i,trophyMatches[0]);
						inserted = true;
						break;
					}
				}
				if (inserted == false){
					bullet_point_matches.push_back(trophyMatches[0]);
				}
			}
			assert (bullet_point_matches.size() >=1);
			if (bullet_point_matches.size() < subquest_number+1){
				cout <<bullet_point_matches.size();
				cout << subquest_number+1;
				cout<< "not enough lumber";
				Sleep(222222);
				exit(2);
			}
			MouseClick(L"left", subquest_x+bullet_point_matches[subquest_number].x+30, subquest_y+bullet_point_matches[subquest_number].y, 1, 9);
		}
		else{
			cout << quest_number << " is invalid";
		}
		return true;
	}


	
	~Bot() {
		ReleaseDC(winHandle, hDDC);
		DeleteDC(hCDC);
		Gdiplus::GdiplusShutdown(gdiToken);
		delete shotAsMat;
	}
};


int main(int argn, char** argv)
{
	//Sleep(2000);
	//MouseWheel(L"up", 20);
	//return 0;
	LPCWSTR title = L"Diablo III";
	AutoItSetOption(L"MouseCoordMode", 2);

	WinActivate(title, L"");
	Sleep(200);
	WinWaitActive(title, L"", 0);
	//have to get window position so that WinMove only rescales, instead of move + rescale
	long x = WinGetPosX(title, L"");
	long y = WinGetPosY(title, L"");

	WinMove(title, L"", x, y, 1280, 768);
	
	WCHAR handleArr[33];
	WinGetHandle(L"Diablo III", L"", handleArr, 32);
	HWND handle = (HWND) wcstol(handleArr, NULL, 16);
	cout<<"starting bot"<<endl;
	Bot myBot(handle);
	//myBot.updateShot();
	//myBot.saveSnapshot("mytest.png", 2000);
	//return 0;
	//for (int i=0; i < 10; i++) {
	//	myBot.killGhom();
	//}
	//myBot.updateShot();
	//return 0;
	//bool m = myBot.checkMouseWheel(Rect(235,176,218,347), L"down", 14);
	//cout << m <<endl;
	myBot.selectQuest(30,1);
	//Sleep (5000);
	//myBot.selectQuest(17);
	//Sleep (5000);
	//myBot.selectQuest(1);
	//Sleep (5000);
	//myBot.selectQuest(29);
	//cout <<"2222222222222222222222222222222222222222222222222222222222222222222222222222222222222";
	//Sleep(100000);
	//return 0;
	//int startTime2 = time(NULL);
	//myBot.saveSnapshot("itemtest2.png", 2000);
	//for (int i=0; i < 10; i++) {
	//	cout << "run number " << i << endl;
	//	myBot.killGhom();
	//}
	//int endTime2 = time(NULL);
	//cout << endTime2 - startTime2 << endl;
	//return 0;
	//myBot.saveSnapshot("trophyquest.png", 200);
	return 0;
}


