#pragma once

#include "../inspire_remote.h"
#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace inspire_remote {

class InspireLight : public light::LightOutput, public Component, public Parented<InspireRemote> {
 public:
  InspireLight() = default;
  void setup_state(light::LightState *state) override;
  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;
};

}  // namespace inspire_remote
}  // namespace esphome
