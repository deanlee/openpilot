#include "selfdrive/ui/qt/onroad/road_camera.h"


RoadCameraView::RoadCameraView(std::string stream_name, VisionStreamType stream_type, QWidget* parent)
: CameraView(stream_name, stream_type, parent) {

}

