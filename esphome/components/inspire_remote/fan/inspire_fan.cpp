#include "inspire_fan.h"

#include "esphome/core/log.h"

namespace esphome::inspire_remote {

ESPHOME_LOG_TAG(TAG, "inspire_remote.fan");

void InspireFan::setup() {
  auto restore = this->restore_state_();
  if (!restore.has_value())
    return;

  // Fan commands are absolute, so replaying restore is safe and applies ALWAYS_ON/OFF to the device.
  // Restore data is not validated by FanRestoreState::apply(); normalize a missing or stale speed.
  this->state = restore->state;
  this->speed = restore->speed == 0 ? this->speed_count_ : restore->speed;
  this->speed = clamp(this->speed, 1, static_cast<int>(this->speed_count_));
  this->transmit_state_();
  this->publish_state();
}

void InspireFan::dump_config() { LOG_FAN("", "Inspire Fan", this); }

void InspireFan::control(const fan::FanCall &call) {
  if (call.get_state().has_value())
    this->state = *call.get_state();
  if (call.get_speed().has_value())
    this->speed = *call.get_speed();
  if (this->state)
    this->speed = clamp(this->speed, 1, static_cast<int>(this->speed_count_));

  this->transmit_state_();
  this->publish_state();
}

void InspireFan::transmit_state_() {
  uint8_t code;
  if (!this->state) {
    code = INSPIRE_REMOTE_OFF;
  } else if (this->speed >= 3) {
    code = INSPIRE_REMOTE_HIGH;
  } else if (this->speed == 2) {
    code = INSPIRE_REMOTE_MEDIUM;
  } else {
    code = INSPIRE_REMOTE_LOW;
  }
  this->parent_->transmit_code(code);
}

}  // namespace esphome::inspire_remote
