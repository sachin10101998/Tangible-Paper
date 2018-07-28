#include <stdio.h>
#include <ctime>
#include <string>
#include <iostream>
#include "opencv2/core.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/opencv.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/aruco.hpp> 
#include <time.h>
#include <sys/timeb.h>
#include <sys/time.h>
#include <sstream>

namespace patch
{
    template < typename T > std::string to_string( const T& n )
    {
        std::ostringstream stm ;
        stm << n ;
        return stm.str() ;
    }
}

using namespace std;
using namespace cv;

Mat clk(640,640,CV_8UC3, Scalar(1, 1, 1)); //Mat to store clock image
Mat back_up(640,640,CV_8UC3); //Mat to store backup image

Point cent(320,320);
Point perim(320,0);
int rad =320;
float sec_angle=270;
float min_angle=330;
float hour_angle=210;

time_t rawtime;
struct tm * timeinfo;
float second;
float minute;
float hour;
float millisec;
struct timeb tmb;

int main(int argc, char const *argv[])
{
	Mat markerImage, inputImage, imageLogo, finalImage;
	//imageLogo = imread(argv[1], 1);
	Ptr<aruco::Dictionary> dictionary=aruco::getPredefinedDictionary(aruco::DICT_6X6_250);
	//aruco::drawMarker(dictionary, 23, 200, markerImage, 1); 
	//imshow("markerImage", markerImage);
	VideoCapture capture("http://192.168.0.102:8080/video"), toBePasted;
	//VideoCapture capture(0), toBePasted;
	vector< int > markerIds;
	vector< vector<Point2f> > markerCorners, rejectedCandidates;
	Ptr<aruco::DetectorParameters> parameters = aruco::DetectorParameters::create(); 

	clk = imread("clocks/clock3.png");
	resize(clk, clk, Size(640, 640));
	string day, date, mon, year;
    time_t t = time(0);  
    struct tm * now = localtime( & t );
    day = patch::to_string(now->tm_mday);
    mon = patch::to_string(now->tm_mon+1);
    year = patch::to_string(now->tm_year+1900);
    date = day+"-"+mon+"-"+year;
    putText(clk, date, Point(120, 280), FONT_HERSHEY_TRIPLEX, 2, Scalar(1, 1, 1), 4, false);

	back_up=clk.clone();

	while(capture.read(inputImage))
	{
		//inputImage = imread("full.jpg", 1);
		//imshow("inputImage", inputImage);
		ftime(&tmb);
		rawtime=tmb.time;
		timeinfo = localtime ( &rawtime );

		second     = timeinfo->tm_sec;
		minute     = timeinfo->tm_min;
		hour       = timeinfo->tm_hour;
		millisec   = tmb.millitm;


		second=second+millisec/1000;
		sec_angle=(second*6)+270;

		minute=minute+second/60;
		min_angle=minute*6+270; 

		if(hour>12)hour = hour-12;
		   hour_angle=(hour*30)+(minute*.5)+270; 


		if(sec_angle>360)sec_angle=sec_angle-360;
		if(min_angle>360)min_angle=min_angle-360;
		if(hour_angle>360)hour_angle=hour_angle-360;

		perim.x =  (int)round(cent.x + (rad-25) * cos(sec_angle * CV_PI / 180.0));
		perim.y =  (int)round(cent.y + (rad-25) * sin(sec_angle * CV_PI / 180.0));
		line(clk,cent,perim, Scalar(255,0,0,0), 4,CV_AA,0);


		perim.x =  (int)round(cent.x + (rad-50) * cos(min_angle * CV_PI / 180.0));
		perim.y =  (int)round(cent.y + (rad-50) * sin(min_angle * CV_PI / 180.0));
		line(clk,cent,perim, Scalar(255,0,0,0), 8,CV_AA,0);


		perim.x =  (int)round(cent.x + (rad-75) * cos(hour_angle * CV_PI / 180.0));
		perim.y =  (int)round(cent.y + (rad-75) * sin(hour_angle * CV_PI / 180.0));
		line(clk,cent,perim, Scalar(255,0,0,0), 12,CV_AA,0);

		imageLogo = clk.clone();
		//imshow("Clock",clk); 
		clk.setTo(0); 
		clk=back_up.clone();

		aruco::detectMarkers(inputImage, dictionary, markerCorners, markerIds, parameters, rejectedCandidates);
		finalImage = inputImage.clone();
			for(int i = 0; i < markerCorners.size(); ++i)
			{
				if(markerIds[i] == 40)
				{
				vector<Point2f> left_image;      
				vector<Point2f> right_image;

				left_image.push_back(Point2f(float(0),float(0)));
				left_image.push_back(Point2f(float(imageLogo.cols),float(0)));
				left_image.push_back(Point2f(float(imageLogo.cols),float(imageLogo.rows)));
			    left_image.push_back(Point2f(float(0),float(imageLogo.rows)));		    

			    for(int j = 0; j < markerCorners[i].size(); ++j)
			    {	
			    	right_image.push_back(markerCorners[i][j]);
			    }
				Mat H = findHomography(  left_image,right_image,0 );
		        Mat logoWarped;
		        warpPerspective(imageLogo,logoWarped,H,finalImage.size() );


		        Mat gray,gray_inv,src1final,src2final;
		        Mat src1 = finalImage, src2 = logoWarped.clone();
			    cvtColor(src2,gray,CV_BGR2GRAY);
			    threshold(gray,gray,0,255,CV_THRESH_BINARY);
			    //adaptiveThreshold(gray,gray,255,ADAPTIVE_THRESH_MEAN_C,THRESH_BINARY,5,4);
			    bitwise_not ( gray, gray_inv );
			    src1.copyTo(src1final,gray_inv);
			    src2.copyTo(src2final,gray);
			    finalImage = src1final+src2final;
				}
		    }
		    
		aruco::drawDetectedMarkers(finalImage, markerCorners, markerIds);
		imshow("outputImage", finalImage);
		waitKey(10);
	}
	waitKey(0);
}
