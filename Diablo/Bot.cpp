#include "Bot.h"
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
	enum{IM_TO_MAP, MAP_TO_IM};
	ItemFinder f;
public:
	bool* isColorInDisc;


	int* colorFreq;
	int numUsed;
	CvKNearest finder;
//public:
	Bot(HWND handle) {
		srand((unsigned int)time(NULL));
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

	void screenshot(string filename, int sleepTime) {
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
		BYTE* dest = (BYTE*) shotAsMat->data;
		for (size_t y = 0; y < source.Height; y++) {
			BYTE* src = (BYTE*) source.Scan0 + y * source.Stride;
			BYTE* dst = (BYTE*) (dest + y * shotAsMat->step);
			memcpy(dst, src, 3*source.Width);
		}
		shotBitmap.UnlockBits(&source);
	}

	int randInt(int lower, int upper) {
		int range = upper - lower + 1;
		return rand()%range + lower;
	}

	void sendKey(string key, int holdMin, int holdMax) {
	//	Send(L"{"+key+L" down}", 0);
		Send(L"{ESC down}", 0);
	}

	void moveAndClick(int leftx, int topy, int rightx, int boty, 
		int holdMin, int holdMax, LPCWSTR button = L"left") {
		int x = randInt(leftx, rightx);
		int y = randInt(topy, boty);
		int moveSpeed = randInt(9, 13);
		MouseClick(button, x, y, 1, moveSpeed);
		int waitTime = randInt(holdMin, holdMax);
		Sleep(waitTime);
	}

	void moveAndHold(int leftx, int topy, int rightx, int boty, 
		int holdMin, int holdMax, bool mouseUp = true, 
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
			int x = (int) (x0 + r*cos(theta));
			int y = (int) (y0 + r*sin(theta));
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
		int x = randInt(59, 69);
		int y = randInt(240, 249);
		int moveSpeed = randInt(10, 15);
		MouseMove(x, y, moveSpeed);
		MouseDown(L"left");
		
		double startTime = (double) clock()/CLOCKS_PER_SEC;
		while((double) clock()/CLOCKS_PER_SEC - startTime < 4) {
			updateShot();
			Mat region(*shotAsMat, Rect(0, 140, 220, 100));
			if (f.boxExists(region)) {
				MouseUp(L"left");
				MouseClick(L"left", x, y, 1, 1);
				Sleep(3000);
				return true;
			}
		}
		MouseUp(L"left");
		return SpiralSearch(700, 400);
	}

	double getTime() {
		return (double) clock()/CLOCKS_PER_SEC;
	}

void recover() {
		cout << "recover called" << endl;
		//true on success, false on failure
		//updateShot();
		Send(L"{ESC down}", 0);
		Sleep(randInt(300, 500));
		Send(L"{ESC up}", 0);
		Sleep(randInt(1000, 1600));
		updateShot();
		if (!f.exitMenuSeen(*shotAsMat)) exit(-1);
		moveAndClick(623, 386, 689, 397, 1500, 1800); //click "leave game"
		waitTillSeen(f.GAME_START_SCREEN);
	}
	
	void waitTillSeen(int what, double thres=0.95) {
		updateShot();
		while(!f.isScreenSeen(*shotAsMat, what, thres)) {
			updateShot();
			Sleep(100);
		}
		Sleep(700);
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
		}
	}

	bool selectQuest(int questNumber, int subquestNumber){
		cout<<"selecting quest"<<endl;
		int questRegionX = 235;
		int questRegionY = 176;
		int questRegionXSize = 218;
		int questRegionYSize = 347;
		Rect questRegionRect = Rect(questRegionX, questRegionY, questRegionXSize, questRegionYSize);
		vector<Point> desiredMatches;
		bool quest_found = false;
		vector<Mat> quests;
		string directoryName = "quests";
		Mat bullet_point = imread (directoryName + "/bullet_point.png");
		Mat trophy = imread (directoryName+ "/trophy.png");
		for (int i =1; i <=10; i++){
			string questName = directoryName + "/1_" + itoa(i) + "_*";
			WIN32_FIND_DATA file;
			HANDLE filehandle = FindFirstFile(questName.c_str(), &file);
			quests.push_back( imread (directoryName + "/" +file.cFileName));
		}
		for (int i =1; i <=10; i++){
			string questName = directoryName + "/2_" + itoa(i) + "_*";
			WIN32_FIND_DATA file;
			HANDLE filehandle = FindFirstFile(questName.c_str(), &file);
			quests.push_back(imread (directoryName + "/" +file.cFileName));
		}	
		for (int i =1; i <=7; i++){
			string questName = directoryName + "/3_" + itoa(i) + "_*";
			WIN32_FIND_DATA file;
			HANDLE filehandle = FindFirstFile(questName.c_str(), &file);
			quests.push_back(imread (directoryName + "/" +file.cFileName));
		}
		for (int i =1; i <=4; i++){
			string questName = directoryName + "/4_" + itoa(i) + "_*";
			WIN32_FIND_DATA file;
			HANDLE filehandle = FindFirstFile(questName.c_str(), &file);
			quests.push_back(imread (directoryName + "/" +file.cFileName));
		}
		while (!quest_found){
			Mat questRegion(*shotAsMat,questRegionRect);
			Sleep(randInt(200,800));
			updateShot();
			desiredMatches = f.findTemp(questRegion, quests[questNumber]);
			if (desiredMatches.size() >= 1){
				quest_found = true;
				break;
			}
			bool match_found =false;
			vector<Point> matches;
			int i;
			for(i = 3; match_found==false && i<quests.size(); i+=3){
				matches = f.findTemp(questRegion, quests[i]);
				if (matches.size() >= 1){
					match_found = true;
					break;
				}
			}
			assert (match_found);
			assert (matches.size()==1);
			int relative_loc = questNumber - i;
			int moves = relative_loc + randInt(-3,3);
			if (moves < 0){
				scrollQuestMenu(L"up", abs(moves));
			}
			else{
				scrollQuestMenu(L"down", abs(moves));
			}
		}
		assert(desiredMatches.size() ==1);
		if (subquestNumber ==0){
			MouseClick(L"left", questRegionX+desiredMatches[0].x, questRegionY+desiredMatches[0].y, 1, randInt(1,9));
		}
		else if (subquestNumber >0 && questNumber <quests.size()-1){
			Sleep(randInt(50,200));
			updateShot();
			Mat questRegion(*shotAsMat, questRegionRect);
			vector<Point> nextQuestMatches = f.findTemp(questRegion, quests[questNumber+1]);
			assert (nextQuestMatches.size()<=1);
			if (nextQuestMatches.size()==0){
				scrollQuestMenu(L"down", randInt(2,4));
				Sleep(randInt(50,100));
				updateShot();
				questRegion = Mat(*shotAsMat,Rect(235,176,218,347));
				nextQuestMatches = f.findTemp(questRegion, quests[questNumber+1]);
				assert (nextQuestMatches.size()==1);
			}
			desiredMatches = f.findTemp(questRegion, quests[questNumber]);
			assert(desiredMatches.size()==1);
			if ((nextQuestMatches[0].y - desiredMatches[0].y) <18){
				cout<< "erorroror";	
				Sleep(222222);
				exit(1);
			}
			else{
				int subquest_x = questRegionX;
				int subquest_y = questRegionY+desiredMatches[0].y;
				int subquest_x_size = 35;
				int subquest_y_size = nextQuestMatches[0].y - desiredMatches[0].y;
				Sleep(randInt(50,100));
				updateShot();
				Mat subquestRegion(*shotAsMat, Rect(subquest_x, subquest_y, subquest_x_size, subquest_y_size));
				vector<Point> subquestMatches = f.findTemp(subquestRegion, bullet_point);
				subquestMatches = f.segregateY(subquestMatches);
				vector<Point> trophyMatches = f.findTemp(subquestRegion, trophy);
				trophyMatches = f.segregateY(trophyMatches);
				assert (trophyMatches.size() <=1);
				if (subquestMatches.size()==0 && trophyMatches.size() ==0){
					MouseClick(L"left", questRegionX+desiredMatches[0].x, questRegionY+desiredMatches[0].y, 1, 9);
					Sleep(randInt(50,100));
					updateShot();
					questRegion = Mat(*shotAsMat,questRegionRect);					
					nextQuestMatches = f.findTemp(questRegion, quests[questNumber+1]);
					if (nextQuestMatches.size()==0){
						scrollQuestMenu(L"down", randInt(2,4));
					}
					Sleep(randInt(50,100));
					updateShot();

					questRegion = Mat(*shotAsMat,questRegionRect);					
					nextQuestMatches = f.findTemp(questRegion, quests[questNumber+1]);
					assert (nextQuestMatches.size()==1);
					desiredMatches = f.findTemp(questRegion, quests[questNumber]);
					assert(desiredMatches.size()==1);
					subquest_y = questRegionY+desiredMatches[0].y;
					subquest_y_size = nextQuestMatches[0].y - desiredMatches[0].y;
					subquestRegion=Mat(*shotAsMat, Rect(subquest_x, subquest_y, subquest_x_size, subquest_y_size));
					subquestMatches = f.findTemp(subquestRegion, bullet_point);
					subquestMatches = f.segregateY(subquestMatches);
					trophyMatches = f.findTemp(subquestRegion, trophy);
					trophyMatches = f.segregateY(trophyMatches);
					assert (trophyMatches.size() <=1);
				}
				if (trophyMatches.size() == 1 && subquestMatches.size() == 0 ){
					subquestMatches = trophyMatches;
				}
				else if (trophyMatches.size() == 1){
					bool inserted = false;
					for (int i =0; i<subquestMatches.size(); i++){
						if (trophyMatches[0].y < subquestMatches[i].y){
							vector<Point>::iterator bulletPointMatchesBegin = subquestMatches.begin();
							subquestMatches.insert(bulletPointMatchesBegin+i,trophyMatches[0]);
							inserted = true;
							break;
						}
					}
					if (inserted == false){
						subquestMatches.push_back(trophyMatches[0]);
					}
				}
				if (subquestMatches.size() < subquestNumber+1){
					cout <<subquestMatches.size();
					cout << subquestNumber+1;
					cout<< "not enough lumber";
					Sleep(222222);
					exit(2);
				}
				MouseClick(L"left", subquest_x+subquestMatches[subquestNumber].x+30, subquest_y+subquestMatches[subquestNumber].y, 1, 9);
			}
		}
		else if(subquestNumber >0 && questNumber ==quests.size()-1){
			Sleep(randInt(50,150));
			updateShot();
			Mat questRegion(*shotAsMat,questRegionRect);
			Mat subquestRegion;
			desiredMatches = f.findTemp(questRegion, quests[questNumber]);
			assert(desiredMatches.size()==1);
			int subquest_x = questRegionX;
			int subquest_y = questRegionY+desiredMatches[0].y;
			int subquest_x_size = 35;
			int subquest_y_size = questRegionYSize - desiredMatches[0].y;
			vector<Point> subquestMatches;
			vector<Point> trophyMatches;
			if (subquest_y_size < 18){
				MouseClick(L"left", 235+desiredMatches[0].x, 176+desiredMatches[0].y, 1, 9);
			}
			else{
				subquestRegion =Mat(*shotAsMat, Rect(subquest_x, subquest_y, subquest_x_size, subquest_y_size));
				subquestMatches = f.findTemp(subquestRegion, bullet_point);
				subquestMatches = f.segregateY(subquestMatches);;
				trophyMatches = f.findTemp(subquestRegion, trophy);
				trophyMatches = f.segregateY(trophyMatches);
				assert (trophyMatches.size() <=1);
				if (subquestMatches.size() ==0 && trophyMatches.size() ==0){
					MouseClick(L"left", questRegionX+desiredMatches[0].x, questRegionY+desiredMatches[0].y, 1, 9);
				}
			}
			Sleep(randInt(50,150));
			updateShot();
			questRegion= Mat(*shotAsMat,questRegionRect);
			desiredMatches = f.findTemp(questRegion, quests[questNumber]);
			assert(desiredMatches.size()==1);
			subquest_x = 235;
			subquest_y = 176+desiredMatches[0].y;
			subquest_x_size = 35;
			subquest_y_size = 347 - desiredMatches[0].y;
			subquestRegion = Mat(*shotAsMat, Rect(subquest_x, subquest_y, subquest_x_size, subquest_y_size));
			subquestMatches = f.findTemp(subquestRegion, bullet_point);
			subquestMatches = f.segregateY(subquestMatches);
			trophyMatches = f.findTemp(subquestRegion, trophy);
			trophyMatches = f.segregateY(trophyMatches);
			assert (trophyMatches.size() <=1);
			if (trophyMatches.size() == 1 && subquestMatches.size() == 0 ){
				subquestMatches = trophyMatches;
			}
			else if (trophyMatches.size() == 1){
				bool inserted = false;
				for (int i =0; i<subquestMatches.size(); i++){
					if (trophyMatches[0].y < subquestMatches[i].y){
						vector<Point>::iterator bulletPointMatchesBegin = subquestMatches.begin();
						subquestMatches.insert(bulletPointMatchesBegin+i,trophyMatches[0]);
						inserted = true;
						break;
					}
				}
				if (inserted == false){
					subquestMatches.push_back(trophyMatches[0]);
				}
			}
			assert (subquestMatches.size() >=1);
			if (subquestMatches.size() < subquestNumber+1){
				cout <<subquestMatches.size();
				cout << subquestNumber+1;
				cout<< "not enough lumber";
				Sleep(222222);
				exit(2);
			}
			MouseClick(L"left", subquest_x+subquestMatches[subquestNumber].x+30, subquest_y+subquestMatches[subquestNumber].y, 1, 9);
		}
		else{
			cout << questNumber << " is invalid";
		}
		return true;
	}


	
	~Bot() {
		ReleaseDC(winHandle, hDDC);
		DeleteDC(hCDC);
		Gdiplus::GdiplusShutdown(gdiToken);
		delete shotAsMat;
	}
	Vec2f convertPoint(float x, float y, int direction=IM_TO_MAP) {
		Mat camera = (Mat_<float>(3,3) <<1079,0,631.5,0,1079,364.5,0,0,1);
		Mat RT = (Mat_<float>(3,3) <<1,0,0,0,cos(0.8),0,0,-sin(0.8),58);
		Mat transform;
		if (direction==IM_TO_MAP)
			transform = (camera*RT).inv();
		else if (direction==MAP_TO_IM)
			transform = camera*RT;
		else assert(false);
		Mat p = (Mat_<float>(3,1) << x,y,1);
		Mat transPoint = transform*p;
		float s = transPoint.at<float>(2,0);
		float resultX = transPoint.at<float>(0,0)/s;
		float resultY = transPoint.at<float>(1,0)/s;
		return Vec2f(resultX, resultY);
	}
	
	double getDist(float x, float y) {
		Vec2f mapPoint = convertPoint(x, y, IM_TO_MAP);
		return sqrt(mapPoint[0]*mapPoint[0] + mapPoint[1]*mapPoint[1]);
	}
	
	void identifyRare(int x, int y) {
		Send(L"{ESC down}", 0);
		Sleep(randInt(150, 200));
		Send(L"{ESC up}", 0);

		Send(L"{i down}", 0);
		Sleep(randInt(150, 200));
		Send(L"{i up}", 0);
		moveAndClick(x, y, x, y, 3500, 4500, L"right");
		Send(L"{ESC down}", 0);
		Sleep(randInt(150, 200));
		Send(L"{ESC up}", 0);
		moveAndClick(525,275,547,318,300,500);
	}

	void collectGold(vector<Point> locs) {
		//get map coordinates for gold
		vector<Vec2f> mapLocs;
		for (int i=0; i < locs.size(); i++) {
			mapLocs.push_back(
				convertPoint(locs[i].x-14, locs[i].y+11, IM_TO_MAP));
		}
		//initialize position to 0
		int xpos = 0;
		int ypos = 0;
		//while gold locs isn't empty:
		while(!mapLocs.empty()) {
			//pop 1 from locs
			Vec2f newGold = mapLocs.back();
			mapLocs.pop_back();
			//predict position on image based on own pos, go there
			Vec2f imPoint = convertPoint(newGold[0]-xpos, newGold[1]-ypos, MAP_TO_IM);
			int leftx = (int) imPoint[0];
			int topy = (int) imPoint[1];
			assert(leftx > 0 && topy > 0);
			int waitTime = 60*(int)(getDist(imPoint[0], imPoint[1])) + 2000;
		//		moveAndClick(leftx, topy, leftx + 12, topy + 3, 
			//		waitTime, waitTime+200);
			moveAndClick(leftx, topy, leftx, topy,
				waitTime, waitTime+200);
			//update position
			//I clicked leftx, topy, which represents position:
			//Vec2f mapPoint = convertPoint(leftx, topy, IM_TO_MAP);
			//xpos = mapPoint[0];
			//ypos = mapPoint[1];
			xpos = newGold[0];
			ypos = newGold[1];
		}
	}

	void killGhom() {
		moveAndClick(95, 320, 236, 336, 200, 250); //change quest
		Sleep(randInt(400,700));
		updateShot();
		//menu as expected? if so, just click
		if (f.isScreenSeen(*shotAsMat, f.MENU_SCREEN)) {
			moveAndClick(291, 438, 424, 442, 200, 250); //breached keep
			moveAndClick(280, 517, 384, 520, 200, 250); //kill ghom
		}
		//already in quest?
		else if (f.isScreenSeen(*shotAsMat, f.EARLY_ABORT_MENU_SCREEN)) {
			cout<<"already in quest"<<endl;
			selectQuest(23,0);
			moveAndClick(845, 586, 845, 586, 200, 250); //click "Select Quest"
			waitTillSeen(f.OK_SCREEN);
			moveAndClick(533, 423, 580, 431, 300, 350); //click OK
			waitTillSeen(f.GAME_START_SCREEN);
			moveAndClick(179, 278, 233, 289, 0, 100); //click "Start Game"
			waitTillSeen(f.KEEP0_SCREEN);
			Sleep(randInt(1000,1500));
			//press Esc once to end Azmodan conversation
			Send(L"{ESC down}", 0);
			Sleep(randInt(150, 200));
			Send(L"{ESC up}", 0);
			recover();
			killGhom();
			return;
		}
		else selectQuest(22, 2);
		moveAndClick(845, 586, 845, 586, 200, 250); //click "Select Quest"
		waitTillSeen(f.OK_SCREEN);
		moveAndClick(533, 423, 580, 431, 300, 350); //click OK
		waitTillSeen(f.GAME_START_SCREEN);
		moveAndClick(179, 278, 233, 289, 0, 100); //click "Start Game"
		waitTillSeen(f.KEEP0_SCREEN);
		Sleep(randInt(200,500));
		moveAndClick(418, 349, 445, 368, 0, 100); //click on waypoint
		waitTillSeen(f.WAYPOINT_SCREEN);
		moveAndClick(122, 291, 222, 299, 0, 100); //go to keep 3
		waitTillSeen(f.KEEP_LEVEL3_SCREEN);
		moveAndClick(1062, 154, 1082, 197, 0, 100); //enter larder
		waitTillSeen(f.LARDER_SCREEN);
		moveAndHold(918, 130, 923, 145, 7000, 9000); //walk in larder
		//click escape
		Send(L"{SPACE down}", 0);
		Sleep(randInt(50, 100));
		Send(L"{SPACE up}", 0);
		moveAndClick(541, 245, 584, 256, 1000, 1900); //click "OK"
		moveAndHold(913, 133, 934, 152, 0, 0, false, L"right"); //right click on Ghom

		double endTime = getTime() + 20;
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

			Send(L"{1 down}", 0);
			Sleep(randInt(100, 400));
			Send(L"{1 up}", 0);
			Send(L"{1 down}", 0);
			Sleep(randInt(100, 400));
			Send(L"{1 up}", 0);

			double leftClickEndTime = getTime() + randInt(60, 100)/10.0;
			leftClickEndTime = min(leftClickEndTime, endTime);
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
		Send(L"{LSHIFT up}", 0);
		updateShot();
		if (!f.questEnded(*shotAsMat)) {
			cout << "quest ended returned false" << endl;
			recover(); return;
		}
		moveAndClick(646, 620, 704, 631, 200, 300);
		//don't fool around picking things up for more than 10 seconds
		double startPickTime = getTime();
		Point* p;
		int n=0;
		
		do {
			updateShot();
			p = f.nextItem(*shotAsMat);
			if (p != NULL) {
				int leftx = p->x - 6;
				int topy = p->y - 3;
				assert(leftx > 0 && topy > 0);
				int waitTime = 60*(int)getDist(p->x, p->y) + 100;
				moveAndClick(leftx, topy, leftx + 12, topy + 3, 
					waitTime, waitTime+200);
			}
		} while(p != NULL && getTime() - startPickTime < 20);
		//now pick up gold
		
		Point* goldLoc = NULL;
		do {
			updateShot();
			goldLoc = f.findGold(*shotAsMat);
			if (goldLoc != NULL) {
				int leftx = goldLoc->x - 7;
				int topy = goldLoc->y - 4;
				assert(leftx > 0 && topy > 0);
				int waitTime = 60*(int)getDist(goldLoc->x, goldLoc->y) + 100;
				moveAndClick(leftx, topy, leftx + 14, topy + 8,
					waitTime, waitTime+200);
			}
		} while(goldLoc != NULL && getTime() - startPickTime < 20);
		/*
		updateShot();
		vector<Point> allGold = f.findAllGold(*shotAsMat);
		collectGold(allGold);*/
		Send(L"{t down}", 0);
		Sleep(randInt(100, 160));
		Send(L"{t up}", 0);
		waitTillSeen(f.END_KEEP_SCREEN);
		Sleep(randInt(300, 400));
		bool success = goToSupplyChest();
		if (!success) {
			recover(); 
			return;
		}
		moveAndClick(457, 299, 471, 315, 700, 800);
		//sell items
		Sleep(randInt(600,1000));
		updateShot();
		Mat refShot = (*shotAsMat).clone();
		for (int r = 0; r < 5; r++) {
			for (int c = 0; c < 10; c++) {
				if (r==0 && c==0) continue;
				if (r != 0 && c != 0) continue;
				if (!f.invSpotEmpty(refShot, r, c)) {
					//updateShot();
					int leftx = 921 + (int)32.78*c + 5;
					int topy = 397 + (int)32.4*r + 5;
					moveAndClick(leftx, topy, leftx + 20, topy + 20,
						200, 300, L"right");
					updateShot();
					if (f.couldNotSell(*shotAsMat)) {
						identifyRare(leftx + randInt(0,20), 
							topy + randInt(0,20));
						moveAndClick(leftx, topy, leftx + 20, topy + 20,
							100, 200, L"right");
					}
				}
			}
		}
		
		Send(L"{ESC down}", 0);
		Sleep(randInt(150, 200));
		Send(L"{ESC up}", 0);
		updateShot();
		if (!f.exitMenuSeen(*shotAsMat)) {
			Send(L"{ESC down}", 0);
			Sleep(randInt(50, 100));
			Send(L"{ESC up}", 0);
		}
		else { recover(); return; }
		moveAndClick(623, 386, 689, 397, 1000, 2000); //click "leave game"
		waitTillSeen(f.GAME_START_SCREEN);	
	}

};


