#include "daikin_wrc.h"

#include <cmath>

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::daikin_wrc {

ESPHOME_LOG_TAG(TAG, "daikin_wrc.climate");

namespace {

// All values in the state frame are four-bit nibbles sent least-significant bit first.
static constexpr uint8_t TEMP_MIN = 16;
static constexpr uint8_t TEMP_MAX = 30;

static constexpr uint8_t MODE_AUTO = 0xA;
static constexpr uint8_t MODE_COOL = 0x2;
static constexpr uint8_t MODE_HEAT = 0x8;
static constexpr uint8_t MODE_DRY = 0x1;
static constexpr uint8_t MODE_FAN = 0x4;
static constexpr uint8_t MODE_OFF = 0x0;

static constexpr uint8_t FAN_AUTO = 0x1;
static constexpr uint8_t FAN_SILENT = 0x9;
static constexpr uint8_t FAN_TURBO = 0x3;
static constexpr uint8_t FAN_LOW = 0x8;
static constexpr uint8_t FAN_MEDIUM = 0x4;
static constexpr uint8_t FAN_HIGH = 0x2;

static constexpr uint8_t FRAME_HEADER_1 = 0x6;
static constexpr uint8_t FRAME_HEADER_2 = 0x1;
static constexpr uint8_t MODE_INDEX = 2;
static constexpr uint8_t FAN_INDEX = 3;
static constexpr uint8_t TEMPERATURE_UNITS_INDEX = 12;
static constexpr uint8_t TEMPERATURE_TENS_INDEX = 13;
static constexpr uint8_t FLAGS_INDEX = 14;
static constexpr uint8_t CHECKSUM_INDEX = 15;

static constexpr uint8_t FLAG_SWING = 0x1;
static constexpr uint8_t FLAG_SLEEP = 0x2;
static constexpr uint8_t FLAG_DEFAULT = 0x4;
static constexpr uint8_t FLAG_POWER_TOGGLE = 0x8;

static constexpr uint32_t IR_FREQUENCY = 38000;
static constexpr uint32_t HEADER_MARK = 9800;
static constexpr uint32_t HEADER_SPACE = 9700;
static constexpr uint32_t MESSAGE_HEADER_MARK = 4700;
static constexpr uint32_t MESSAGE_HEADER_SPACE = 2400;
static constexpr uint32_t BIT_MARK = 400;
static constexpr uint32_t ONE_SPACE = 800;
static constexpr uint32_t ZERO_SPACE = 300;
static constexpr uint32_t MESSAGE_SPACE = 20000;
static constexpr uint32_t END_SPACE = 100000;
static constexpr uint32_t SELF_ECHO_GUARD_MS = 500;

uint8_t calculate_checksum(const uint8_t *frame, size_t size) {
  uint8_t checksum = 0;
  for (size_t i = 0; i + 1 < size; i++) {
    checksum += frame[i];
  }
  return checksum & 0x0F;
}

}  // namespace

DaikinWrcClimate::DaikinWrcClimate()
    : climate_ir::ClimateIR(
          TEMP_MIN, TEMP_MAX, 1.0f, true, true,
          {climate::CLIMATE_FAN_QUIET, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
           climate::CLIMATE_FAN_HIGH},
          {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL},
          {climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_SLEEP, climate::CLIMATE_PRESET_BOOST}) {}

void DaikinWrcClimate::setup() {
  climate_ir::ClimateIR::setup();
  if (this->mode != climate::CLIMATE_MODE_OFF)
    this->last_active_mode_ = this->mode;

  if (this->state_sensor_ == nullptr)
    return;

  this->state_sensor_->add_on_state_callback([this](bool state) { this->set_power_state_(state); });
  if (this->state_sensor_->has_state())
    this->set_power_state_(this->state_sensor_->state);
}

void DaikinWrcClimate::set_power_state_(bool power) {
  if (power) {
    if (this->mode == climate::CLIMATE_MODE_OFF) {
      this->mode = this->last_active_mode_;
      this->publish_state();
    }
    return;
  }

  if (this->mode != climate::CLIMATE_MODE_OFF) {
    this->last_active_mode_ = this->mode;
    this->mode = climate::CLIMATE_MODE_OFF;
    this->publish_state();
  }
}

void DaikinWrcClimate::control(const climate::ClimateCall &call) {
  const climate::ClimateMode requested_mode = call.get_mode().value_or(this->mode);
  this->power_toggle_ = (this->mode == climate::CLIMATE_MODE_OFF) != (requested_mode == climate::CLIMATE_MODE_OFF);
  climate_ir::ClimateIR::control(call);
  if (this->mode != climate::CLIMATE_MODE_OFF)
    this->last_active_mode_ = this->mode;
}

void DaikinWrcClimate::build_state_frame_(uint8_t frame[]) const {
  // In flash: an initialized local array is copied from a RAM constant on ESP8266
  static constexpr uint8_t FRAME_TEMPLATE[STATE_FRAME_SIZE] PROGMEM = {
      FRAME_HEADER_1, FRAME_HEADER_2, MODE_COOL, FAN_AUTO, 0x0, 0x0, 0x0, 0x0, 0x2, 0x6, 0x2, 0x4, 0x2, 0x2, 0xC, 0x0,
  };
  progmem_memcpy(frame, FRAME_TEMPLATE, STATE_FRAME_SIZE);

  frame[MODE_INDEX] = this->operation_mode_();
  frame[FAN_INDEX] = this->fan_speed_();
  const uint8_t temperature = this->temperature_();
  frame[TEMPERATURE_UNITS_INDEX] = temperature & 0x0F;
  frame[TEMPERATURE_TENS_INDEX] = temperature >> 4;
  frame[FLAGS_INDEX] = this->special_flags_(this->power_toggle_);
  frame[CHECKSUM_INDEX] = calculate_checksum(frame, STATE_FRAME_SIZE);
}

void DaikinWrcClimate::transmit_state() {
  // Set this before starting because a receiver can see the frame while it is being sent.
  this->last_sent_time_ = millis();

  uint8_t remote_state[STATE_FRAME_SIZE];
  this->build_state_frame_(remote_state);
  this->power_toggle_ = false;

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(IR_FREQUENCY);
  data->reserve(6 + STATE_FRAME_SIZE * 8 + 4);

  data->item(HEADER_MARK, HEADER_SPACE);
  data->item(HEADER_MARK, HEADER_SPACE);
  data->item(MESSAGE_HEADER_MARK, MESSAGE_HEADER_SPACE);
  for (const uint8_t nibble : remote_state) {
    for (uint8_t bit = 0; bit < 4; bit++) {
      data->item(BIT_MARK, (nibble & (1U << bit)) != 0 ? ONE_SPACE : ZERO_SPACE);
    }
  }
  data->item(BIT_MARK, MESSAGE_SPACE);
  data->item(HEADER_MARK, END_SPACE);

  transmit.perform();
}

uint8_t DaikinWrcClimate::operation_mode_() const {
  switch (this->mode) {
    case climate::CLIMATE_MODE_AUTO:
    case climate::CLIMATE_MODE_HEAT_COOL:
      return MODE_AUTO;
    case climate::CLIMATE_MODE_COOL:
      return MODE_COOL;
    case climate::CLIMATE_MODE_DRY:
      return MODE_DRY;
    case climate::CLIMATE_MODE_HEAT:
      return MODE_HEAT;
    case climate::CLIMATE_MODE_FAN_ONLY:
      return MODE_FAN;
    case climate::CLIMATE_MODE_OFF:
    default:
      return MODE_OFF;
  }
}

uint8_t DaikinWrcClimate::fan_speed_() const {
  if (this->preset == climate::CLIMATE_PRESET_BOOST)
    return FAN_TURBO;

  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_QUIET:
      return FAN_SILENT;
    case climate::CLIMATE_FAN_LOW:
      return FAN_LOW;
    case climate::CLIMATE_FAN_MEDIUM:
      return FAN_MEDIUM;
    case climate::CLIMATE_FAN_HIGH:
      return FAN_HIGH;
    case climate::CLIMATE_FAN_AUTO:
    default:
      return FAN_AUTO;
  }
}

