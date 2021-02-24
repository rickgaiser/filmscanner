cmake_minimum_required(VERSION 2.8)
project( filmscanner )

find_package( OpenCV REQUIRED )
include_directories( ${OpenCV_INCLUDE_DIRS} )

find_package( raspicam REQUIRED )
include_directories( ${raspicam_INCLUDE_DIRS} )

add_executable( filmscanner main.cpp )

target_link_libraries( filmscanner ${OpenCV_LIBS} )
target_link_libraries( filmscanner ${raspicam_CV_LIBS} )