int main(int argn, char** argv)
{
	LPCWSTR title = L"Diablo III";
	AutoItSetOption(L"MouseCoordMode", 2);

	WinActivate(title, L"");
	WinWaitActive(title, L"", 0);
	//have to get window position so that WinMove only rescales, instead of move + rescale
	long x = WinGetPosX(title, L"");
	long y = WinGetPosY(title, L"");

	WinMove(title, L"", x, y, 1280, 768);
	
	WCHAR handleArr[33];
	WinGetHandle(L"Diablo III", L"", handleArr, 32);
	HWND handle = (HWND) wcstol(handleArr, NULL, 16);
	Bot myBot(handle);
	//myBot.screenshot("diablo0.png",2000);
	//myBot.screenshot("diablo1.png",2000);
	//myBot.screenshot("diablo2.png",2000);
	//myBot.screenshot("diablo3.png",2000);
	//myBot.screenshot("diablo4.png",2000);
	//myBot.screenshot("diablo5.png",2000);
	//myBot.screenshot("diablo6.png",2000);
	//myBot.screenshot("diablo7.png",2000);
	//myBot.screenshot("diablo8.png",2000);
	//myBot.screenshot("diablo9.png",2000);
	//myBot.killGhom();
	for (int i=0; i < 1; i++) {
		cout<<"run "<<i<<endl;
		myBot.killGhom();
	}

	return 0;
}


