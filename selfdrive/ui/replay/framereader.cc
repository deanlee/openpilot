#include "selfdrive/ui/replay/framereader.h"

#include <unistd.h>

#include <QDebug>
#include <cassert>

QFrameReader::QFrameReader(const std::string &url, QObject *parent) : url_(url), QObject(parent) {
  process_thread_ = QThread::create(&QFrameReader::process, this);
  connect(process_thread_, &QThread::finished, process_thread_, &QThread::deleteLater);
  process_thread_->start();
}

QFrameReader::~QFrameReader() {
  delete frame_reader_;
}

void QFrameReader::process() {
  frame_reader_ = new FrameReader(url_);
  if (frame_reader_->process()) {
    width = frame_reader_->width;
    height = frame_reader_->height;
    emit finished();
  }
}
