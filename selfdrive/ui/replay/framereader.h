#pragma once

#include <unistd.h>

#include <string>

#include <QThread>

#include "selfdrive/common/framereader.h"
class QFrameReader : public QObject {
  Q_OBJECT

 public:
  QFrameReader(const std::string &url, QObject *parent = nullptr);
  ~QFrameReader();
  uint8_t *get(int idx) { return frame_reader_->get(idx); };
  int getRGBSize() { return frame_reader_->getRGBSize(); }
  bool valid() const { return frame_reader_->valid(); }

  int width = 0, height = 0;

 signals:
  void finished();

 private:
  void process();

  std::string url_;
  FrameReader *frame_reader_ = nullptr;
  QThread *process_thread_;
};
