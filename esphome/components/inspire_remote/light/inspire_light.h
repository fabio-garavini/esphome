#pragma once

#include "../inspire_remote.h"
#include "esphome/components/light/light_output.h"

namespace esphome::inspire_remote {

class InspireLight final : public light::LightOutput, public Parented<InspireRemote> {
 public:
  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override;
  void update_state(light::LightState *state) override;
  void write_state(light::LightState *state) override;

 protected:
  bool initial_update_seen_{false};
};

}  // namespace esphome::inspire_remote
