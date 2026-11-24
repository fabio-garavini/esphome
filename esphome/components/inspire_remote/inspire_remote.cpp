#include "inspire_remote.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::inspire_remote {

ESPHOME_LOG_TAG(TAG, "inspire_remote");

namespace {

// A frame contains 11 mark/space pairs, a final mark and the gap that follows it. Folding the final
// bit space into that gap produces the same waveform as two consecutive spaces without redundant entries.
void encode_frame(remote_base::RemoteTransmitData *data, uint16_t frame, uint32_t trailing_space) {
  for (uint8_t bit = 0; bit < INSPIRE_REMOTE_FRAME_SIZE; bit++) {
    const bool one = (frame & (1U << (INSPIRE_REMOTE_FRAME_SIZE - bit - 1))) != 0;
    data->mark(one ? INSPIRE_REMOTE_ONE_MARK : INSPIRE_REMOTE_ZERO_MARK);
    const uint32_t bit_space = one ? INSPIRE_REMOTE_ONE_SPACE : INSPIRE_REMOTE_ZERO_SPACE;
    data->space(bit + 1 == INSPIRE_REMOTE_FRAME_SIZE ? trailing_space + bit_space : bit_space);
  }
}

bool decode_frame(remote_base::RemoteReceiveData &data, uint16_t &frame) {
  frame = 0;
  for (uint8_t bit = 0; bit < INSPIRE_REMOTE_FRAME_SIZE; bit++) {
    bool one;
    if (data.expect_mark(INSPIRE_REMOTE_ONE_MARK)) {
      one = true;
    } else if (data.expect_mark(INSPIRE_REMOTE_ZERO_MARK)) {
      one = false;
    } else {
      return false;
    }

    frame |= static_cast<uint16_t>(one) << (INSPIRE_REMOTE_FRAME_SIZE - bit - 1);
    if (bit + 1 != INSPIRE_REMOTE_FRAME_SIZE &&
        !data.expect_space(one ? INSPIRE_REMOTE_ONE_SPACE : INSPIRE_REMOTE_ZERO_SPACE)) {
      return false;
    }
  }
  return true;
}

bool expect_frame(remote_base::RemoteReceiveData &data, uint16_t expected, uint32_t trailing_space) {
  uint16_t frame;
  if (!decode_frame(data, frame) || frame != expected)
    return false;
  const uint32_t bit_space = (frame & 1) != 0 ? INSPIRE_REMOTE_ONE_SPACE : INSPIRE_REMOTE_ZERO_SPACE;
  return data.expect_space(trailing_space + bit_space);
}

}  // namespace

void InspireRemote::dump_config() { ESP_LOGCONFIG(TAG, "Inspire Remote:"); }

void InspireRemote::transmit_code(uint8_t code) {
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(INSPIRE_REMOTE_IR_FREQUENCY);
  data->reserve((INSPIRE_REMOTE_REPEATS + 2) * INSPIRE_REMOTE_FRAME_SIZE * 2);

  const uint16_t prefix = INSPIRE_REMOTE_FIXED << 8;
  encode_frame(data, prefix | INSPIRE_REMOTE_HEADER1, INSPIRE_REMOTE_MESSAGE_SPACE);
  encode_frame(data, prefix | INSPIRE_REMOTE_HEADER2, INSPIRE_REMOTE_MESSAGE_SPACE);
  for (uint8_t repeat = 0; repeat < INSPIRE_REMOTE_REPEATS; repeat++) {
    const uint32_t trailing_space =
        repeat + 1 == INSPIRE_REMOTE_REPEATS ? INSPIRE_REMOTE_END_SPACE : INSPIRE_REMOTE_MESSAGE_SPACE;
    encode_frame(data, prefix | code, trailing_space);
  }

  ESP_LOGV(TAG, "Transmitting command 0x%02X", code);
  this->last_sent_time_ = millis();
  this->has_transmitted_ = true;
  transmit.perform();
}

bool InspireRemote::on_receive(remote_base::RemoteReceiveData data) {
  if (this->has_transmitted_ && millis() - this->last_sent_time_ < INSPIRE_REMOTE_SELF_ECHO_DELAY)
    return false;

  // A transmission starts with two headers, followed by the repeated command. Decoding the first
  // command is enough to update the entities; its fixed prefix and both headers reject other protocols.
  const uint16_t prefix = INSPIRE_REMOTE_FIXED << 8;
  if (!expect_frame(data, prefix | INSPIRE_REMOTE_HEADER1, INSPIRE_REMOTE_MESSAGE_SPACE) ||
      !expect_frame(data, prefix | INSPIRE_REMOTE_HEADER2, INSPIRE_REMOTE_MESSAGE_SPACE)) {
    return false;
  }

  uint16_t frame;
  if (!decode_frame(data, frame) || (frame >> 8) != INSPIRE_REMOTE_FIXED)
    return false;

  const uint8_t code = frame & 0xFF;
  ESP_LOGV(TAG, "Received command 0x%02X", code);
  return this->parse_code_(code);
}

bool InspireRemote::parse_code_(uint8_t code) {
  switch (code) {
    case INSPIRE_REMOTE_HIGH:
    case INSPIRE_REMOTE_MEDIUM:
    case INSPIRE_REMOTE_LOW: {
#ifdef USE_FAN
      const uint8_t speed = code == INSPIRE_REMOTE_HIGH ? 3 : (code == INSPIRE_REMOTE_MEDIUM ? 2 : 1);
      this->update_fan_(true, speed);
#endif
      return true;
    }
    case INSPIRE_REMOTE_OFF:
#ifdef USE_FAN
      this->update_fan_(false);
#endif
      return true;
    case INSPIRE_REMOTE_LIGHT:
#ifdef USE_LIGHT
      this->toggle_light_state_();
#endif
      return true;
    case INSPIRE_REMOTE_2H:
    case INSPIRE_REMOTE_4H:
    case INSPIRE_REMOTE_8H:
      // Timer commands do not immediately change the fan or light state.
      return true;
    default:
      ESP_LOGV(TAG, "Unknown command 0x%02X", code);
      return false;
  }
}

#ifdef USE_LIGHT
void InspireRemote::set_restored_light_state(bool state) {
  this->assumed_light_state_ = state;
  this->light_state_known_ = true;
}

void InspireRemote::write_light_state(bool state) {
  // A toggle-only light must not be commanded for an idempotent write. The restored state seeds our
  // assumption about the physical light without transmitting, as replaying it could toggle the light.
  if (!this->light_state_known_) {
    this->set_restored_light_state(state);
    return;
  }
  if (state == this->assumed_light_state_)
    return;

  this->assumed_light_state_ = state;
  this->transmit_code(INSPIRE_REMOTE_LIGHT);
}

void InspireRemote::toggle_light_state_() {
  if (this->light_ == nullptr)
    return;

  const bool state = this->light_state_known_ ? !this->assumed_light_state_ : !this->light_->remote_values.is_on();
  this->assumed_light_state_ = state;
  this->light_state_known_ = true;
  auto call = this->light_->make_call();
  call.set_state(state);
  call.perform();
}
#endif

#ifdef USE_FAN
void InspireRemote::update_fan_(bool state, uint8_t speed) {
  if (this->fan_ == nullptr)
    return;

  bool changed = this->fan_->state != state;
  if (state) {
    const int new_speed = std::min<int>(speed, this->fan_->get_traits().supported_speed_count());
    changed |= this->fan_->speed != new_speed;
    this->fan_->speed = new_speed;
  }
  this->fan_->state = state;
  if (changed)
    this->fan_->publish_state();
}
#endif

}  // namespace esphome::inspire_remote
