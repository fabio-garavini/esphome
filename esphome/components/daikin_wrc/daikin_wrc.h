#pragma once

#include <cstdint>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::daikin_wrc {

// Values from Daikin WRC-PHC IR Remote
// Temperature
constexpr uint8_t DAIKIN_WRC_TEMP_MIN = 16;  // Celsius
constexpr uint8_t DAIKIN_WRC_TEMP_MAX = 30;  // Celsius

// Modes
constexpr uint8_t DAIKIN_WRC_MODE_AUTO = 0xa;
constexpr uint8_t DAIKIN_WRC_MODE_COOL = 0x2;
constexpr uint8_t DAIKIN_WRC_MODE_HEAT = 0x8;
constexpr uint8_t DAIKIN_WRC_MODE_DRY = 0x1;
constexpr uint8_t DAIKIN_WRC_MODE_FAN = 0x4;
constexpr uint8_t DAIKIN_WRC_MODE_OFF = 0x0;

// Fan Speed
constexpr uint8_t DAIKIN_WRC_FAN_AUTO = 0x1;
constexpr uint8_t DAIKIN_WRC_FAN_SILENT = 0x9;
constexpr uint8_t DAIKIN_WRC_FAN_TURBO = 0x3;
constexpr uint8_t DAIKIN_WRC_FAN_LOW = 0x8;
constexpr uint8_t DAIKIN_WRC_FAN_MEDIUM = 0x4;
constexpr uint8_t DAIKIN_WRC_FAN_HIGH = 0x2;

// Frame header nibbles
constexpr uint8_t DAIKIN_WRC_FRAME_HEADER_1 = 0x6;
constexpr uint8_t DAIKIN_WRC_FRAME_HEADER_2 = 0x1;

// IR Transmission
constexpr uint32_t DAIKIN_WRC_IR_FREQUENCY = 38000;
constexpr uint32_t DAIKIN_WRC_HEADER_MARK = 9800;
constexpr uint32_t DAIKIN_WRC_HEADER_SPACE = 9700;
constexpr uint32_t DAIKIN_WRC_HDR_MSG_MARK = 4700;
constexpr uint32_t DAIKIN_WRC_HDR_MSG_SPACE = 2400;
constexpr uint32_t DAIKIN_WRC_BIT_MARK = 400;
constexpr uint32_t DAIKIN_WRC_ONE_SPACE = 800;
constexpr uint32_t DAIKIN_WRC_ZERO_SPACE = 300;
constexpr uint32_t DAIKIN_WRC_MESSAGE_SPACE = 20000;
constexpr uint32_t DAIKIN_WRC_END_SPACE = 100000;

// Ignore IR data received shortly after transmitting our own command
constexpr uint32_t DAIKIN_WRC_SELF_ECHO_GUARD = 500;  // ms

// State Frame size
constexpr uint8_t DAIKIN_WRC_STATE_FRAME_SIZE = 16;

class DaikinWrcClimate : public climate_ir::ClimateIR {
 public:
  void set_state_sensor(binary_sensor::BinarySensor *bs) { this->state_sensor_ = bs; }

  DaikinWrcClimate()
      : climate_ir::ClimateIR(
            DAIKIN_WRC_TEMP_MIN, DAIKIN_WRC_TEMP_MAX, 1.0f, true, true,
            {climate::CLIMATE_FAN_QUIET, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW,
             climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH},
            {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL},
            {climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_SLEEP, climate::CLIMATE_PRESET_BOOST}) {}

  void setup() override;

 protected:
  uint32_t last_sent_time_{0};
  binary_sensor::BinarySensor *state_sensor_{nullptr};
  climate::ClimateMode previous_mode_ = climate::CLIMATE_MODE_OFF;

  void transmit_state() override;
  void control(const climate::ClimateCall &call) override;
  uint8_t operation_mode_() const;
  uint8_t fan_speed_() const;
  uint8_t special_flags_() const;
  uint8_t temperature_() const;
  bool on_receive(remote_base::RemoteReceiveData data) override;
  bool read_state_frame_(remote_base::RemoteReceiveData &data, uint8_t frame[]);
  bool parse_state_frame_(const uint8_t frame[]);
};

}  // namespace esphome::daikin_wrc
