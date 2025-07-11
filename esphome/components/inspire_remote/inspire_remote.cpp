#include "inspire_remote.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome {
namespace inspire_remote {

static const char *const TAG = "inspire_remote";

void InspireRemote::encode_ir_data_(remote_base::RemoteTransmitData *data, uint16_t code) {
  for (int16_t i = INSPIRE_REMOTE_FRAME_SIZE - 1; i >= 0; i--) {
    if (code & ((uint16_t) 1 << i)) {
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

  for (int i = 0; i < 8; i++) {
    data->space(INSPIRE_REMOTE_MESSAGE_SPACE);
    this->encode_ir_data_(data, code | (INSPIRE_REMOTE_FIXED << 8));
  }

  data->space(INSPIRE_REMOTE_END_SPACE);

  transmit.perform();
}

bool InspireRemote::parse_code_(uint8_t code) {
  this->last_received_time_ = millis();
  switch (code) {
#ifdef USE_LIGHT
    case INSPIRE_REMOTE_LIGHT: {
      if (this->light_ != nullptr) {
        // Get current state and toggle it
        auto call = this->light_->make_call();
        call.set_state(!this->light_->current_values.is_on());
        call.perform();
      }
      break;
    }
#endif
#ifdef USE_FAN
    case INSPIRE_REMOTE_HIGH:
      fan_->speed = 3;
      fan_->state = true;
      fan_->publish_state();
      break;
    case INSPIRE_REMOTE_MEDIUM:
      fan_->speed = 2;
      fan_->state = true;
      fan_->publish_state();
      break;
    case INSPIRE_REMOTE_LOW:
      fan_->speed = 1;
      fan_->state = true;
      fan_->publish_state();
      break;
    case INSPIRE_REMOTE_OFF:
      fan_->state = false;
      fan_->publish_state();
      break;
#endif
    default:
      return false;
  }
  return true;
}

bool InspireRemote::on_receive(remote_base::RemoteReceiveData data) {
  if (millis() - this->last_sent_time_ < 500)
    return false;  // To ignore self sent command

  if (!expect_code_(data, INSPIRE_REMOTE_HEADER1 | (INSPIRE_REMOTE_FIXED << 8)))
    return false;
  if (!data.expect_space(INSPIRE_REMOTE_MESSAGE_SPACE))
    return false;
  if (!expect_code_(data, INSPIRE_REMOTE_HEADER2 | (INSPIRE_REMOTE_FIXED << 8)))
    return false;

  while (true) {
    if (!data.expect_space(INSPIRE_REMOTE_MESSAGE_SPACE))
      return false;

    uint8_t code = receive_code_(data);

    if (this->parse_code_(code))
      return true;
  }
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

uint8_t InspireRemote::receive_code_(remote_base::RemoteReceiveData &data) {
  uint8_t code = 0;

  for (int bit = INSPIRE_REMOTE_FRAME_SIZE - 1; bit >= 0; bit--) {
    if (bit != 0) {
      if (data.expect_item(INSPIRE_REMOTE_ONE_MARK, INSPIRE_REMOTE_ONE_SPACE)) {
        code |= (1 << bit);
      } else if (!data.expect_item(INSPIRE_REMOTE_ZERO_MARK, INSPIRE_REMOTE_ZERO_SPACE)) {
      }
    } else {
      if (data.expect_mark(INSPIRE_REMOTE_ONE_MARK)) {
        code |= (1 << bit);
      } else if (data.expect_mark(INSPIRE_REMOTE_ZERO_MARK)) {
      }
    }
  }

  return code;
}

}  // namespace inspire_remote
}  // namespace esphome
