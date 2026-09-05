#include "inspire_light.h"

namespace esphome::inspire_remote {

light::LightTraits InspireLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::ON_OFF});
  return traits;
}

void InspireLight::write_state(light::LightState *state) {
  // The LightState is not known at compile time, so capture it on the first call. The first
  // call happens at boot when the restored state is applied; skip transmitting then, otherwise
  // the restore state would toggle the physical light.
  if (!this->initial_state_written_) {
    this->initial_state_written_ = true;
    this->parent_->set_light(state);
    return;
  }

  // Skip transmitting when the state change originated from an IR command, otherwise the
  // light would toggle a second time
  if (millis() - this->parent_->get_last_received_time() > INSPIRE_REMOTE_IR_STATE_DELAY) {
    this->parent_->transmit_code(INSPIRE_REMOTE_LIGHT);
  }
}

}  // namespace esphome::inspire_remote
