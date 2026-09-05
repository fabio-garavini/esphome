#pragma once

#include "../inspire_remote.h"
#include "esphome/components/light/light_output.h"

namespace esphome::inspire_remote {

class InspireLight : public light::LightOutput, public Parented<InspireRemote> {
 public:
  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;

 protected:
  bool initial_state_written_{false};
};

}  // namespace esphome::inspire_remote
