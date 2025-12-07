#include "inspire_fan.h"

namespace esphome {
namespace inspire_remote {

static const char *const TAG = "inspire_remote.fan";

void InspireFan::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(*this);
  }

  // Construct traits
  this->traits_ =
      fan::FanTraits(this->has_oscillating_, this->speed_count_ > 0, this->has_direction_, this->speed_count_);
}

void InspireFan::dump_config() { LOG_FAN("", "Inspire Fan", this); }

void InspireFan::control(const fan::FanCall &call) {
  if (call.get_state().has_value())
    this->state = *call.get_state();
  if (call.get_speed().has_value() && (this->speed_count_ > 0))
    this->speed = *call.get_speed();

  this->transmit_state();
  this->publish_state();
}

void InspireFan::transmit_state() {
  uint8_t message;

  if (this->state) {
    switch (this->speed) {
      case 0:
        message = INSPIRE_REMOTE_OFF;
        break;
      case 1:
        message = INSPIRE_REMOTE_LOW;
        break;
      case 2:
        message = INSPIRE_REMOTE_MEDIUM;
        break;
      case 3:
        message = INSPIRE_REMOTE_HIGH;
        break;
      default:
        message = INSPIRE_REMOTE_MEDIUM;
        break;
    }
  } else {
    message = INSPIRE_REMOTE_OFF;
  }

  this->parent_->transmit_code(message);
}

}  // namespace inspire_remote
}  // namespace esphome
