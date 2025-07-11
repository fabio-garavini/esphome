#pragma once

#include "esphome/core/component.h"
#include "esphome/components/remote_base/remote_base.h"
#ifdef USE_LIGHT
#include "esphome/components/light/light_state.h"
#endif
#ifdef USE_FAN
#include "esphome/components/fan/fan.h"
#endif

namespace esphome {
namespace inspire_remote {

// Commands
const uint8_t INSPIRE_REMOTE_FIXED = 0xc;
const uint8_t INSPIRE_REMOTE_HEADER1 = 0x0;
const uint8_t INSPIRE_REMOTE_HEADER2 = 0x7f;
const uint8_t INSPIRE_REMOTE_HIGH = 0x1;
const uint8_t INSPIRE_REMOTE_MEDIUM = 0x4;
const uint8_t INSPIRE_REMOTE_LOW = 0x43;
const uint8_t INSPIRE_REMOTE_OFF = 0x10;
const uint8_t INSPIRE_REMOTE_LIGHT = 0x8;
const uint8_t INSPIRE_REMOTE_2H = 0x20;
const uint8_t INSPIRE_REMOTE_4H = 0x46;
const uint8_t INSPIRE_REMOTE_8H = 0x2;

// IR Timings
const uint32_t INSPIRE_REMOTE_ONE_MARK = 1340;
const uint32_t INSPIRE_REMOTE_ZERO_MARK = 460;
const uint32_t INSPIRE_REMOTE_ONE_SPACE = 400;
const uint32_t INSPIRE_REMOTE_ZERO_SPACE = 1200;
const uint32_t INSPIRE_REMOTE_MESSAGE_SPACE = 7000;
const uint32_t INSPIRE_REMOTE_END_SPACE = 100000;

const uint32_t INSPIRE_REMOTE_IR_FREQUENCY = 38000;
const uint32_t INSPIRE_REMOTE_FRAME_SIZE = 12;

class InspireRemote : public Component,
                      public remote_base::RemoteReceiverListener,
                      public remote_base::RemoteTransmittable {
 public:
  InspireRemote() {}
  void transmit_code(uint8_t code);
  unsigned long get_last_received_time() const { return this->last_received_time_; }
#ifdef USE_LIGHT
  void set_light(light::LightState *l) { this->light_ = l; }
  void toggle_light() { this->transmit_code(INSPIRE_REMOTE_LIGHT); };
#endif
#ifdef USE_FAN
  void set_fan(fan::Fan *f) { this->fan_ = f; }
#endif

 protected:
#ifdef USE_LIGHT
  light::LightState *light_{nullptr};
#endif
#ifdef USE_FAN
  fan::Fan *fan_{nullptr};
#endif
  unsigned long last_sent_time_{0};
  unsigned long last_received_time_{0};
  bool parse_code_(uint8_t code);
  bool on_receive(remote_base::RemoteReceiveData data) override;
  void encode_ir_data_(remote_base::RemoteTransmitData *data, uint16_t code);
  bool expect_code_(remote_base::RemoteReceiveData &data, uint16_t expected_code);
  uint8_t receive_code_(remote_base::RemoteReceiveData &data);
};

}  // namespace inspire_remote
}  // namespace esphome
