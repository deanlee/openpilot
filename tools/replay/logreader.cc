#include "tools/replay/logreader.h"

#include <algorithm>
#include "tools/replay/filereader.h"
#include "tools/replay/util.h"

LogReader::LogReader(size_t memory_pool_block_size) {
  events.reserve(memory_pool_block_size);
}

#include <QDebug>
#include "common/timing.h"
double dd = 0;
bool LogReader::load(const std::string &url, std::atomic<bool> *abort, bool local_cache, int chunk_size, int retries) {
  dd = millis_since_boot();
  raw_ = FileReader(local_cache, chunk_size, retries).read(url, abort);
  if (raw_.empty()) return false;

  if (url.find(".bz2") != std::string::npos) {
    raw_ = decompressBZ2(raw_, abort);
    if (raw_.empty()) return false;
  }
  return parse(abort);
}

bool LogReader::load(const std::byte *data, size_t size, std::atomic<bool> *abort) {
  raw_.assign((const char *)data, size);
  return parse(abort);
}

bool LogReader::parse(std::atomic<bool> *abort) {
  double t1 = millis_since_boot();
  try {
    kj::ArrayPtr<const capnp::word> words((const capnp::word *)raw_.data(), raw_.size() / sizeof(capnp::word));
    while (words.size() > 0 && !(abort && *abort)) {
      capnp::FlatArrayMessageReader reader(words);
      auto event = reader.getRoot<cereal::Event>();
      auto which = event.which();
      uint64_t mono_time = event.getLogMonoTime();
      auto event_data = kj::arrayPtr(words.begin(), reader.getEnd());

      const Event &evt = events.emplace_back(which, mono_time, event_data);
      // Add encodeIdx packet again as a frame packet for the video stream
      if (evt.which == cereal::Event::ROAD_ENCODE_IDX ||
          evt.which == cereal::Event::DRIVER_ENCODE_IDX ||
          evt.which == cereal::Event::WIDE_ROAD_ENCODE_IDX) {
        auto idx = capnp::AnyStruct::Reader(event).getPointerSection()[0].getAs<cereal::EncodeIndex>();
        if (uint64_t sof = idx.getTimestampSof()) {
          mono_time = sof;
        }
        events.emplace_back(which, mono_time, event_data, idx.getSegmentNum());
      }

      words = kj::arrayPtr(reader.getEnd(), words.end());
    }
  } catch (const kj::Exception &e) {
    rWarning("Failed to parse log : %s.\nRetrieved %zu events from corrupt log", e.getDescription().cStr(), events.size());
  }

  if (!events.empty() && !(abort && *abort)) {
    std::sort(events.begin(), events.end());
    // qDebug() << events.capacity() << events.size() << DEFAULT_EVENT_MEMORY_POOL_BLOCK_SIZE*sizeof(Event) << sizeof(Event) << sizeof(Event *);
    qDebug() << "load time" << millis_since_boot() - t1 << "total" << millis_since_boot() - dd;
    return true;
  }
  return false;
}
