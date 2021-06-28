#pragma once

#include <vector>

#include "cereal/messaging/messaging.h"
#include "selfdrive/common/util.h"
#include "selfdrive/modeld/models/commonmodel.h"
#include "selfdrive/modeld/runners/run.h"

#define OUTPUT_SIZE 40

typedef struct DMonitoringModelState {
  RunModel *m;
  bool is_rhd;
  float output[OUTPUT_SIZE];
  std::vector<uint8_t> resized_buf;
  std::vector<uint8_t> cropped_buf;
  std::vector<uint8_t> premirror_cropped_buf;
  std::vector<float> net_input_buf;
} DMonitoringModelState;

void dmonitoring_init(DMonitoringModelState* s);
float dmonitoring_eval_frame(DMonitoringModelState* s, void* stream_buf, int width, int height);
void dmonitoring_publish(PubMaster &pm, uint32_t frame_id, const float *res, float execution_time, float dsp_execution_time);
void dmonitoring_free(DMonitoringModelState* s);

