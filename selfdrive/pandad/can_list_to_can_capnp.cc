#include "cereal/messaging/messaging.h"
#include "selfdrive/pandad/panda.h"
#include "opendbc/can/common.h"
#include "Python.h"

void can_list_to_can_capnp_cpp(const std::vector<can_frame> &can_list, std::string &out, bool sendcan, bool valid) {
  MessageBuilder msg;
  auto event = msg.initEvent(valid);

  auto canData = sendcan ? event.initSendcan(can_list.size()) : event.initCan(can_list.size());
  int j = 0;
  for (auto it = can_list.begin(); it != can_list.end(); it++, j++) {
    auto c = canData[j];
    c.setAddress(it->address);
    c.setDat(kj::arrayPtr((uint8_t*)it->dat.data(), it->dat.size()));
    c.setSrc(it->src);
  }
  const uint64_t msg_size = capnp::computeSerializedSizeInWords(msg) * sizeof(capnp::word);
  out.resize(msg_size);
  kj::ArrayOutputStream output_stream(kj::ArrayPtr<capnp::byte>((unsigned char *)out.data(), msg_size));
  capnp::writeMessage(output_stream, msg);
}

// Converts a vector of Cap'n Proto serialized can strings into a vector of CanData structures.
PyObject* can_capnp_to_can_list_cpp(const std::vector<std::string> &strings, bool sendcan) {
  AlignedBuffer aligned_buf;

  PyObject* result_list = PyList_New(0);
  for (const auto &str : strings) {
    // extract the messages
    capnp::FlatArrayMessageReader reader(aligned_buf.align(str.data(), str.size()));
    cereal::Event::Reader event = reader.getRoot<cereal::Event>();

    auto frames = sendcan ? event.getSendcan() : event.getCan();

    PyObject* frame_list = PyList_New(0);
    // Populate CAN frames
    for (const auto &frame : frames) {
      // Create a tuple for each frame (address, data, src)
      PyObject* address = PyLong_FromUnsignedLong(frame.getAddress());
      PyObject* data_bytes = PyBytes_FromStringAndSize((const char*)frame.getDat().begin(), frame.getDat().size());
      PyObject* src = PyLong_FromUnsignedLong(frame.getSrc());

      // Create a tuple (address, data, src) and add it to frame_list
      PyObject* frame_tuple = PyTuple_Pack(3, address, data_bytes, src);
      PyList_Append(frame_list, frame_tuple);
      // Decrease reference counts
      Py_DECREF(address);
      Py_DECREF(data_bytes);
      Py_DECREF(src);
      Py_DECREF(frame_tuple);
    }

     // Create a tuple for each CanData (nanos, frame_list) and add it to result_list
    PyObject* nanos = PyLong_FromUnsignedLongLong(event.getLogMonoTime());
    PyObject* data_tuple = PyTuple_Pack(2, nanos, frame_list);
    PyList_Append(result_list, data_tuple);

    // Decrease reference counts
    Py_DECREF(nanos);
    Py_DECREF(frame_list);
    Py_DECREF(data_tuple);
  }
  return result_list;
}
