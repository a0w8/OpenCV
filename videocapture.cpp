#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <iostream>
#include <string>
#include <queue>

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


struct MouseArg{
     Mat frame;
     Rect ROI;
     Point first_pos,second_pos;
     bool left_mouse_clicked;
};


void mouseCall(int event, int x, int y, int flags, void* param)
{
     MouseArg &MyMouseArg = *(MouseArg*)param;
     switch(event)
     {
     case (EVENT_LBUTTONDOWN): {
	  MyMouseArg.left_mouse_clicked = 1; 
	  MyMouseArg.first_pos.x = x;  
	  MyMouseArg.first_pos.y = y;  
     }
     break;
     case (EVENT_MOUSEMOVE): {
	  if (MyMouseArg.left_mouse_clicked == 1)
	  {
	       MyMouseArg.second_pos.x = std::min(std::max(x,0), MyMouseArg.frame.cols);  
	       MyMouseArg.second_pos.y = std::min(std::max(y,0), MyMouseArg.frame.rows);
	       MyMouseArg.ROI = Rect(MyMouseArg.first_pos,MyMouseArg.second_pos);
	  }
     }
     break;
     case (EVENT_LBUTTONUP): {
	  MyMouseArg.left_mouse_clicked = 0;
     }
     }
}


bool isInsideRect(Rect bound_rect, Rect MyMouseArgROI)
{
   Point tr=Point(bound_rect.br().x , bound_rect.tl().y);
   Point bl=Point(bound_rect.tl().x , bound_rect.br().y);
   if (MyMouseArgROI.contains(bound_rect.tl()) || MyMouseArgROI.contains(tr)
       || MyMouseArgROI.contains(bl) || MyMouseArgROI.contains(bound_rect.br()))
      return 1;
   else
      return 0;
}
   


String intToString(int num)
{
   std::stringstream ss;
   ss << num;
   return ss.str();
}



int main(int argc, char *argv[])
{
   if (argc<2)
   {
      std::cout<< "need to enter file path as an argument to program\nUsage: <Program_name> <path to video file>" ;
      return -1;
   }

   namedWindow("video", WINDOW_NORMAL );
   int width, height;
   getScreenResolution(width,height);
   resizeWindow("video", (int)width*0.6, (int)height*0.6);


   // open capture and check validity
   const String path=argv[1];
   VideoCapture cap;
   cap.open(path);
   if (!cap.isOpened())
   {
      std::cout << "couldn't open stream" << std::endl;
      return -1;
   }

   Mat frame1,frame2 ;
   cap >> frame1 ;
   cap >> frame2 ;

   //ROI logic
   MouseArg MyMouseArg;
   MyMouseArg.frame = frame2.clone();
   std::cout << "Choose a Region of Interest for movement detection" << std::endl;
   std::cout << "Press left mouse click and drag, press 'ESC' when ready" << std::endl;
   setMouseCallback("video", mouseCall, (void*)&MyMouseArg);
   while (1)
   {
      rectangle (MyMouseArg.frame , MyMouseArg.ROI, Scalar(0,0,255), 5);
      imshow("video",MyMouseArg.frame);
      int key = waitKey(25);
      if (key == 27)
      {
	 setMouseCallback("video", NULL, NULL);
	 break;
      }
      MyMouseArg.frame = frame2.clone();
   }
  

   bool started_recording = 0;
   int video_counter = 0;
   Size frame_size = frame1.size();
   int video_fps = cap.get(CAP_PROP_FPS);
   VideoWriter writer;
   std::queue<Mat> buffer;
   int THRESH_SENS = 22;
   //FRONT_BUFFER=time passed from last movement before ending video. In seconds
   int frame_counter = 0,FRONT_BUFFER = 3, BACK_BUFFER=3;
   double MIN_AREA=400;


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
      threshold(img_blur,img_threshold,THRESH_SENS,255,THRESH_BINARY);


      std::vector<std::vector<Point> > contours;
      std::vector<Vec4i> hierarchy;
      findContours(img_threshold, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

      bool motion_detected=0;
      for (int i=0; i<contours.size(); i++)
      {
	 Rect boundRect = boundingRect(contours[i]);
	 //for each contour detected check if it's bigger than MIN_AREA
	 //and intersects with ROI only then mark frame as motion_detected
	 if (isInsideRect(boundRect, MyMouseArg.ROI) && contourArea(contours[i])>MIN_AREA)
	 {
	    rectangle (frame1, boundRect, Scalar(0,255,0),5);
	    motion_detected = 1;
	 }
      }

       
      //write video logic . writes to a file every motion detected .
      //Doesn't start a new video if the motion is less than FRONT_BUFFER frames apart.
      //Also records a buffer of BACK_BUFFER seconds before every movement
      if (motion_detected)
      {
	 if (!started_recording)
	 {
	    writer.open(intToString(++video_counter)+".avi", VideoWriter::fourcc('M','J','P','G'), video_fps, frame_size);
	    if (!writer.isOpened())
	    {
	       std::cout << "can't save file" << std::endl;
	       return -1;
	    }
	    started_recording = 1;
	    while (!buffer.empty())
	    {
	       writer.write(buffer.front());
	       buffer.pop();
	    }  
	 }
	 writer.write(frame1);
	 frame_counter = 0;
      }
      //no motion detected and started recording
      else if (started_recording)
      {
	 frame_counter++;
	 if (frame_counter < (FRONT_BUFFER*video_fps))
	    writer.write(frame1);
	 else
	 {
	    frame_counter=0;
	    writer.release();
	    started_recording=0;
	 }
      }
      //no motion detected and didn't start recording
      else
      {
	 buffer.push(frame1);
	 if (buffer.size() > BACK_BUFFER*video_fps)
	    buffer.pop();
      }
       

      imshow("video", frame1);
      int key = waitKey(30);
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
