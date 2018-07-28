#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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

#include <libxml++/libxml++.h>
#include <libxml++/parsers/textreader.h>
#include <curl/curl.h>
#include <cstdlib>
#include <fstream>
#include <stdlib.h>

#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT        unsigned short
#define SOCKET    int
#define HOSTENT  struct hostent
#define SOCKADDR    struct sockaddr
#define SOCKADDR_IN  struct sockaddr_in
#define ADDRPOINTER  unsigned int*
#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1

#define FULL 0
#define CENTERED 1
#define MAINPAGE 2
#define NEWSPAGE 3

using namespace std;
using namespace cv;

Mat weatherIcon, finalCal, tempImage, backIcon, nextIcon, prevIcon, noImageIcon, finalImage, inputImage;
vector< int > markerIds;
vector< vector<Point2f> > markerCorners, rejectedCandidates;
int idx = 0;
map<int, int> convertToId;

Mat clk(640,640,CV_8UC3, Scalar(1, 1, 1)); 
Mat back_up(640,640,CV_8UC3);

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

struct Page
{
  int type;
  int code;
  vector<Mat> images;
  Mat header;

  Page()
  {
    type = CENTERED;
    code = NEWSPAGE;
    header = imread("noImageIcon.jpg", 1);
  }
  Page(int t, int c)
  {
    type = t;
    code = c;
  }
}mainPage(FULL, MAINPAGE), topNewsPage, moneyNewsPage, sportsNewsPage, lifestylePage, worldNewsPage, healthPage, techNewsPage, mediaNewsPage;

Page currentPage;

void initializeMainPage()
{
  Mat test;
  test = imread("mainPageHeaders/topHeader.jpg", 1);
  mainPage.images.push_back(test);
  topNewsPage.header = test.clone();
  test = imread("mainPageHeaders/moneyHeader.jpg", 1);
  mainPage.images.push_back(test);
  moneyNewsPage.header = test.clone();
  test = imread("mainPageHeaders/sportsHeader.jpg", 1);
  mainPage.images.push_back(test);
  sportsNewsPage.header = test.clone();
  test = imread("mainPageHeaders/lifeStyleHeader.jpg", 1);
  mainPage.images.push_back(test);
  lifestylePage.header = test.clone();
  test = imread("mainPageHeaders/worldHeader.jpg", 1);
  mainPage.images.push_back(test);
  worldNewsPage.header = test.clone();
  test = imread("mainPageHeaders/healthHeader.jpg", 1);
  mainPage.images.push_back(test);
  healthPage.header = test.clone();
  test = imread("mainPageHeaders/techHeader.jpg", 1);
  mainPage.images.push_back(test);
  techNewsPage.header = test.clone();
  test = imread("mainPageHeaders/mediaHeader.jpg", 1);
  mainPage.images.push_back(test);
  mediaNewsPage.header = test.clone();
}

double triArea(Point2f a, Point2f b, Point2f c)
{
  return abs(((b.x-a.x)*(c.y-a.y) - (c.x-a.x)*(b.y-a.y))/2.0);
}

struct box
{
  Point2f pt[4];

  box()
  {
    for(int i = 0; i < 4; ++i)
      pt[i] = Point(0, 0);
  }

  bool valid()
  {
    for(int i = 0; i < 4; ++i)
    {
      if(pt[i].x <= 0 or pt[i].y <= 0)
        return false;
    }
    return true;
  }
  bool inside(Point2f selectPoint)
  {
    cout<<"box is : ("<<pt[0].x<<", "<<pt[0].y<<") ("<<pt[1].x<<", "<<pt[1].y<<") ("<<pt[2].x<<", "<<pt[2].y<<") ("<<pt[3].x<<", "<<pt[3].y<<") \n";
    double area = triArea(pt[0], pt[1], selectPoint), boxArea = triArea(pt[0], pt[1], pt[2]) + triArea(pt[0], pt[3], pt[2]);
    if(boxArea < 0.1)
      return false;
    area += triArea(pt[1], pt[2], selectPoint);
    area += triArea(pt[2], pt[3], selectPoint);
    area += triArea(pt[3], pt[0], selectPoint);
    cout<<"boxArea = "<<boxArea<<" area = "<<area<<endl;
    if(abs(area-boxArea) < 0.1)
      return true;
    else
      return false;
  }
};
map<int, box> tiles;
box nextTile, prevTile, enterTile, upTile;

