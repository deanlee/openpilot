#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "tools/replay/camera.h"
#include "tools/replay/logreader.h"
#include "tools/replay/seg_mgr.h"

class EventStream {
 public:
  EventStream();
  void initialize(SubMaster *sm, uint32_t flags, std::vector<std::string> allow, std::vector<std::string> block);
  void start();
  void seekTo(uint64_t mono_time);
  void setEvents(std::shared_ptr<SegmentManager::Events> events);
  void pause(bool pause);
  void stop();

  pthread_t stream_thread_id = 0;
  inline void setSpeed(float speed) { speed_ = speed; }
  inline float getSpeed() const { return speed_; }
  std::thread stream_thread_;
  std::mutex stream_lock_;
  bool user_paused_ = false;
  std::condition_variable stream_cv_;
  std::atomic<int> current_segment_ = 0;
  std::atomic<bool> exit_ = false;
  std::atomic<bool> paused_ = false;
  bool events_ready_ = false;
  std::atomic<uint64_t> cur_mono_time_ = 0;
  SubMaster *sm_ = nullptr;
  std::unique_ptr<PubMaster> pm_;
  std::vector<const char *> sockets_;
  std::unique_ptr<CameraServer> camera_server_;
  std::atomic<float> speed_ = 1.0;
  std::function<bool(const Event *)> event_filter_ = nullptr;
  std::shared_ptr<SegmentManager::Events> events_;
  std::atomic<uint32_t> flags_ = REPLAY_FLAG_NONE;

private:
  void streamThread();
  void pauseStreamThread();
  void publishMessage(const Event *e);
  void publishFrame(const Event *e);
  void updateEvents(const std::function<bool()> &update_events_function);
  std::vector<Event>::const_iterator publishEvents(std::vector<Event>::const_iterator first,
                                                   std::vector<Event>::const_iterator last);

};
