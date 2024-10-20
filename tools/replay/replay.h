#pragma once

#include <QThread>
#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "tools/replay/camera.h"
#include "tools/replay/segment_manager.h"

#define DEMO_ROUTE "a2a0ccea32023010|2023-07-27--13-01-19"

typedef bool (*replayEventFilter)(const Event *, void *);
Q_DECLARE_METATYPE(std::shared_ptr<LogReader>);

class Replay : public QObject {
  Q_OBJECT

 public:
  Replay(const std::string &route, std::vector<std::string> allow, std::vector<std::string> block, SubMaster *sm = nullptr,
         uint32_t flags = REPLAY_FLAG_NONE, const std::string &data_dir = "", QObject *parent = 0);
  ~Replay();
  bool load();
  void start(int seconds = 0);
  void stop();
  void pause(bool pause);
  void seekToFlag(FindFlag flag);
  void seekTo(double seconds, bool relative);
  inline bool isPaused() const { return user_paused_; }
  const SegmentManager &getSegmentManager() const { return segment_manager_; }
  // the filter is called in streaming thread.try to return quickly from it to avoid blocking streaming.
  // the filter function must return true if the event should be filtered.
  // otherwise it must return false.
  inline void installEventFilter(replayEventFilter filter, void *opaque) {
    filter_opaque = opaque;
    event_filter = filter;
  }
  inline bool hasFlag(REPLAY_FLAGS flag) const { return flags_ & flag; }
  inline void addFlag(REPLAY_FLAGS flag) { flags_ |= flag; }
  inline void removeFlag(REPLAY_FLAGS flag) { flags_ &= ~flag; }
  inline double currentSeconds() const { return double(cur_mono_time_ - segment_manager_.route_start_ts_) / 1e9; }
  inline QDateTime currentDateTime() const { return segment_manager_.route_date_time_.addSecs(currentSeconds()); }
  inline void setSpeed(float speed) { speed_ = speed; }
  inline float getSpeed() const { return speed_; }
  inline const std::vector<Event> *events() const { return &events_; }

 signals:
  void streamStarted();
  void segmentsMerged();
  void seeking(double sec);
  void seekedTo(double sec);
  void qLogLoaded(std::shared_ptr<LogReader> qlog);
  void minMaxTimeChanged(double min_sec, double max_sec);

 protected:
  void pauseStreamThread();
  void startStream(const Segment *cur_segment);
  void streamThread();
  void updateEvents(const std::function<bool()> &update_events_function);
  std::vector<Event>::const_iterator publishEvents(std::vector<Event>::const_iterator first,
                                                   std::vector<Event>::const_iterator last);
  void publishMessage(const Event *e);
  void publishFrame(const Event *e);
  void checkSeekProgress();

  SegmentManager segment_manager_;

  pthread_t stream_thread_id = 0;
  QThread *stream_thread_ = nullptr;
  std::mutex stream_lock_;
  bool user_paused_ = false;
  std::condition_variable stream_cv_;
  std::optional<double> seeking_to_;

  // the following variables must be protected with stream_lock_
  std::atomic<bool> exit_ = false;
  std::atomic<bool> paused_ = false;
  bool events_ready_ = false;

  std::atomic<uint64_t> cur_mono_time_ = 0;
  std::vector<Event> events_;

  // messaging
  SubMaster *sm = nullptr;
  std::unique_ptr<PubMaster> pm;
  std::vector<const char *> sockets_;
  std::unique_ptr<CameraServer> camera_server_;
  std::atomic<uint32_t> flags_ = REPLAY_FLAG_NONE;

  std::atomic<float> speed_ = 1.0;
  replayEventFilter event_filter = nullptr;
  void *filter_opaque = nullptr;
};
