/**
 * @file april_tags.cpp
 * @brief Example application for April tags library
 * @author: Michael Kaess
 *
 * Opens the first available camera (typically a built in camera in a
 * laptop) and continuously detects April tags in the incoming
 * images. Detections are both visualized in the live image and shown
 * in the text console. Optionally allows selecting of a specific
 * camera in case multiple ones are present and specifying image
 * resolution as long as supported by the camera. Also includes the
 * option to send tag detections via a serial port, for example when
 * running on a Raspberry Pi that is connected to an Arduino.
 */

using namespace std;

#include <iostream>
#include <cstring>
#include <vector>
#include <list>
#include <sys/time.h>

// OpenCV library for easy access to USB camera and drawing of images
// on screen
#include "opencv2/opencv.hpp"

// April tags detector and various families that can be selected by command line option
#include "AprilTags/TagDetector.h"
#include "AprilTags/Tag16h5.h"

// base armor detector
#include "FindArmorV.h"

// utility function to provide current system time (used below in
// determining frame rate at which images are being processed)
double tic()
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return ((double)t.tv_sec + ((double)t.tv_usec) / 1000000.);
}

#include <cmath>

#define corA 0.1524
#define corB 1.8476

Eigen::Vector3d basetags[8] = {{corA, corA, 0}, {1, corA, 0}, {corB, corA, 0}, {corA, 1, 0}, {corB, 1, 0}, {corA, corB, 0}, {1, corB, 0}, {corB, corB, 0}};

vector<Point2f> base_position;
int base_position_length = 10;//length for vetcor base_position
int base_position_counter = 0;

#define m100_move 1 //1 is move;


class Demo
{

    AprilTags::TagDetector *m_tagDetector;
    AprilTags::TagCodes m_tagCodes;

    bool m_draw;   // draw image and April tag detections?
    bool m_timing; // print timing information for each tag extraction call

    int m_width; // image size in pixels
    int m_height;
    double m_tagSize; // April tag side length in meters of square black frame
    double m_fx;      // camera focal length in pixels
    double m_fy;
    double m_px; // camera principal point
    double m_py;

    int m_deviceId; // camera id (in case of multiple cameras)

    Eigen::Vector3d camera;
    Eigen::Vector3d base;
    bool CameraFound;
    bool BaseFound;

    cv::VideoCapture m_cap;

  public:
    // default constructor
    Demo() : // default settings, most can be modified through command line options (see below)
             m_tagDetector(NULL),
             m_tagCodes(AprilTags::tagCodes16h5),

             m_draw(true),
             m_timing(true),

             m_width(640),
             m_height(480),
             m_tagSize(0.2286),
             m_fx(508.013),
             m_fy(507.49),
             m_px(322.632),
             m_py(231.39),

             m_deviceId(1)
    {
    }

    void setup()
    {
        m_tagDetector = new AprilTags::TagDetector(m_tagCodes);

        // prepare window for drawing the camera images
        if (m_draw)
        {
            cv::namedWindow("apriltags_demo", 1);
        }
    }

    void setupVideo()
    {

        // find and open a USB camera (built in laptop camera, web cam etc)
        m_cap = cv::VideoCapture(m_deviceId);
        //m_cap.open("/home/mzm/apriltags/example/releaseball.avi");
        if (!m_cap.isOpened())
        {
            cerr << "ERROR: Can't find video device " << m_deviceId << "\n";
            exit(1);
        }
        //m_cap.set(CV_CAP_PROP_POS_FRAMES,1500);
        m_cap.set(CV_CAP_PROP_FRAME_WIDTH, m_width);
        m_cap.set(CV_CAP_PROP_FRAME_HEIGHT, m_height);
        cout << "Camera successfully opened (ignore error messages above...)" << endl;
        cout << "Actual resolution: "
             << m_cap.get(CV_CAP_PROP_FRAME_WIDTH) << "x"
             << m_cap.get(CV_CAP_PROP_FRAME_HEIGHT) << endl;
    }

    void print_detection(AprilTags::TagDetection &detection) const
    {
        cout << "  Id: " << detection.id
             << " (Hamming: " << detection.hammingDistance << ")";

        // recovering the relative pose of a tag:

        // NOTE: for this to be accurate, it is necessary to use the
        // actual camera parameters here as well as the actual tag size
        // (m_fx, m_fy, m_px, m_py, m_tagSize)

        Eigen::Vector3d translation;
        Eigen::Matrix3d rotation, Eye;
        detection.getRelativeTranslationRotation(m_tagSize, m_fx, m_fy, m_px, m_py,
                                                 translation, rotation);

        Eye << 1, 0, 0,
            0, 1, 0,
            0, 0, 1;
        rotation = Eye * rotation.transpose();
        translation = -rotation * translation + basetags[detection.id];

        cout << "  distance=" << translation.norm()
             << endl
             << "  x=" << translation(0)
             << ", y=" << translation(1)
             << ", z=" << translation(2)

             << endl;
    }

