#include "inspire_light.h"

namespace esphome::inspire_remote {

light::LightTraits InspireLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::ON_OFF});
  return traits;
}

void InspireLight::setup_state(light::LightState *state) { this->parent_->set_light(state); }

void InspireLight::update_state(light::LightState *state) {
  // Only the first update is the restored state. If an on_boot command follows it before the first
  // write, write_state() compares that newer request with the restored state and transmits if needed.
  if (this->initial_update_seen_)
    return;
  this->initial_update_seen_ = true;

  bool state_value;
  state->current_values_as_binary(&state_value);
  this->parent_->set_restored_light_state(state_value);
}

void InspireLight::write_state(light::LightState *state) {
  bool state_value;
  state->current_values_as_binary(&state_value);
  this->parent_->write_light_state(state_value);
}

}  // namespace esphome::inspire_remote
