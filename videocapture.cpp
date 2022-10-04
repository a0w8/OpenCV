#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <iostream>
#include <string>

#if WIN32
#include <windows.h>
#else
#include <X11/Xlib.h>
#endif

using namespace cv;



void getScreenResolution(int &width, int &height)
{
  #if WIN32
    width  = (int) GetSystemMetrics(SM_CXSCREEN);
    height = (int) GetSystemMetrics(SM_CYSCREEN);
  #else
    Display *disp = XOpenDisplay(NULL);
    Screen *scrn = DefaultScreenOfDisplay(disp);
    width = scrn->width;
    height = scrn->height;
  #endif
}


String intToString(int num)
{
   std::stringstream ss;
   ss << num;
   return ss.str();
}



int main()
{
  namedWindow("video", WINDOW_NORMAL );
  int width, height;
  getScreenResolution(width,height);
  resizeWindow("video", (int)width*0.6, (int)height*0.6);

  // open capture and check validity
  const String path="car_parking.mp4";
  VideoCapture cap;
  cap.open(path);
  if (!cap.isOpened())
  {
    std::cout << "couldn't open stream" << std::endl;
     return -1;
  }

  Mat frame1,frame2 ;
  int SENSITIVITY_THRESH = 22;
  bool started_recording = 0;
  int video_counter = 0;
  

  cap >> frame1 ;
  cap >> frame2 ;
  Size frame_size = frame1.size();
  int video_fps = cap.get(CAP_PROP_FPS);
  //LEN=time passed from last movement before ending video. In seconds
  int frame_counter = 0,LEN = 2;
  VideoWriter writer;


  while (1)
  {
    if ( frame2.empty() )
    {
	std::cout << "Stream finished" << std::endl;
	break;
    }

    Mat diff_gray,img_blur ;
    Mat img_threshold;
    Mat diff;

    absdiff(frame1,frame2,diff); 
    cvtColor(diff, diff_gray, COLOR_BGR2GRAY);
    //reduce noise with kernel size()
    GaussianBlur(diff_gray, img_blur, Size(5,5), 3, 0);
    threshold(img_blur,img_threshold,SENSITIVITY_THRESH,255,THRESH_BINARY);


    std::vector<std::vector<Point> > contours;
    std::vector<Vec4i> hierarchy;
    findContours(img_threshold, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    for (int i=0; i<contours.size(); i++)
    {
      Rect boundRect = boundingRect(contours[i]);
      rectangle (frame1, boundRect, Scalar(0,255,0),5);
    }


    //write video logic . writes to a file every motion detected . Doesn't start a new video if the motion is less than LEN apart
    if (contours.size()!=0)
    {
       if (!started_recording)
       {
	  writer.open(intToString(++video_counter)+".avi", VideoWriter::fourcc('M','J','P','G'), video_fps, frame_size);
	  if (!writer.isOpened())
	     std::cout << "can't save file" << std::endl;
	  started_recording = 1;
       }
       writer.write(frame1);
       frame_counter = 0;
    }
    else if (started_recording)
    {
       if (frame_counter < (LEN*video_fps))
	{
	    frame_counter++;
	    writer.write(frame1);
	}
	else
        {
	  frame_counter=0;
	  writer.release();
        }
    }

    imshow("video", frame1);

    int key = waitKey(1000 / 30);
    if (key == 27)
      break;
    frame2.copyTo(frame1);
    cap >> frame2;
    
  }
    
    waitKey(0);
    if (started_recording)
       writer.release();
    cap.release();
    destroyAllWindows();
    return 0;
} 