    void processImage(cv::Mat &image, cv::Mat &image_gray)
    {
        // detect April tags (requires a gray scale image)
        cv::cvtColor(image, image_gray, CV_BGR2GRAY);
        double t0;
        if (m_timing)
        {
            t0 = tic();
        }

        vector<AprilTags::TagDetection> detections = m_tagDetector->extractTags(image_gray);

        if (m_timing)
        {
            double dt = tic() - t0;
            cout << "Extracting tags took " << dt << " seconds." << endl;
        }
        // select base_tags and change #10 to #7
        for (vector<AprilTags::TagDetection>::iterator itr = detections.begin(); itr != detections.end(); ++itr)
        {
            if (itr->id == 10)
            {
                itr->id = 7;
            }
            if (itr->id > 7)
            {
                itr = detections.erase(itr);
                --itr;
            }
            cout<<"num:"<<itr->id<<" hammingDistance = "<<itr->hammingDistance<<endl;
            //cout<<"num:"<<itr->id<<" good = "<<itr->good<<endl;
            cout<<"num:"<<itr->id<<" x = "<<itr->cxy.first<<" y = "<<itr->cxy.second<<endl;
            if(itr->hammingDistance>0)
            {
                itr = detections.erase(itr);
                --itr;
            }
        }
        CameraFound=false;
        // print out each detection
        cout << detections.size() << " tags detected:" << endl;

        Point2f base_position_center;
        float base_position_radius;

        if (detections.size() > 0)
        {
            CameraFound=true;
            std::vector<cv::Point3f> objPts;
            std::vector<cv::Point2f> imgPts;
            double s = m_tagSize / 2.;
            for (int i = 0; i < detections.size(); i++)
            {
                // print_detection(detections[i]);
                float dx = +basetags[detections[i].id](0);
                float dy = +basetags[detections[i].id](1);
                objPts.push_back(cv::Point3f(-s + dx, -s + dy, 0));
                objPts.push_back(cv::Point3f(s + dx, -s + dy, 0));
                objPts.push_back(cv::Point3f(s + dx, s + dy, 0));
                objPts.push_back(cv::Point3f(-s + dx, s + dy, 0));

                std::pair<float, float> p1 = detections[i].p[0];
                std::pair<float, float> p2 = detections[i].p[1];
                std::pair<float, float> p3 = detections[i].p[2];
                std::pair<float, float> p4 = detections[i].p[3];
                imgPts.push_back(cv::Point2f(p1.first, p1.second));
                imgPts.push_back(cv::Point2f(p2.first, p2.second));
                imgPts.push_back(cv::Point2f(p3.first, p3.second));
                imgPts.push_back(cv::Point2f(p4.first, p4.second));
            }
            cv::Mat rvec, tvec;
            cv::Matx33f cameraMat(
                m_fx, 0, m_px,
                0, m_fy, m_py,
                0, 0, 1);
            cv::Vec4f distParam(0, 0, 0, 0); // all 0?
            cv::solvePnP(objPts, imgPts, cameraMat, distParam, rvec, tvec);
            cv::Matx33d r;
            cv::Rodrigues(rvec, r);
            Eigen::Matrix3d rotation;
            rotation << r(0, 0), r(0, 1), r(0, 2), r(1, 0), r(1, 1), r(1, 2), r(2, 0), r(2, 1), r(2, 2);
            Eigen::Vector3d translation;
            translation << tvec.at<double>(0), tvec.at<double>(1), tvec.at<double>(2);
            camera = -rotation.transpose() * translation;

            cout << "camera  distance=" << camera.norm()
                 << endl
                 << "  xc=" << camera(0)
                 << ", yc=" << camera(1)
                 << ", zc=" << camera(2)
                 << endl;

            cout<<"rotation:"<<rotation<<endl;

            //movement
            if (m100_move)
            {
            	float target_x = 1.0;
            	float target_y = 1.0;
            	float target_z = -2.5;
                Eigen::Vector3d target;
                target<<target_x,target_y,target_z;

            	// float move_x,move_y,move_z;
            	// move_x = target_x-camera(0);
            	// move_y = target_y-camera(1);
            	// move_z = target_z-camera(2);
            	// Eigen::Vector3d move_to_target;
            	// move_to_target << move_x, move_y, move_z;

                Eigen::Matrix3d change;
                change<<0,-1,0,
                        1,0,0,
                        0,0,1;
                Eigen::Matrix3d last_rotation;
                last_rotation = rotation*change;

                cout<<"last ratation:"<<last_rotation<<endl;

                Eigen::Vector3d target_to_M100;
                target_to_M100 = last_rotation*target-camera;

                cout<<"target_to_M100:"<<target_to_M100<<endl;


            	// if(move_x>1.5)
            	// 	move_x = 0.5;
            	// if(move_y>1.5)
            	// 	move_y = 0.5;

            	cout<<"M100 move x:"<<move_x<<endl;
            	cout<<"M100 move y:"<<move_y<<endl;
            }


            BaseFound = false;
            Point BaseCenter(0, 0);
            FindBase(image, BaseFound, BaseCenter);
            if (BaseFound)
            {
                double Zc = 2.0;
                Eigen::Vector3d B;
                Eigen::Matrix3d cameraMatrix;
                // Eigen::Matrix<double, 3, 4> , At;
                // detections[0].getRelativeTranslationRotation(m_tagSize, m_fx, m_fy, m_px, m_py,
                //                                              translation, rotation);
                cameraMatrix << m_fx, 0, m_px,
                    0, m_fy, m_py,
                    0, 0, 1;

                // translation = -rotation * (-rotation.transpose() * translation + basetags[detections[0].id]);

                // Eigen::Matrix4d T;
                // T.topLeftCorner(3, 3) = rotation;
                // T.col(3).head(3) << translation(0), translation(1), translation(2);
                // T.row(3) << 0, 0, 0, 1;
                // At = cameraMatrix * T;
                B << BaseCenter.x, BaseCenter.y, 1;
                B = B * Zc;
                B = cameraMatrix.inverse() * B;
                B = B - translation;
                base = rotation.transpose() * B;
                cout << "find base at :"
                     << endl
                     << "  xb=" << base(0)
                     << ", yb=" << base(1)
                     << ", zb=" << base(2)
                     << endl;

                Point2f tmp;
                tmp.x = base(0)*100;
                tmp.y= base(1)*100; 
                

                    base_position[base_position_counter]=tmp;
                    base_position_counter++;
                    base_position_counter%=base_position_length;

                    for(int i=0;i<base_position.size();i++)
                    {
                    cout<<"base_position "<<i<<":("<<base_position[i].x<<","<<base_position[i].y<<")"<<endl;
                    }

                    if(base_position[base_position_length-1].x > 0 && base_position[base_position_length-1].y > 0)
                    {
                    cv::minEnclosingCircle(base_position,base_position_center,base_position_radius);
                    cout<<"center:"<<base_position_center<<" radius:"<<base_position_radius<<endl;
                    // for(int i=0;i<base_position.size();i++)
                    // {

                    // }
                    }

                    if(base_position_radius <= 2)
                        cout<<"Base STOP!!!BOMB!!"<<endl;

                    vector<Point2f> base_tmp(base_position);
                    base_tmp.swap(base_position);

                    //cout<<"capacity:"<<base_position.capacity()<<endl;
                 	//cout<<"size:"<<base_position.size()<<endl;
            }
        
        }
        
        // show the current image including any detections
        if (m_draw)
        {
            for (int i = 0; i < detections.size(); i++)
            {
                // also highlight in the image
                detections[i].draw(image);
            }
            Point a1=Point(320,0);
            Point a2=Point(320,480);
            Point b1=Point(0,240);
            Point b2=Point(640,240);
            line(image,a1,a2,Scalar(0,255,0));
            line(image,b1,b2,Scalar(0,255,0));
            //imshow("apriltags_demo", image); // OpenCV call
        }


    }

    // The processing loop where images are retrieved, tags detected,
    // and information about detections generated
    void loop()
    {

        cv::Mat image;
        cv::Mat image_gray;

        int frame = 0;
        double last_t = tic();

        for(int a=0;a<base_position_length;a++)
        {
            base_position.push_back(Point2f(0,0));
        }
        while (true)
        {
            // capture frame
            m_cap >> image;

            processImage(image, image_gray);

            // print out the frame rate at which image frames are being processed
            frame++;
            if (frame % 10 == 0)
            {
                double t = tic();
                cout << "  " << 10. / (t - last_t) << " fps" << endl;
                last_t = t;
            }

            // exit if any key is pressed
            // if (cv::waitKey(1) >= 0)            break;
            cv::waitKey(1);
        }
    }

}; // Demo

// here is were everything begins
int main(int argc, char *argv[])
{
    Demo demo;

    demo.setup();

    // setup image source, window for drawing, serial port...
    demo.setupVideo();

    // the actual processing loop where tags are detected and visualized
    demo.loop();

    return 0;
}
