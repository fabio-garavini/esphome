#include "inspire_remote.h"

#include "esphome/core/log.h"

namespace esphome::inspire_remote {

static const char *const TAG = "inspire_remote";

void InspireRemote::dump_config() { ESP_LOGCONFIG(TAG, "Inspire Remote:"); }

void InspireRemote::toggle_light_state() {
  this->last_received_time_ = millis();
  if (this->light_ == nullptr)
    return;
  auto call = this->light_->make_call();
  call.set_state(!this->light_->current_values.is_on());
  call.perform();
}

void InspireRemote::encode_ir_data_(remote_base::RemoteTransmitData *data, uint16_t code) {
  for (int16_t i = INSPIRE_REMOTE_FRAME_SIZE - 1; i >= 0; i--) {
    if (code & (1 << i)) {
      data->mark(INSPIRE_REMOTE_ONE_MARK);
      data->space(INSPIRE_REMOTE_ONE_SPACE);
    } else {
      data->mark(INSPIRE_REMOTE_ZERO_MARK);
      data->space(INSPIRE_REMOTE_ZERO_SPACE);
    }
  }
}

void InspireRemote::transmit_code(uint8_t code) {
  this->last_sent_time_ = millis();

  auto transmit = this->transmitter_->transmit();
  remote_base::RemoteTransmitData *data = transmit.get_data();
  data->set_carrier_frequency(INSPIRE_REMOTE_IR_FREQUENCY);

  this->encode_ir_data_(data, INSPIRE_REMOTE_HEADER1 | (INSPIRE_REMOTE_FIXED << 8));
  data->space(INSPIRE_REMOTE_MESSAGE_SPACE);
  this->encode_ir_data_(data, INSPIRE_REMOTE_HEADER2 | (INSPIRE_REMOTE_FIXED << 8));

  // The device only accepts a command after it has been repeated this many times,
  // making a full transmission take roughly 300 ms.
  for (int i = 0; i < INSPIRE_REMOTE_REPEATS; i++) {
    data->space(INSPIRE_REMOTE_MESSAGE_SPACE);
    this->encode_ir_data_(data, code | (INSPIRE_REMOTE_FIXED << 8));
  }

  data->space(INSPIRE_REMOTE_END_SPACE);

  transmit.perform();
}

bool InspireRemote::on_receive(remote_base::RemoteReceiveData data) {
  if (millis() - this->last_sent_time_ < INSPIRE_REMOTE_SELF_ECHO_DELAY)
    return false;

  // Frame layout: header1, message space, header2, [message space, code] x repeats
  if (!this->expect_code_(data, INSPIRE_REMOTE_HEADER1 | (INSPIRE_REMOTE_FIXED << 8)))
    return false;
  if (!data.expect_space(INSPIRE_REMOTE_MESSAGE_SPACE))
    return false;
  if (!this->expect_code_(data, INSPIRE_REMOTE_HEADER2 | (INSPIRE_REMOTE_FIXED << 8)))
    return false;
  if (!data.expect_space(INSPIRE_REMOTE_MESSAGE_SPACE))
    return false;

  uint8_t code;
  if (!this->receive_code_(data, code))
    return false;

  return this->parse_code_(code);
}

bool InspireRemote::parse_code_(uint8_t code) {
#ifdef USE_FAN
  if (this->fan_ == nullptr && (code == INSPIRE_REMOTE_HIGH || code == INSPIRE_REMOTE_MEDIUM ||
                                code == INSPIRE_REMOTE_LOW || code == INSPIRE_REMOTE_OFF)) {
    ESP_LOGW(TAG, "Received fan code 0x%02X but no fan is configured", code);
    return true;
  }
#endif
  switch (code) {
#ifdef USE_LIGHT
    case INSPIRE_REMOTE_LIGHT:
      this->toggle_light_state();
      break;
#endif
#ifdef USE_FAN
    case INSPIRE_REMOTE_HIGH:
      this->fan_->speed = 3;
      this->fan_->state = true;
      this->fan_->publish_state();
      break;
    case INSPIRE_REMOTE_MEDIUM:
      this->fan_->speed = 2;
      this->fan_->state = true;
      this->fan_->publish_state();
      break;
    case INSPIRE_REMOTE_LOW:
      this->fan_->speed = 1;
      this->fan_->state = true;
      this->fan_->publish_state();
      break;
    case INSPIRE_REMOTE_OFF:
      this->fan_->state = false;
      this->fan_->publish_state();
      break;
#endif
    default:
      ESP_LOGD(TAG, "Received unknown code 0x%02X", code);
      return false;
  }
  return true;
}

bool InspireRemote::expect_code_(remote_base::RemoteReceiveData &data, uint16_t expected_code) {
  for (int i = INSPIRE_REMOTE_FRAME_SIZE - 1; i >= 0; i--) {
    bool bit = expected_code & (1 << i);
    if (i != 0) {
      if (!data.expect_item(bit ? INSPIRE_REMOTE_ONE_MARK : INSPIRE_REMOTE_ZERO_MARK,
                            bit ? INSPIRE_REMOTE_ONE_SPACE : INSPIRE_REMOTE_ZERO_SPACE))
        return false;
    } else {
      if (!data.expect_mark(bit ? INSPIRE_REMOTE_ONE_MARK : INSPIRE_REMOTE_ZERO_MARK))
        return false;
    }
  }
  return true;
}

bool InspireRemote::receive_code_(remote_base::RemoteReceiveData &data, uint8_t &code) {
  uint16_t frame = 0;

  for (int bit = INSPIRE_REMOTE_FRAME_SIZE - 1; bit >= 0; bit--) {
    if (bit != 0) {
      if (data.expect_item(INSPIRE_REMOTE_ONE_MARK, INSPIRE_REMOTE_ONE_SPACE)) {
        frame |= 1 << bit;
      } else if (!data.expect_item(INSPIRE_REMOTE_ZERO_MARK, INSPIRE_REMOTE_ZERO_SPACE)) {
        return false;
      }
    } else {
      if (data.expect_mark(INSPIRE_REMOTE_ONE_MARK)) {
        frame |= 1 << bit;
      } else if (!data.expect_mark(INSPIRE_REMOTE_ZERO_MARK)) {
        return false;
      }
    }
  }

  // Validate the fixed prefix to reject noise that happens to match a command byte
  if ((frame >> 8) != INSPIRE_REMOTE_FIXED)
    return false;

  code = frame & 0xFF;
  return true;
}

}  // namespace esphome::inspire_remote
