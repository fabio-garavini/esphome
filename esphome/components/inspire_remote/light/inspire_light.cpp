#include "inspire_light.h"
#include "../inspire_remote.h"
#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace inspire_remote {

static const char *const TAG = "inspire_remote.light";

void InspireLight::setup_state(light::LightState *state) { this->parent_->set_light(state); }

light::LightTraits InspireLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::ON_OFF});
  return traits;
}

void InspireLight::write_state(light::LightState *state) {
  bool new_state;
  state->current_values_as_binary(&new_state);

  if (millis() - this->parent_->get_last_received_time() > 500) {
    this->parent_->transmit_code(INSPIRE_REMOTE_LIGHT);
  }
}

}  // namespace inspire_remote
}  // namespace esphome
