#include <iostream>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

using namespace cv;
using namespace std;

 int main( int argc, char** argv )
 {
    VideoCapture cap(0); 

    if ( !cap.isOpened() ) 
    {
         cout << "Cannot open the web cam" << endl;
         return -1;
    }

    namedWindow("Control", CV_WINDOW_AUTOSIZE); 

  int iLowH = 0;
 int iHighH = 10;

  int iLowS = 100; 
 int iHighS = 255;

  int iLowV = 100;
 int iHighV = 255;

 cvCreateTrackbar("LowH", "Control", &iLowH, 179); 
 cvCreateTrackbar("HighH", "Control", &iHighH, 179);

  cvCreateTrackbar("LowS", "Control", &iLowS, 255); 
 cvCreateTrackbar("HighS", "Control", &iHighS, 255);

  cvCreateTrackbar("LowV", "Control", &iLowV, 255); 
 cvCreateTrackbar("HighV", "Control", &iHighV, 255);

    while (cap.read(imgOriginal))
    { 

        Mat imgHSV;

        cvtColor(imgOriginal, imgHSV, COLOR_BGR2HSV); 
       
        Mat imgThresholded, drawing = Mat::zeros(imgOriginal.size(), imgOriginal.type()), threshold_output;

        inRange(imgHSV, Scalar(iLowH, iLowS, iLowV), Scalar(iHighH, iHighS, iHighV), imgThresholded); 
            
        erode(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)) );
        dilate( imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)) ); 

        dilate( imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)) ); 
        erode(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)) );

        Canny( imgThresholded, threshold_output, 50, 150, 3 );
        imshow("threshold_output", threshold_output);
        vector<vector<Point> > contours;
        vector<Vec4i> hierarchy;

        findContours( threshold_output, contours, hierarchy, RETR_CCOMP, CHAIN_APPROX_NONE, Point(0, 0) );
        for( size_t i = 0; i< contours.size(); i++ )
        {
            Scalar color = Scalar(0, 0, 255);
            drawContours( drawing, contours, (int)i, color, 1, 8, vector<Vec4i>(), 0, Point() );
        }
        imshow("drawing", drawing);

        imshow("Thresholded Image", imgThresholded); 
        imshow("Original", imgOriginal); 

              if (waitKey(30) == 27) 
             {
                  cout << "esc key is pressed by user" << endl;
                  break; 
             }
    }

   return 0;

}
