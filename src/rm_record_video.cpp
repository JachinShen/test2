#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <string>
#include <ros/ros.h>

int main(int argc, char **argv)
{
  ros::init(argc, argv, "rm_record_video");
  ros::NodeHandle node;

  /*initialize video wrtier*/
  cv::VideoWriter writer;
  std::string file_name;
  std::cout<<"please give a file name"<<std::endl;
  std::cin>>file_name;
  file_name= "/home/zby/ros_bags/7.31/" + file_name + ".avi";
    writer.open(file_name, CV_FOURCC('P', 'I', 'M', '1'), 30,
                  cv::Size(640, 480));
                  
  /*initialize video capture*/
  cv::VideoCapture cap(1);
  if(!cap.isOpened())
  {
    std::cout<<"open camera error "<<std::endl;
    return -2;
  }

  while(ros::ok())
  {
    cv::Mat frame;
    cap>>frame;
    cv::imshow("frame",frame);
    writer.write(frame);
    cv::waitKey(1);
  }

  cap.release();
  writer.release();

  return 1;
}