uint8_t DaikinWrcClimate::special_flags_(bool power_toggle) const {
  uint8_t flags = FLAG_DEFAULT;
  if (power_toggle)
    flags |= FLAG_POWER_TOGGLE;
  if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL)
    flags |= FLAG_SWING;
  if (this->preset == climate::CLIMATE_PRESET_SLEEP)
    flags |= FLAG_SLEEP;
  return flags;
}

uint8_t DaikinWrcClimate::temperature_() const {
  const uint8_t temperature = static_cast<uint8_t>(roundf(clamp<float>(this->target_temperature, TEMP_MIN, TEMP_MAX)));
  return ((temperature / 10) << 4) | (temperature % 10);
}

bool DaikinWrcClimate::read_state_frame_(remote_base::RemoteReceiveData &data, uint8_t frame[]) const {
  for (uint8_t pos = 0; pos < STATE_FRAME_SIZE; pos++) {
    uint8_t nibble = 0;
    for (uint8_t bit = 0; bit < 4; bit++) {
      if (data.expect_item(BIT_MARK, ONE_SPACE)) {
        nibble |= 1U << bit;
      } else if (!data.expect_item(BIT_MARK, ZERO_SPACE)) {
        return false;
      }
    }
    frame[pos] = nibble;
    if ((pos == 0 && nibble != FRAME_HEADER_1) || (pos == 1 && nibble != FRAME_HEADER_2))
      return false;
  }
  return true;
}

