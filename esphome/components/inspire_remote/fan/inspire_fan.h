#pragma once

#include "../inspire_remote.h"
#include "esphome/components/fan/fan.h"
#include "esphome/core/component.h"

namespace esphome::inspire_remote {

class InspireFan final : public fan::Fan, public Component, public Parented<InspireRemote> {
 public:
  explicit InspireFan(uint8_t speed_count) : traits_(false, true, false, speed_count), speed_count_(speed_count) {}

  void setup() override;
  void dump_config() override;
  fan::FanTraits get_traits() override { return this->traits_; }

 protected:
  void control(const fan::FanCall &call) override;
  void transmit_state_();

  fan::FanTraits traits_;
  uint8_t speed_count_;
};

}  // namespace esphome::inspire_remote
