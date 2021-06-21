#pragma once

#include <cstdint>
#include <cstdlib>

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#define CL_CHECK(_expr)          \
  do {                           \
    assert(CL_SUCCESS == _expr); \
  } while (0)

#define CL_CHECK_ERR(_expr)           \
  ({                                  \
    cl_int err = CL_INVALID_VALUE;    \
    __typeof__(_expr) _ret = _expr;   \
    assert(_ret&& err == CL_SUCCESS); \
    _ret;                             \
  })

cl_device_id cl_get_device_id(cl_device_type device_type);
cl_program cl_program_from_file(cl_context ctx, cl_device_id device_id, const char* path, const char* args);
const char* cl_get_error_string(int err);

class CLContext {
public:
  static void init(cl_device_type device_type, cl_context_properties* props = nullptr);
  static inline cl_device_id deviceId() { return def_ctx_.device_id_; }
  static inline cl_context context() { return def_ctx_.context_; }
  static inline cl_program buildProgramFromFile(const char* path, const char* args) {
    return cl_program_from_file(def_ctx_.context_, def_ctx_.device_id_, path, args);
  }

protected:
  CLContext() {}
  ~CLContext();
  cl_device_id device_id_ = 0;
  cl_context context_ = 0;
  static CLContext def_ctx_;
};
