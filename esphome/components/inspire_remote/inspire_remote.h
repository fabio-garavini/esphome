#pragma once

#include "esphome/core/component.h"
#include "esphome/components/remote_base/remote_base.h"
#ifdef USE_LIGHT
#include "esphome/components/light/light_state.h"
#endif
#ifdef USE_FAN
#include "esphome/components/fan/fan.h"
#endif

namespace esphome::inspire_remote {

// Commands are sent as 12-bit frames with this fixed value in the upper nibble.
inline constexpr uint8_t INSPIRE_REMOTE_FIXED = 0xC;
inline constexpr uint8_t INSPIRE_REMOTE_HEADER1 = 0x00;
inline constexpr uint8_t INSPIRE_REMOTE_HEADER2 = 0x7F;
inline constexpr uint8_t INSPIRE_REMOTE_HIGH = 0x01;
inline constexpr uint8_t INSPIRE_REMOTE_MEDIUM = 0x04;
inline constexpr uint8_t INSPIRE_REMOTE_LOW = 0x43;
inline constexpr uint8_t INSPIRE_REMOTE_OFF = 0x10;
inline constexpr uint8_t INSPIRE_REMOTE_LIGHT = 0x08;
inline constexpr uint8_t INSPIRE_REMOTE_2H = 0x20;
inline constexpr uint8_t INSPIRE_REMOTE_4H = 0x46;
inline constexpr uint8_t INSPIRE_REMOTE_8H = 0x02;

// IR timings in microseconds.
inline constexpr uint32_t INSPIRE_REMOTE_ONE_MARK = 1340;
inline constexpr uint32_t INSPIRE_REMOTE_ZERO_MARK = 460;
inline constexpr uint32_t INSPIRE_REMOTE_ONE_SPACE = 400;
inline constexpr uint32_t INSPIRE_REMOTE_ZERO_SPACE = 1200;
inline constexpr uint32_t INSPIRE_REMOTE_MESSAGE_SPACE = 7000;
inline constexpr uint32_t INSPIRE_REMOTE_END_SPACE = 100000;

inline constexpr uint32_t INSPIRE_REMOTE_IR_FREQUENCY = 38000;
inline constexpr uint8_t INSPIRE_REMOTE_FRAME_SIZE = 12;
// The receiver only accepts a command after it has been repeated this many times.
inline constexpr uint8_t INSPIRE_REMOTE_REPEATS = 8;
// Ignore our own transmission if the IR receiver reports it back.
inline constexpr uint32_t INSPIRE_REMOTE_SELF_ECHO_DELAY = 500;

class InspireRemote final : public Component,
                            public remote_base::RemoteReceiverListener,
                            public remote_base::RemoteTransmittable {
 public:
  void dump_config() override;
  void transmit_code(uint8_t code);

#ifdef USE_LIGHT
  void set_light(light::LightState *light) { this->light_ = light; }
  void set_restored_light_state(bool state);
  void write_light_state(bool state);
#endif
#ifdef USE_FAN
  void set_fan(fan::Fan *fan) { this->fan_ = fan; }
#endif

 protected:
  bool on_receive(remote_base::RemoteReceiveData data) override;
  bool parse_code_(uint8_t code);
#ifdef USE_LIGHT
  void toggle_light_state_();
  light::LightState *light_{nullptr};
  bool assumed_light_state_{false};
  bool light_state_known_{false};
#endif
#ifdef USE_FAN
  void update_fan_(bool state, uint8_t speed = 0);
  fan::Fan *fan_{nullptr};
#endif
  uint32_t last_sent_time_{0};
  bool has_transmitted_{false};
};

}  // namespace esphome::inspire_remote
