#pragma once

#include "../inspire_remote.h"
#include "esphome/core/component.h"
#include "esphome/components/fan/fan.h"

namespace esphome {
namespace inspire_remote {

class InspireFan : public fan::Fan, public Component, public Parented<InspireRemote> {
 public:
  InspireFan() = default;
  void setup() override;
  void dump_config() override;
  void set_speed_count(int count) { this->speed_count_ = count; }
  fan::FanTraits get_traits() override { return this->traits_; }

 protected:
  void control(const fan::FanCall &call) override;
  void transmit_state();

  bool has_oscillating_{false};
  bool has_direction_{false};
  int speed_count_{0};
  fan::FanTraits traits_;
  std::set<std::string> preset_modes_{};
};

}  // namespace inspire_remote
}  // namespace esphome
