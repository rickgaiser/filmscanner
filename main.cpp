#include <iostream>

#include <opencv2/opencv.hpp>
#include <opencv2/videoio/videoio_c.h>
#include <raspicam/raspicam_cv.h>

const char * display_original = "Display Original";
const char * display_result = "Display Result";
const char * display_debug = "Display Debug";
const int white_threshold = 240;

using namespace cv;
using namespace std;

const String keys =
    "{help h ?       |       | print this message }"
    "{width          |  1640 | set camera width }"
    "{height         |  1232 | set camera height }"
    "{br             |    50 | set camera brightness (0 - 100) }"
    "{co             |     0 | set camera contrast (-100 - 100) }"
    "{sa             |     0 | set camera saturation (-100 - 100) }"
    "{iso            |   100 | set camera gain (100 - 800) }"
    "{ss             |  1400 | set camera shutter speed (us?) }"
    "{wb_r           | 2.255 | set camera white balance red (?) }"
    "{wb_b           | 1.164 | set camera white balance blue (?) }"
    ;

Mat zoom(const Mat &m, float wzoom, float hzoom)
{
    int width  = (float)m.cols / wzoom;
    int height = (float)m.rows / hzoom;
    return m(Rect(
        (m.cols - width) / 2,
        (m.rows - height) / 2,
        width,
        height));
}

int main(int argc, char** argv )
{
    CommandLineParser parser(argc, argv, keys);
    parser.about("Super8 Filmscanner v1.0.0");
    if (!parser.check()) {
        parser.printErrors();
        return 0;
    }
    if (parser.has("help")) {
        parser.printMessage();
        return 0;
    }

    raspicam::RaspiCam_Cv cam;
    cam.setCaptureSize(parser.get<int>("width"), parser.get<int>("height"));
    cam.setBrightness(parser.get<int>("br"));
    cam.setContrast(parser.get<int>("co"));
    cam.setSaturation(parser.get<int>("sa"));
    cam.setISO(parser.get<int>("iso"));
    cam.setShutterSpeed(parser.get<int>("ss"));
    cam.setAWB(raspicam::RASPICAM_AWB_OFF);
    cam.setAWB_RB(parser.get<double>("wb_r"), parser.get<double>("wb_b"));

    cout<<"Connecting to camera"<<endl;
    if ( !cam.open() ) {
        cerr<<"Error opening camera"<<endl;
        return -1;
    }
    cout<<"Connected to camera ="<<cam.getId() <<endl;

    namedWindow(display_original, WINDOW_AUTOSIZE);
    namedWindow(display_result, WINDOW_AUTOSIZE);
    namedWindow(display_debug, WINDOW_AUTOSIZE);

    Point center_prev; // previous center, for detecting movement
    Rect center_move(-5,-5,10,10); // when center moves more than this, movement is detected
    Rect roi_hole_detect(60, 220, 330, 250);
    Rect frame_to_center(-5, -5 - (200/2), 270, 200);

    int state = 0; // 0=unknown, 1=moving, 2=stopped
    while(1) {
        // Capture raw image from camera
        Mat mCap;
        cam.grab();
        cam.retrieve(mCap);

        // TODO: get a zoom lens
        mCap = zoom(mCap, 2.0f, 2.0f);

        // TODO: rotate the camera 90 degrees
        rotate(mCap, mCap, cv::ROTATE_90_CLOCKWISE);

        // Select region of interest
        Mat mRoi = mCap(roi_hole_detect);

        // Detect hole in roi
        Mat mMask;
        inRange(mRoi, Scalar(white_threshold, white_threshold, white_threshold), Scalar(255, 255, 255), mMask);
        Rect hole = boundingRect(mMask);
        imshow(display_debug, mMask);

        // Validate size of detected hole
        if(hole.height > 10
        && hole.height < 100
        && hole.width  > 10
        && hole.width  < 100) {
            // Valid hole found
            Point center(hole.x+hole.width, hole.y+hole.height/2);
            Rect frame = frame_to_center + center;

            if((center - center_prev).inside(center_move)) {
                // Frame stopped
                if(state == 1) {
                    // Frame was moving, this must be the new frame:
                    // - Take the shot!
                    imshow(display_result, Mat(mRoi, frame));
        
                    // Debug the average "white" color, used for:
                    // - white balancing
                    // - iso and shutter speed calibration
                    Scalar average = mean(mRoi, mMask);
                    cout<<average<<endl;
                }
                state = 2;
            }
            else {
                // Frame moving
                state = 1;
            }
            center_prev = center;

            // Draw mRoi
            rectangle(mCap, roi_hole_detect, Scalar(255, 0, 0), 1);
            
            // Draw in mRoi
            rectangle(mRoi, hole, Scalar(255, 0, 0), (state == 2) ? -1 : 1);
            circle(mRoi, center, 10, Scalar(255, 0, 0), 1);
            rectangle(mRoi, frame, Scalar(0, 255, 0), 1);
        }
        else {
            // Invalidate the center, frame is moving
            center_prev = Point(0, 0);
            state = 1;
        }

        imshow(display_original, mCap);
        if (waitKey(1) == 27)
            break;
    }
    
    cam.release();

    return 0;
}