bool DaikinWrcClimate::parse_state_frame_(const uint8_t frame[]) {
  ESP_LOGVV(TAG,
            "Received state frame: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            frame[0], frame[1], frame[2], frame[3], frame[4], frame[5], frame[6], frame[7], frame[8], frame[9],
            frame[10], frame[11], frame[12], frame[13], frame[14], frame[15]);

  const uint8_t checksum = calculate_checksum(frame, STATE_FRAME_SIZE);
  if (frame[CHECKSUM_INDEX] != checksum) {
    ESP_LOGV(TAG, "Invalid checksum: expected 0x%x, got 0x%x", checksum, frame[CHECKSUM_INDEX]);
    return false;
  }

  // Bit 3 toggles power; it is not an absolute power state. Decode everything before changing the entity so a
  // malformed frame cannot leave a partially updated state behind.
  const bool power_toggle = (frame[FLAGS_INDEX] & FLAG_POWER_TOGGLE) != 0;
  const bool powered_on = (this->mode != climate::CLIMATE_MODE_OFF) != power_toggle;
  climate::ClimateMode new_mode = climate::CLIMATE_MODE_OFF;
  float new_temperature = this->target_temperature;

  if (powered_on) {
    switch (frame[MODE_INDEX]) {
      case MODE_COOL:
        new_mode = climate::CLIMATE_MODE_COOL;
        break;
      case MODE_DRY:
        new_mode = climate::CLIMATE_MODE_DRY;
        break;
      case MODE_HEAT:
        new_mode = climate::CLIMATE_MODE_HEAT;
        break;
      case MODE_AUTO:
        new_mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      case MODE_FAN:
        new_mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      default:
        ESP_LOGV(TAG, "Invalid operation mode: 0x%x", frame[MODE_INDEX]);
        return false;
    }

    if (new_mode == climate::CLIMATE_MODE_COOL || new_mode == climate::CLIMATE_MODE_HEAT ||
        new_mode == climate::CLIMATE_MODE_HEAT_COOL) {
      const uint8_t temperature = frame[TEMPERATURE_TENS_INDEX] * 10 + frame[TEMPERATURE_UNITS_INDEX];
      if (temperature < TEMP_MIN || temperature > TEMP_MAX || frame[TEMPERATURE_UNITS_INDEX] > 9) {
        ESP_LOGV(TAG, "Invalid temperature: %u", temperature);
        return false;
      }
      new_temperature = static_cast<float>(temperature);
    }
  }

  climate::ClimateFanMode new_fan_mode;
  switch (frame[FAN_INDEX]) {
    case FAN_LOW:
      new_fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case FAN_MEDIUM:
      new_fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case FAN_HIGH:
      new_fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case FAN_AUTO:
    case FAN_TURBO:
      new_fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
    case FAN_SILENT:
      new_fan_mode = climate::CLIMATE_FAN_QUIET;
      break;
    default:
      ESP_LOGV(TAG, "Invalid fan mode: 0x%x", frame[FAN_INDEX]);
      return false;
  }

  climate::ClimatePreset new_preset = climate::CLIMATE_PRESET_NONE;
  if (frame[FAN_INDEX] == FAN_TURBO)
    new_preset = climate::CLIMATE_PRESET_BOOST;
  if ((frame[FLAGS_INDEX] & FLAG_SLEEP) != 0)
    new_preset = climate::CLIMATE_PRESET_SLEEP;

  this->mode = new_mode;
  this->target_temperature = new_temperature;
  this->fan_mode = new_fan_mode;
  this->swing_mode =
      (frame[FLAGS_INDEX] & FLAG_SWING) != 0 ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
  this->preset = new_preset;
  if (new_mode != climate::CLIMATE_MODE_OFF)
    this->last_active_mode_ = new_mode;
  this->publish_state();
  return true;
}

bool DaikinWrcClimate::on_receive(remote_base::RemoteReceiveData data) {
  if (this->last_sent_time_ != UINT32_MAX && millis() - this->last_sent_time_ < SELF_ECHO_GUARD_MS)
    return false;

  // A receiver can distort the first symbols after being idle, and some third-party remotes omit the two preamble
  // headers. Try every message header in the capture rather than dropping the whole command after one bad symbol.
  for (int32_t offset = 0; offset + 1 < data.size(); offset++) {
    data.reset();
    if (!data.peek_item(MESSAGE_HEADER_MARK, MESSAGE_HEADER_SPACE, offset))
      continue;

    data.advance(offset + 2);
    uint8_t state_frame[STATE_FRAME_SIZE];
    if (this->read_state_frame_(data, state_frame) && this->parse_state_frame_(state_frame))
      return true;
  }
  return false;
}

}  // namespace esphome::daikin_wrc
