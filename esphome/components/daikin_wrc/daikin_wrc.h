#pragma once

#include <cstdint>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::daikin_wrc {

class DaikinWrcClimate final : public climate_ir::ClimateIR {
 public:
  DaikinWrcClimate();

  void setup() override;
  void set_state_sensor(binary_sensor::BinarySensor *sensor) { this->state_sensor_ = sensor; }

 protected:
  static constexpr uint8_t STATE_FRAME_SIZE = 16;

  void control(const climate::ClimateCall &call) override;
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  void build_state_frame_(uint8_t frame[]) const;
  uint8_t operation_mode_() const;
  uint8_t fan_speed_() const;
  uint8_t special_flags_(bool power_toggle) const;
  uint8_t temperature_() const;
  bool read_state_frame_(remote_base::RemoteReceiveData &data, uint8_t frame[]) const;
  bool parse_state_frame_(const uint8_t frame[]);
  void set_power_state_(bool power);

  binary_sensor::BinarySensor *state_sensor_{nullptr};
  uint32_t last_sent_time_{UINT32_MAX};
  climate::ClimateMode last_active_mode_{climate::CLIMATE_MODE_HEAT_COOL};
  bool power_toggle_{false};
};

}  // namespace esphome::daikin_wrc
