#include <stdio.h>
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

//parse command line
//returns the index of a command line param in argv. If not found, return -1
int findParam ( string param,int argc,char **argv ) {
    int idx=-1;
    for ( int i=0; i<argc && idx==-1; i++ )
        if ( string ( argv[i] ) ==param ) idx=i;
    return idx;

}
//parse command line
//returns the value of a command line param. If not found, defvalue is returned
float getParamVal ( string param,int argc,char **argv,float defvalue=-1 ) {
    int idx=-1;
    for ( int i=0; i<argc && idx==-1; i++ )
        if ( string ( argv[i] ) ==param ) idx=i;
    if ( idx==-1 ) return defvalue;
    else return atof ( argv[  idx+1] );
}

void processCommandLine ( int argc,char **argv,raspicam::RaspiCam_Cv &Camera ) {
    Camera.set ( CV_CAP_PROP_FRAME_WIDTH,  getParamVal ( "-w",argc,argv,1280 ) );
    Camera.set ( CV_CAP_PROP_FRAME_HEIGHT, getParamVal ( "-h",argc,argv,960 ) );
    Camera.set ( CV_CAP_PROP_BRIGHTNESS,getParamVal ( "-br",argc,argv,50 ) );
    Camera.set ( CV_CAP_PROP_CONTRAST ,getParamVal ( "-co",argc,argv,50 ) );
    Camera.set ( CV_CAP_PROP_SATURATION, getParamVal ( "-sa",argc,argv,50 ) );
    Camera.set ( CV_CAP_PROP_GAIN, getParamVal ( "-g",argc,argv ,50 ) );
    if ( findParam ( "-gr",argc,argv ) !=-1 )
        Camera.set ( CV_CAP_PROP_FORMAT, CV_8UC1 );
    if ( findParam ( "-ss",argc,argv ) !=-1 )
        Camera.set ( CV_CAP_PROP_EXPOSURE, getParamVal ( "-ss",argc,argv )  );
    if ( findParam ( "-wb_r",argc,argv ) !=-1 )
        Camera.set ( CV_CAP_PROP_WHITE_BALANCE_RED_V,getParamVal ( "-wb_r",argc,argv )     );
    if ( findParam ( "-wb_b",argc,argv ) !=-1 )
        Camera.set ( CV_CAP_PROP_WHITE_BALANCE_BLUE_U,getParamVal ( "-wb_b",argc,argv )     );


//     Camera.setSharpness ( getParamVal ( "-sh",argc,argv,0 ) );
//     if ( findParam ( "-vs",argc,argv ) !=-1 )
//         Camera.setVideoStabilization ( true );
//     Camera.setExposureCompensation ( getParamVal ( "-ev",argc,argv ,0 ) );


}

void showUsage() {
    cout<<"Usage: "<<endl;
    cout<<"[-gr set gray color capture]\n";
    cout<<"[-test_speed use for test speed and no images will be saved]\n";
    cout<<"[-w width] [-h height] \n[-br brightness_val(0,100)]\n";
    cout<<"[-co contrast_val (0 to 100)]\n[-sa saturation_val (0 to 100)]";
    cout<<"[-g gain_val  (0 to 100)]\n";
    cout<<"[-ss shutter_speed (0 to 100) 0 auto]\n";
    cout<<"[-wb_r val  (0 to 100),0 auto: white balance red component]\n";
    cout<<"[-wb_b val  (0 to 100),0 auto: white balance blue component]\n";

    cout<<endl;
}

int main(int argc, char** argv )
{
    if ( argc==1 ) {
        cerr<<"Usage (-help for help)"<<endl;
    }
    if ( findParam ( "-help",argc,argv ) !=-1 ) {
        showUsage();
        return -1;
    }

    raspicam::RaspiCam_Cv Camera;
    processCommandLine ( argc,argv,Camera );
    cout<<"Connecting to camera"<<endl;
    if ( !Camera.open() ) {
        cerr<<"Error opening camera"<<endl;
        return -1;
    }
    cout<<"Connected to camera ="<<Camera.getId() <<endl;

    namedWindow(display_original, WINDOW_AUTOSIZE);
    namedWindow(display_result, WINDOW_AUTOSIZE);

    Point prev_center(0,0);
    Rect move_rect(-5,-5,10,10);
    int state = 0; // 0=unknown, 1=moving, 2=stopped
    while(1) {
        // Capture raw image from camera
        Mat mCap;
        Camera.grab();
        Camera.retrieve(mCap);

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
    
    Camera.release();

    return 0;
}