class Stream
{
    std::map<int, int> packetsSend;
    std::vector<SOCKET> clients;
    SOCKET sock;
    fd_set master;
    int timeout; 
    int quality; 

    int _write( int sock, char *s, int len ) 
    { 
        if ( len < 1 ) { len = strlen(s); }
        #if defined(__APPLE_CC__) || defined(BSD)
            return send(sock, s, len, 0);
        #elif defined(__linux__)
            return send(sock, s, len, MSG_NOSIGNAL);
        #endif
    }

    public:

        Stream(int port = 0) : sock(INVALID_SOCKET), timeout(10), quality(50)
        {
            FD_ZERO( &master );
            if (port) open(port);
        }

        ~Stream() 
        {
            release();
        }

        bool release();
        bool open(int port);
        bool isOpened();
        bool connect();
        void write(Mat &image);
};

bool Stream::release()
    {
        for(int i = 0; i < clients.size(); i++)
        {
            shutdown(clients[i], 2);
            FD_CLR(clients[i],&master); 
        }
        
        clients.clear();
        
        if (sock != INVALID_SOCKET)
        {
            shutdown(sock, 2);
        }
        sock = (INVALID_SOCKET);
        
        return false;
    }

bool Stream::open(int port)
    {
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        
        int reuse = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

        SOCKADDR_IN address;       
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        
        while(bind(sock, (SOCKADDR*) &address, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
        {
            cout << "Stream: couldn't bind sock";
            release();
            usleep(1000*10000);
            sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        }
        
        while(listen(sock, 2) == SOCKET_ERROR)
        {
            cout << "Stream: couldn't listen on sock";
            usleep(1000*10000);
        }
        
        FD_SET(sock, &master);    
        
        return true;
    }

bool Stream::isOpened() 
    {
        return sock != INVALID_SOCKET; 
    }

bool Stream::connect()
    {
        fd_set rread = master;
        struct timeval to = {0,timeout};
        SOCKET maxfd = sock+1;
        
        if(select( maxfd, &rread, NULL, NULL, &to ) <= 0)
            return true;

        int addrlen = sizeof(SOCKADDR);
        SOCKADDR_IN address = {0};     
        SOCKET client = accept(sock, (SOCKADDR*)&address, (socklen_t*) &addrlen);

        if (client == SOCKET_ERROR)
        {
            cout << "Stream: couldn't accept connection on sock";
            cout << "Stream: reopening master sock";
            release();
            open(8888);
            return false;
        }
  
        maxfd=(maxfd>client?maxfd:client);
        FD_SET( client, &master );
        _write( client,"HTTP/1.0 200 OK\r\n"
            "Server: Mozarella/2.2\r\n"
            "Accept-Range: bytes\r\n"
            "Max-Age: 0\r\n"
            "Expires: 0\r\n"
            "Cache-Control: no-cache, private\r\n"
            "Pragma: no-cache\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=mjpegstream\r\n"
            "\r\n",0);
        
        clients.push_back(client);
        packetsSend[client] = 0;

        return true;
    }

void Stream::write(Mat &image)
{
    try
    {
        if(clients.size()==0) return;
            
            // Encode the image
        cv::Mat frame = image;
        if(frame.cols > 0 && frame.rows > 0)
        {
            std::vector<uchar>outbuf;
            std::vector<int> params;
            params.push_back(cv::IMWRITE_JPEG_QUALITY);
            params.push_back(quality);
            cv::imencode(".jpg", frame, outbuf, params);
            int outlen = outbuf.size();

            for(int i = 0; i < clients.size(); i++)
            {
                packetsSend[clients[i]]++;

                int error = 0;
                socklen_t len = sizeof (error);
                int retval = getsockopt(clients[i], SOL_SOCKET, SO_ERROR, &error, &len);

                if (retval == 0 && error == 0)
                {
                    char head[400];
                    sprintf(head,"--mjpegstream\r\nContent-Type: image/jpeg\r\nContent-Length: %lu\r\n\r\n",outlen);

                    _write(clients[i],head,0);

                    retval = getsockopt(clients[i], SOL_SOCKET, SO_ERROR, &error, &len);

                    if (retval == 0 && error == 0)
                    {
                        _write(clients[i],(char*)(&outbuf[0]),outlen);
                    }
                }

                if (retval != 0 || error != 0)
                {
                    shutdown(clients[i], 2);
                    FD_CLR(clients[i],&master);
                    std::vector<int>::iterator position = std::find(clients.begin(), clients.end(), clients[i]);
                    if (position != clients.end())
                    {
                        clients.erase(position);
                    }
                }
            }
        }
    }
    catch(cv::Exception & ex){}
}

namespace patch
{
    template < typename T > std::string to_string( const T& n )
    {
        std::ostringstream stm ;
        stm << n ;
        return stm.str() ;
    }
}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

struct indent {
  int depth_;
  indent(int depth): depth_(depth) {};
};

std::ostream & operator<<(std::ostream & o, indent const & in)
{
  for(int i = 0; i != in.depth_; ++i)
  {
    o << "  ";
  }
  return o;
}

bool createNewsImages(string type, vector<Mat> &newsImages)//e.g type = topNews not top
{
	CURL *curl;
  CURLcode res;
  std::string readBuffer;
  string url = "http://feeds.reuters.com/reuters/IN";
  url += type;
  string xmlFileName = "xmls/";
  xmlFileName += type;
  xmlFileName += ".xml";
    
  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        
    res = curl_easy_perform(curl);
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
    else
    {   
      ofstream myfile (xmlFileName.c_str());
      myfile<<readBuffer;
    }
    curl_easy_cleanup(curl);
  }
  int count = 1, number = 2;
  string toput[3]; 
  Mat test;
  std::locale::global(std::locale(""));
  try
  {
    xmlpp::TextReader reader(xmlFileName.c_str());

    while(reader.read())
    {
      int depth = reader.get_depth();
      if(reader.has_value())
      {
        if(reader.get_value() == type)
        {
          count = 1;
          toput[count-1] = type;
        }
        else
        {
          if(reader.get_depth() == 4)
          {
            string s = reader.get_value();
            if(s.substr(0, 4) != "http" and !(s[0] >= '0' and s[0] <='9' ) and s != "Reuters News")
            {
              string sub = s.substr(0, 3);
              if(!(sub == "Mon" or sub == "Tue" or sub == "Wed" or sub == "Thu" or sub == "Fri" or sub == "Sat" or sub == "Sun"))
              {
                string toprint;
                count++;
                int i = s.find("<");
                if(i != string::npos)
                 toprint = s.substr(0, i);
                else
                  toprint = s;
                cout << toprint << endl;
                toput[count-1] = toprint;
                if(count == 3)
                {
                  int cnt = 0, previ = 0;
                  for(int i = 0; i < toput[1].size(); ++i)
                  {
                    previ = i;
                    while(toput[1][i] != ' ' and i < toput[1].size()-1)
                    {
                    	if(toput[1][i] == '\'')
                    	{
                    		toput[1] = toput[1].substr(0, i)+toput[1].substr((i+1)%toput[1].size(), toput[1].size()-i-1);
                    		--i;
                    	}
                    	if(toput[1][i] == '\"')
                    	{
                    		toput[1] = toput[1].substr(0, i)+toput[1].substr((i+1)%toput[1].size(), toput[1].size()-i-1);
                    		--i;
                    	}

                        ++i;
                        ++cnt;
                    }
                    ++cnt;
                    if(cnt > 24)
                    {
                      toput[1] = toput[1].substr(0, previ)+'\n'+toput[1].substr(previ, toput[1].size()-previ);
                      cnt = 0;
                    }
                  }
                  string front = "convert -pointsize 20 -fill gray -draw 'text 30, 100 \"", rear = "\" ' newsImages/newspaper.jpg ", newImage = "newsImages/", command;
                  newImage += type;
                  newImage += "/";
                  newImage += type;
                  if(number == 4)
                  	newImage = newImage + patch::to_string(200) + ".jpg";
              	  else
              	  	newImage = newImage + patch::to_string(number*20) + ".jpg";
                  /*cout<<"front = "<<front<<endl;
                  cout<<"toput[1] = "<<toput[1]<<endl;
                  cout<<"rear = "<<rear;
                  cout<<"newImage = "<<newImage<<endl;*/
                  command = front;
                  command += toput[1];
                  command += rear;
                  command += newImage;
                  //std::cout<<"command = "<<command<<endl;
                  system(command.c_str());
                  //cout<<endl;
                  count = 0;
                  test = imread(newImage, 1);
                  newsImages.push_back(test);
                  number++;
                }
              }
            }
          }
        }
      }
    }
  }
  catch(const std::exception& e)
  {
    std::cerr << "Exception caught: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

bool getWeather()
{
	CURL *curl;
  CURLcode res;
  std::string readBuffer;

    
  curl = curl_easy_init();
  if(curl) 
  {
    curl_easy_setopt(curl, CURLOPT_URL, "http://api.openweathermap.org/data/2.5/weather?q=Mumbai%2CIndia&mode=xml&appid=4208d08d55c67489ec3914dac5d811b2");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        
    res = curl_easy_perform(curl);

    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
        curl_easy_strerror(res));
    else
    {
      ofstream myfile ("xmls/weather.xml");
      myfile<<readBuffer;
    }
    curl_easy_cleanup(curl);
  }

  std::locale::global(std::locale(""));
  string temp, weather, icon;
  int tempInt;

  try
  {
    xmlpp::TextReader reader("xmls/weather.xml");

    while(reader.read())
    {
      int depth = reader.get_depth();

      if(reader.has_attributes())
      {
        if(!reader.get_name().compare("weather"))
        {
          reader.move_to_first_attribute();
          do
          {
            if(!reader.get_name().compare("value"))
              weather = reader.get_value();
            if(!reader.get_name().compare("icon"))
              icon = reader.get_value();
          } while(reader.move_to_next_attribute());
        }
        if(!reader.get_name().compare("temperature"))
        {
          reader.move_to_first_attribute();
          temp = reader.get_value();
        }
      }
    }
  }
  catch(const std::exception& e)
  {
    std::cerr << "Exception caught: " << e.what() << std::endl;
  }
  tempInt = 0;
  int i = 0;
  while(isdigit(temp[i]))
  {
    tempInt *= 10;
    tempInt += (temp[i]-'0');
    i++;
  }
  tempInt -= 273;
  cout<<"temperature = "<<tempInt<<" weather = "<<weather<<" icon = "<<icon<<endl;
  string weatherIconPath = "weather_icons/";
  weatherIconPath += (icon+".png");
  cout<<weatherIconPath<<endl;
  weatherIcon = imread(weatherIconPath, 1);
  string temper = patch::to_string(tempInt);
  string front = "convert -pointsize 120 -fill black -draw 'text 65, 245 ", rear = " ' weatherImages/temp_background.jpg weatherImages/temp_backgroundi.jpg", command;

    command = front+"\""+temper+"\""+rear;
    std::cout<<"command = "<<command;

    system(command.c_str());
    front = "convert -pointsize 40 -fill black -draw 'text 80, 95 ", rear = " ' weatherImages/temp_backgroundi.jpg weatherImages/temperature.jpg", command;

    command = front+"\""+"Mumbai"+"\""+rear;
    std::cout<<"command = "<<command;

    system(command.c_str());

    tempImage = imread("weatherImages/temperature.jpg", 1);

  return EXIT_SUCCESS;
}

void createDateImage()
{
	string day, date, mon, year;
	time_t t = time(0);  
    struct tm * now = localtime( & t );
	int monint = now->tm_mon;
    day = patch::to_string(now->tm_mday);
    year = patch::to_string(now->tm_year+1900);
    if(monint == 0)
    	mon = "JAN";
    if(monint == 1)
    	mon = "FEB";
    if(monint == 2)
    	mon = "MAR";
    if(monint == 3)
    	mon = "APR";
    if(monint == 4)
    	mon = "MAY";
    if(monint == 5)
    	mon = "JUN";
    if(monint == 6)
    	mon = "JUL";
    if(monint == 7)
    	mon = "AUG";
    if(monint == 8)
    	mon = "SEP";
    if(monint == 9)
    	mon = "OCT";
    if(monint == 10)
    	mon = "NOV";
    if(monint == 11)
    	mon = "DEC";

    string front = "convert -pointsize 250 -fill blue -draw 'text 180, 410 ", rear = " ' calendarImages/calendar.jpg calendarImages/cali.jpg", command;

    command = front+"\""+day+"\""+rear;
    std::cout<<"command = "<<command;
	system(command.c_str());
	front = "convert -pointsize 120 -fill black -draw 'text 80, 150 "; rear = " ' calendarImages/cali.jpg calendarImages/cal.jpg"; command = "";

    command = front+"\""+mon+"\""+rear;
    std::cout<<"command = "<<command;
	system(command.c_str());

	finalCal = imread("calendarImages/cal.jpg", 1);
}

void paste(Mat &finalImage, Mat &imageLogo, box &tileCorner)
{
  if(imageLogo.rows > 0 and imageLogo.cols > 0 and tileCorner.valid())
  {
    vector<Point2f> left_image;      
    vector<Point2f> right_image;

    left_image.push_back(Point2f(float(0),float(0)));
    left_image.push_back(Point2f(float(imageLogo.cols),float(0)));
    left_image.push_back(Point2f(float(imageLogo.cols),float(imageLogo.rows)));
    left_image.push_back(Point2f(float(0),float(imageLogo.rows)));        

    for(int j = 0; j < 4; ++j)
    { 
      right_image.push_back(tileCorner.pt[j]);
    }
    Mat H = findHomography(left_image,right_image,0 );
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

clock_t begin_time = clock();
clock_t up_time = begin_time, restBegin = begin_time;

void drawSquare(int tileId)
{
  for( int j = 0; j < 4; j++ )
  {
        line(finalImage, tiles[tileId].pt[j], tiles[tileId].pt[(j+1)%4], Scalar(100, 100, 255), 1, 8 );
  }
}

void drawSquare(box tile)
{
  for( int j = 0; j < 4; j++ )
  {
    line(finalImage, tile.pt[j], tile.pt[(j+1)%4], Scalar(100, 100, 255), 1, 8 );
  }
}

int flag = 0;
vector<RotatedRect> redRectangles, blueRectangles, greenRectangles;
Scalar color = Scalar( 255, 255, 120 );

float dist(Point2f a, Point2f b)
{
  return sqrt((a.x-b.x)*(a.x-b.x) + (a.y-b.y)*(a.y-b.y));
}

Mat hsv; Mat hue, hueSample, greenSample, redSample, blueSample, hsvrSample, hsvySample, hsvbSample;
int bins = 50;
int thresh = 90;

Mat Hist_and_Backproj(Mat , Mat, vector<RotatedRect> &rect);

vector<Point2f> getGesture(Mat src)
{
  std::vector<Point2f> selectPoint;
  blueRectangles.clear();
  greenRectangles.clear();
  cvtColor( src, hsv, CV_BGR2HSV );
  Hist_and_Backproj(hsvySample, src, greenRectangles);
  Hist_and_Backproj(hsvbSample, src, blueRectangles);
  for(int i = 0; i < blueRectangles.size(); ++i)
  {
    Point2f rect_points[4]; blueRectangles[i].points( rect_points );
    float bluel = dist(rect_points[0], rect_points[1]), blueb = dist(rect_points[1], rect_points[2]);
    Point2f blueCentre = Point((rect_points[0].x+rect_points[1].x)/2, (rect_points[0].y+rect_points[1].y)/2);
    for(int j = 0; j < greenRectangles.size(); ++j)
    {
      Point2f rect_points[4]; greenRectangles[j].points( rect_points );
      float greenl = dist(rect_points[0], rect_points[1]), greenb = dist(rect_points[1], rect_points[2]);
      Point2f greenCentre = Point((rect_points[0].x+rect_points[1].x)/2, (rect_points[0].y+rect_points[1].y)/2);
      if((greenl+greenb+bluel+blueb)/2 > dist(blueCentre, greenCentre))
      {
        for( int k = 0; k < 4; k++ )
        {
          line( src, rect_points[k], rect_points[(k+1)%4], color, 1, 8 );
          Point2f rect_points[4]; blueRectangles[i].points( rect_points );
          line( src, rect_points[k], rect_points[(k+1)%4], color, 1, 8 );
          selectPoint.push_back( Point2f((greenCentre.x + blueCentre.x)/2, (greenCentre.y + blueCentre.y)/2) );
        }
      }
    }
  }
  return selectPoint;
}

Mat Hist_and_Backproj( Mat hsvSample, Mat src, vector<RotatedRect> &rect)
{
  MatND hist;

  float hue_range[] = { 0, 180 };
  float sat_range[] = { 0, 255 };
  const float* ranges[] = { hue_range, sat_range };
  int histSize[] = { bins, bins };
  int channels[] = { 0, 1 };

  calcHist( &hsvSample, 1, channels, Mat(), hist, 2, histSize, ranges, true, false );
  normalize( hist, hist, 0, 255, NORM_MINMAX, -1, Mat() );

  /// Get Backprojection
  MatND backproj;
  calcBackProject( &hsv, 1, channels, hist, backproj, ranges, 1, true );

   //morphological closing (removes small holes from the foreground)
  dilate( backproj, backproj, getStructuringElement(MORPH_RECT, Size(7, 7)) ); 
  erode(backproj, backproj, getStructuringElement(MORPH_RECT, Size(7, 7)) );
  blur( backproj, backproj, Size(3,3) );
  for(int row = 0; row < backproj.rows; ++row)
  {
    for(int col = 0; col < backproj.cols; ++col)
    {
      if(backproj.at<uchar>(row, col) > thresh)
        backproj.at<uchar>(row, col) = 255;
      else
        backproj.at<uchar>(row, col) = 0;
    }
  }
  Canny( backproj, backproj, 30, 90, 3);

  vector<vector<Point> > contours;
  vector<Vec4i> hierarchy;

  findContours( backproj, contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, Point(0, 0) );

  vector<RotatedRect> minRect( contours.size() );
  vector<RotatedRect> minEllipse( contours.size() );

  for( int i = 0; i < contours.size(); i++ )
  { 
    minRect[i] = minAreaRect( Mat(contours[i]) );
  }

  Mat drawing = src.clone();
  for( int i = 0; i< contours.size(); i++ )
  {
    Point2f rect_points[4]; minRect[i].points( rect_points );
    float l = dist(rect_points[0], rect_points[1]), b = dist(rect_points[1], rect_points[2]);
    if(l*b > 50 and l*b < 10000)
    {
      rect.push_back(minRect[i]);
      for( int j = 0; j < 4; j++ )
        line( drawing, rect_points[j], rect_points[(j+1)%4], color, 1, 8 );
    }
  }
  return drawing;
}

int selection = 0;
map<int, Page> pageMap;

bool controller()
{
  imshow("outputImage", finalImage);
  vector<Point2f> selectPoint = getGesture(inputImage);
  if((clock() - restBegin)/CLOCKS_PER_SEC > 0.5)
  {
    for(int i = 0; i < selectPoint.size(); ++i)
    {

      cout<<"selectPoint = "<<selectPoint[i].x<<", "<<selectPoint[i].y<<" selectPoint.size() = "<<selectPoint.size()<<endl;
      circle( finalImage,
          selectPoint[i],
          2,  
          Scalar( 0, 0, 255 ),
          -1,
          8 );
      if(currentPage.code == MAINPAGE)
      {
        if(nextTile.inside(selectPoint[i]))
        {
          cout<<"inside next\n";
          cout<<"selection = "<<selection<<endl;
          selection = (selection + 1) % 8;
          cout<<"selection = "<<selection<<endl;
          drawSquare(nextTile);
          imshow("outputImage", finalImage);
          restBegin = clock();
          break;
        }
        if(prevTile.inside(selectPoint[i]))
        {
          cout<<"inside prev\n";
          selection = (selection + 7) % 8;
          drawSquare(prevTile);
          imshow("outputImage", finalImage);
          restBegin = clock();
          break;
        }
        if(enterTile.inside(selectPoint[i]))
        {
          cout<<"inside enter\n";
          currentPage = pageMap[selection];
          drawSquare(enterTile);
          imshow("outputImage", finalImage);
          restBegin = clock();
          break;
        }
        if(upTile.inside(selectPoint[i]))
        {
          cout<<"inside up\n";
          selection = (selection + 4) % 8;
          drawSquare(upTile);
          imshow("outputImage", finalImage);
          restBegin = clock();
          break;
        }
      }
      
      if(currentPage.code == NEWSPAGE)
      {
        if(upTile.inside(selectPoint[i]))
        {
          cout<<"inside up\n";
          currentPage = mainPage;
          drawSquare(upTile);
          imshow("outputImage", finalImage);
          restBegin = clock();
        }
        if(nextTile.inside(selectPoint[i]))
        {
          cout<<"inside next\n";
          begin_time = clock();
          idx = (idx + 1) % currentPage.images.size();
          drawSquare(nextTile);
          imshow("outputImage", finalImage);
          restBegin = clock();
        }
        if(prevTile.inside(selectPoint[i]))
        {
          cout<<"inside prev\n";
          begin_time = clock();
          idx = (idx - 1 + currentPage.images.size()) % currentPage.images.size();
          drawSquare(prevTile);
          imshow("outputImage", finalImage);
          restBegin = clock();
        }
      }
    }
  }
  if(currentPage.code == MAINPAGE)
  {
    //cout<<"in drawSquare selection = "<<selection<<endl;
    drawSquare(convertToId[selection]);
    circle( finalImage,
        tiles[convertToId[selection]].pt[0],
        5,  
        Scalar( 0, 0, 255 ),
        -1,
        8 );
    circle( finalImage,
          tiles[convertToId[selection]].pt[1],
          5,  
          Scalar( 0, 0, 255 ),
          -1,
          8 );
    circle( finalImage,
          tiles[convertToId[selection]].pt[2],
          5,  
          Scalar( 0, 0, 255 ),
          -1,
          8 );
    circle( finalImage,
          tiles[convertToId[selection]].pt[3],
          5,  
          Scalar( 0, 0, 255 ),
          -1,
          8 );
  }
  imshow("outputImage", finalImage);
  waitKey(10);
}

Point2f intersection(Point2f a1, Point2f a2, Point2f b1, Point2f b2)
{
  //cout<<"intersection of "<<a1.x<<", "<<a1.y<<" "<<a2.x<<", "<<a2.y<<" and "<<b1.x<<", "<<b1.y<<" "<<b2.x<<", "<<b2.y;
  /*circle( finalImage,
        a1,
        5,  
        Scalar( 0, 0, 255 ),
        -1,
        8 );
  circle( finalImage,
        a2,
        5,  
        Scalar( 0, 0, 255 ),
        -1,
        8 );
  circle( finalImage,
        b1,
        5,  
        Scalar( 0, 0, 255 ),
        -1,
        8 );
  circle( finalImage,
        b2,
        5,  
        Scalar( 0, 0, 255 ),
        -1,
        8 );
  line(finalImage, a1, a2, Scalar(0, 255, 0), 2);
  line(finalImage, b1, b2, Scalar(0, 255, 0), 2);*/
  double A0, B0, C0, A1, B1, C1, D;
  A0 = a2.y-a1.y;
  B0 = a1.x-a2.x;
  C0 = A0*a1.x + B0*a2.y;
  A1 = b2.y-b1.y;
  B1 = b1.x-b2.x;
  C1 = A1*b1.x + B1*b2.y;
  D = A0*B1 - A1*B0;
  if(abs(D) > 0.0001)
  {
    Point2f intrsxn = Point2f(((B1*C0) - (B0*C1))/D, ((A0*C1) - (A1*C0))/D);
    //cout<<" is : "<<intrsxn.x<<", "<<intrsxn.y<<endl;
    return intrsxn;
  }
  else
    return Point2f(0, 0);
}

int main(int argc, char const *argv[])
{
  Stream wri(7777);
  Mat markerImage, imageLogo;
  convertToId[0] = 200;
  convertToId[1] = 60;
  convertToId[2] = 40;
  convertToId[3] = 140;
  convertToId[4] = 180;
  convertToId[5] = 100;
  convertToId[6] = 120;
  convertToId[7] = 160;

  greenSample = imread("greenSample6.jpg", 1);
  blueSample = imread("blueSample6.jpg", 1);
  cout<<"okay\n";
  cvtColor( greenSample, hsvySample, CV_BGR2HSV );
  cvtColor( blueSample, hsvbSample, CV_BGR2HSV );
  cout<<"okay";

  backIcon = imread("otherIcons/backIcon2.jpg");
  nextIcon = imread("otherIcons/nextIcon2.jpg");
  prevIcon = imread("otherIcons/prevIcon2.jpg");
  noImageIcon = imread("otherIcons/noImageIcon.jpg");
  initializeMainPage();

	Ptr<aruco::Dictionary> dictionary=aruco::getPredefinedDictionary(aruco::DICT_6X6_250);

	VideoCapture capture(1);

  string type = "topNews";
	createNewsImages(type, topNewsPage.images);
  type = "businessNews";
  createNewsImages(type, moneyNewsPage.images);
  type = "sportsNews";
  createNewsImages(type, sportsNewsPage.images);
  type = "worldNews";
  createNewsImages(type, worldNewsPage.images);
  type = "entertainmentNews";
  createNewsImages(type, mediaNewsPage.images);
  type = "technologyNews";
  createNewsImages(type, techNewsPage.images);
  type = "health";
  createNewsImages(type, healthPage.images);
  type = "lifestyle";
  createNewsImages(type, lifestylePage.images);
	getWeather();
	createDateImage();

  pageMap[0] = topNewsPage;
  pageMap[1] = moneyNewsPage;
  pageMap[2] = sportsNewsPage;
  pageMap[3] = lifestylePage;
  pageMap[4] = worldNewsPage;
  pageMap[5] = healthPage;
  pageMap[6] = techNewsPage;
  pageMap[7] = mediaNewsPage;
  currentPage = mainPage;

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
    box centerTile;
		//inputImage = imread("full.jpg", 1);
		imshow("inputImage", inputImage);
    {
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
    }

    tiles.clear();

    for(int i = 0; i < markerCorners.size(); ++i)
    {
      for(int j = 0; j < 4; ++j)
      {
        tiles[markerIds[i]].pt[j] = markerCorners[i][j];
      }
    }
    finalImage = inputImage.clone();

    centerTile.pt[0] = tiles[60].pt[0];
    centerTile.pt[1] = tiles[40].pt[1];
    centerTile.pt[2] = tiles[120].pt[2];
    centerTile.pt[3] = tiles[100].pt[3];

    nextTile.pt[0] = tiles[220].pt[1];
    nextTile.pt[1] = intersection(tiles[220].pt[0], tiles[220].pt[1], tiles[140].pt[2], tiles[140].pt[1]);
    nextTile.pt[2] = tiles[140].pt[1];
    nextTile.pt[3] = intersection(tiles[220].pt[1], tiles[220].pt[2], tiles[140].pt[1], tiles[140].pt[0]);

    prevTile.pt[1] = tiles[1].pt[0];
    prevTile.pt[0] = intersection(tiles[1].pt[1], tiles[1].pt[0], tiles[200].pt[3], tiles[200].pt[0]);
    prevTile.pt[3] = tiles[200].pt[0];
    prevTile.pt[2] = intersection(tiles[1].pt[0], tiles[1].pt[3], tiles[200].pt[0], tiles[200].pt[1]);

    upTile.pt[0] = tiles[180].pt[3];
    upTile.pt[1] = intersection(tiles[180].pt[1], tiles[180].pt[2], tiles[20].pt[1], tiles[20].pt[0]);
    upTile.pt[2] = tiles[20].pt[3];
    upTile.pt[3] = intersection(tiles[180].pt[0], tiles[180].pt[3], tiles[20].pt[2], tiles[20].pt[3]);

    enterTile.pt[1] = tiles[160].pt[2];
    enterTile.pt[0] = intersection(tiles[160].pt[0], tiles[160].pt[3], tiles[80].pt[0], tiles[80].pt[1]);
    enterTile.pt[3] = tiles[80].pt[2];
    enterTile.pt[2] = intersection(tiles[160].pt[1], tiles[160].pt[2], tiles[80].pt[3], tiles[80].pt[2]);

    paste(finalImage, imageLogo, tiles[1]);
		paste(finalImage, weatherIcon, tiles[80]);
		paste(finalImage, tempImage, tiles[20]);
    paste(finalImage, finalCal, tiles[220]);
    paste(finalImage, nextIcon, nextTile);
    paste(finalImage, prevIcon, prevTile);
    paste(finalImage, backIcon, upTile);
    paste(finalImage, noImageIcon, enterTile);
		
    if(currentPage.type == FULL)
    {
      if(currentPage.code == MAINPAGE)
      {
        paste(finalImage, currentPage.images[0], tiles[200]);
        paste(finalImage, currentPage.images[1], tiles[60]);
        paste(finalImage, currentPage.images[2], tiles[40]);
        paste(finalImage, currentPage.images[3], tiles[140]);
        paste(finalImage, currentPage.images[4], tiles[180]);
        paste(finalImage, currentPage.images[5], tiles[100]);
        paste(finalImage, currentPage.images[6], tiles[120]);
        paste(finalImage, currentPage.images[7], tiles[160]);
      }
    }
    if(currentPage.type == CENTERED)
    {
      if(currentPage.code == NEWSPAGE)
      {
        if(begin_time == up_time)
        {
          begin_time = clock();
        }
        if((clock() - begin_time)/CLOCKS_PER_SEC > 10.0)
        {
          begin_time = clock();
          idx = (idx + 1) % currentPage.images.size();
        }
        paste(finalImage, currentPage.header, tiles[200]);
        paste(finalImage, currentPage.images[(idx + 1)%currentPage.images.size()], tiles[140]);
        paste(finalImage, currentPage.images[(idx + currentPage.images.size() - 1)%currentPage.images.size()], tiles[180]);
        paste(finalImage, currentPage.images[(idx + 2)%currentPage.images.size()], tiles[160]);
        paste(finalImage, currentPage.images[idx], centerTile);
      }
    }		    
		//aruco::drawDetectedMarkers(finalImage, markerCorners, markerIds);
    bool b = controller();
		wri.connect();
        wri.write(finalImage);
	}
	waitKey(0);
}