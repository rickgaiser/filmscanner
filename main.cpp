#include <iostream>

#include <opencv2/opencv.hpp>
#include <opencv2/videoio/videoio_c.h>
#include <raspicam/raspicam_cv.h>

const char * display_original = "Display Original";
const char * display_result = "Display Result";
const int white_threshold = 240;
const int frame_xoff = -5;
const int frame_yoff = -5;
const int frame_height = 200;
const int frame_width = 270;

using namespace cv;
using namespace std;

const String keys =
    "{help h ?       |      | print this message                     }"
    "{width          | 1640 | set camera width                       }"
    "{height         | 1232 | set camera height                      }"
    "{br             | 50.0 | set camera brightness    (0.0 - 100.0) }"
    "{co             | 50.0 | set camera contrast      (0.0 - 100.0) }"
    "{sa             | 50.0 | set camera saturation    (0.0 - 100.0) }"
    "{g              | 50.0 | set camera gain          (0.0 - 100.0) }"
    "{ss             | 0    | set camera shutter speed (0.0 - 100.0) 0 is auto }"
    "{wb             | 0    | set camera white balance (0.0 - 100.0) 0 is auto }"
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
    cam.set(CV_CAP_PROP_FRAME_WIDTH,          parser.get<int>("width"));
    cam.set(CV_CAP_PROP_FRAME_HEIGHT,         parser.get<int>("height"));
    cam.set(CV_CAP_PROP_BRIGHTNESS,           parser.get<double>("br"));
    cam.set(CV_CAP_PROP_CONTRAST,             parser.get<double>("co"));
    cam.set(CV_CAP_PROP_SATURATION,           parser.get<double>("sa"));
    cam.set(CV_CAP_PROP_GAIN,                 parser.get<double>("g"));
    cam.set(CV_CAP_PROP_EXPOSURE,             parser.get<double>("ss"));
    cam.set(CV_CAP_PROP_WHITE_BALANCE_RED_V,  parser.get<double>("wb"));
    cam.set(CV_CAP_PROP_WHITE_BALANCE_BLUE_U, parser.get<double>("wb"));

    cout<<"Connecting to camera"<<endl;
    if ( !cam.open() ) {
        cerr<<"Error opening camera"<<endl;
        return -1;
    }
    cout<<"Connected to camera ="<<cam.getId() <<endl;

    namedWindow(display_original, WINDOW_AUTOSIZE);
    namedWindow(display_result, WINDOW_AUTOSIZE);

    Point prev_center(0,0);
    Rect move_rect(-5,-5,10,10);
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
        Rect rRoi(60, 220, 330, 250);
        Mat mRoi(mCap, rRoi);

        //
        Mat mMask;
        inRange(mRoi, Scalar(white_threshold, white_threshold, white_threshold), Scalar(255, 255, 255), mMask);
        //imshow(display_result, mMask);
        
        Rect box = boundingRect(mMask);

        // Validate size of detected box
        if(box.height > 10
        && box.height < 100
        && box.width  > 10
        && box.width  < 100) {
            // Valid box found
            Point center(box.x+box.width, box.y+box.height/2);
            Rect frame(center.x+frame_xoff, center.y+frame_yoff - frame_height/2, frame_width, frame_height);

            if((center - prev_center).inside(move_rect)) {
                // Frame stopped
                if(state == 1) {
                    // Frame was moving, this must be the new frame:
                    // - Take the shot!
                    imshow(display_result, Mat(mRoi, frame));
                }
                state = 2;
            }
            else {
                // Frame moving
                state = 1;
            }
            prev_center = center;

            // Draw mRoi
            rectangle(mCap, rRoi, Scalar(255, 0, 0), 1);
            
            // Draw in mRoi
            rectangle(mRoi, box, Scalar(255, 0, 0), (state == 2) ? -1 : 1);
            circle(mRoi, center, 10, Scalar(255, 0, 0), 1);
            rectangle(mRoi, frame, Scalar(0, 255, 0), 1);
        }
        else {
            // Invalidate the center, frame is moving
            prev_center = Point(0, 0);
            state = 1;
        }

        imshow(display_original, mCap);
        if (waitKey(1) == 27)
            break;
    }
    
    cam.release();

    return 0;
}
