#pragma once

#include <cstdint>
#include <vector>

#include "esphome/components/inspire_remote/inspire_remote.h"

namespace esphome::inspire_remote::testing {

class CapturingTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  CapturingTransmitter() : RemoteTransmitterBase(nullptr) {}

  std::vector<remote_base::RawTimings> frames;
  std::vector<uint32_t> carrier_frequencies;

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override {
    this->frames.push_back(this->temp_.get_data());
    this->carrier_frequencies.push_back(this->temp_.get_carrier_frequency());
  }
};

// A receiver merges consecutive periods at the same level and can omit the final idle period.
inline remote_base::RawTimings as_received(const remote_base::RawTimings &timings, bool drop_tail = false) {
  remote_base::RawTimings merged;
  for (int32_t item : timings) {
    if (merged.empty() && item < 0)
      continue;
    if (!merged.empty() && (merged.back() < 0) == (item < 0)) {
      merged.back() += item;
    } else {
      merged.push_back(item);
    }
  }
  if (drop_tail && !merged.empty() && merged.back() < 0)
    merged.pop_back();
  return merged;
}

inline void append_frame(remote_base::RawTimings &timings, uint16_t frame, uint32_t trailing_space) {
  for (uint8_t bit = 0; bit < INSPIRE_REMOTE_FRAME_SIZE; bit++) {
    const bool one = (frame & (1U << (INSPIRE_REMOTE_FRAME_SIZE - bit - 1))) != 0;
    timings.push_back(one ? INSPIRE_REMOTE_ONE_MARK : INSPIRE_REMOTE_ZERO_MARK);
    timings.push_back(-static_cast<int32_t>(one ? INSPIRE_REMOTE_ONE_SPACE : INSPIRE_REMOTE_ZERO_SPACE));
  }
  timings.push_back(-static_cast<int32_t>(trailing_space));
}

inline remote_base::RawTimings make_transmission(uint8_t code, uint8_t prefix = INSPIRE_REMOTE_FIXED,
                                                 uint8_t header1 = INSPIRE_REMOTE_HEADER1,
                                                 uint8_t header2 = INSPIRE_REMOTE_HEADER2) {
  remote_base::RawTimings timings;
  const uint16_t fixed = prefix << 8;
  append_frame(timings, fixed | header1, INSPIRE_REMOTE_MESSAGE_SPACE);
  append_frame(timings, fixed | header2, INSPIRE_REMOTE_MESSAGE_SPACE);
  for (uint8_t repeat = 0; repeat < INSPIRE_REMOTE_REPEATS; repeat++) {
    append_frame(timings, fixed | code,
                 repeat + 1 == INSPIRE_REMOTE_REPEATS ? INSPIRE_REMOTE_END_SPACE : INSPIRE_REMOTE_MESSAGE_SPACE);
  }
  return as_received(timings);
}

inline bool receive(InspireRemote &remote, const remote_base::RawTimings &timings, uint32_t tolerance = 10) {
  remote_base::RemoteReceiveData data(timings, tolerance, remote_base::TOLERANCE_MODE_PERCENTAGE);
  return static_cast<remote_base::RemoteReceiverListener &>(remote).on_receive(data);
}

}  // namespace esphome::inspire_remote::testing
