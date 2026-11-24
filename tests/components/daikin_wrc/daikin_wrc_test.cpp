#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "esphome/components/daikin_wrc/daikin_wrc.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/application.h"
#include "esphome/core/preferences.h"

namespace esphome::daikin_wrc::testing {

using Frame = std::array<uint8_t, 16>;
using remote_base::RawTimings;

static constexpr uint32_t TOLERANCE_PERCENT = 25;
static constexpr uint8_t MODE_OFF = 0x0;
static constexpr uint8_t MODE_DRY = 0x1;
static constexpr uint8_t MODE_COOL = 0x2;
static constexpr uint8_t MODE_FAN = 0x4;
static constexpr uint8_t MODE_HEAT = 0x8;
static constexpr uint8_t MODE_AUTO = 0xA;
static constexpr uint8_t FAN_AUTO = 0x1;
static constexpr uint8_t FAN_HIGH = 0x2;
static constexpr uint8_t FAN_TURBO = 0x3;
static constexpr uint8_t FAN_MEDIUM = 0x4;
static constexpr uint8_t FAN_LOW = 0x8;
static constexpr uint8_t FAN_SILENT = 0x9;
static constexpr uint8_t FLAG_SWING = 0x1;
static constexpr uint8_t FLAG_SLEEP = 0x2;
static constexpr uint8_t FLAG_DEFAULT = 0x4;
static constexpr uint8_t FLAG_POWER_TOGGLE = 0x8;
static constexpr uint32_t HEADER_MARK = 9800;
static constexpr uint32_t HEADER_SPACE = 9700;
static constexpr uint32_t MESSAGE_HEADER_MARK = 4700;
static constexpr uint32_t MESSAGE_HEADER_SPACE = 2400;
static constexpr uint32_t BIT_MARK = 400;
static constexpr uint32_t ONE_SPACE = 800;
static constexpr uint32_t ZERO_SPACE = 300;
static constexpr uint32_t MESSAGE_SPACE = 20000;
static constexpr uint32_t END_SPACE = 100000;

static uint8_t checksum(const Frame &frame) {
  uint8_t sum = 0;
  for (size_t i = 0; i + 1 < frame.size(); i++)
    sum += frame[i];
  return sum & 0x0F;
}

static Frame make_frame(uint8_t mode = MODE_COOL, uint8_t fan = FAN_AUTO, uint8_t temperature = 24,
                        uint8_t flags = FLAG_DEFAULT) {
  Frame frame{0x6, 0x1, mode, fan, 0x0, 0x0, 0x0, 0x0, 0x2, 0x6, 0x2, 0x4, 0x2, 0x2, 0xC, 0x0};
  frame[12] = temperature % 10;
  frame[13] = temperature / 10;
  frame[14] = flags;
  frame[15] = checksum(frame);
  return frame;
}

static RawTimings capture(const Frame &frame, int preamble_count = 2) {
  RawTimings raw;
  for (int i = 0; i < preamble_count; i++) {
    raw.push_back(HEADER_MARK);
    raw.push_back(-static_cast<int32_t>(HEADER_SPACE));
  }
  raw.push_back(MESSAGE_HEADER_MARK);
  raw.push_back(-static_cast<int32_t>(MESSAGE_HEADER_SPACE));
  for (const uint8_t nibble : frame) {
    for (uint8_t bit = 0; bit < 4; bit++) {
      raw.push_back(BIT_MARK);
      raw.push_back(-static_cast<int32_t>((nibble & (1U << bit)) != 0 ? ONE_SPACE : ZERO_SPACE));
    }
  }
  raw.push_back(BIT_MARK);
  raw.push_back(-static_cast<int32_t>(MESSAGE_SPACE));
  raw.push_back(HEADER_MARK);
  raw.push_back(-static_cast<int32_t>(END_SPACE));
  return raw;
}

static Frame decode(const RawTimings &raw) {
  remote_base::RemoteReceiveData data(raw, TOLERANCE_PERCENT, remote_base::TOLERANCE_MODE_PERCENTAGE);
  for (int32_t offset = 0; offset + 1 < data.size(); offset++) {
    if (!data.peek_item(MESSAGE_HEADER_MARK, MESSAGE_HEADER_SPACE, offset))
      continue;
    data.reset();
    data.advance(offset + 2);
    Frame frame{};
    bool valid = true;
    for (uint8_t &nibble : frame) {
      for (uint8_t bit = 0; bit < 4; bit++) {
        if (data.expect_item(BIT_MARK, ONE_SPACE)) {
          nibble |= 1U << bit;
        } else if (!data.expect_item(BIT_MARK, ZERO_SPACE)) {
          valid = false;
          break;
        }
      }
      if (!valid)
        break;
    }
    if (valid)
      return frame;
  }
  EXPECT_FALSE(true) << "No complete frame in capture";
  return {};
}

class CapturingTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  CapturingTransmitter() : RemoteTransmitterBase(nullptr) {}
  std::vector<RawTimings> frames;
  std::vector<uint32_t> carrier_frequencies;

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override {
    this->frames.push_back(this->temp_.get_data());
    this->carrier_frequencies.push_back(this->temp_.get_carrier_frequency());
  }
};

struct Fixture {
  CapturingTransmitter transmitter;
  binary_sensor::BinarySensor power_sensor;
  DaikinWrcClimate climate;

  explicit Fixture(bool use_power_sensor = false, int initial_power = -1) {
    if (global_preferences != nullptr)
      global_preferences->reset();
    static char name[] = "test_daikin_wrc";
    static char friendly_name[] = "";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
    if (use_power_sensor && initial_power >= 0)
      this->power_sensor.publish_initial_state(initial_power != 0);
    this->climate.set_transmitter(&this->transmitter);
    if (use_power_sensor)
      this->climate.set_state_sensor(&this->power_sensor);
    static_cast<Component &>(this->climate).setup();
  }

  template<typename F> void command(F &&fill) {
    auto call = this->climate.make_call();
    fill(call);
    call.perform();
  }

  Frame last_frame() const {
    EXPECT_FALSE(this->transmitter.frames.empty());
    if (this->transmitter.frames.empty())
      return {};
    return decode(this->transmitter.frames.back());
  }

  bool receive(const RawTimings &raw) {
    remote_base::RemoteReceiveData data(raw, TOLERANCE_PERCENT, remote_base::TOLERANCE_MODE_PERCENTAGE);
    return static_cast<remote_base::RemoteReceiverListener &>(this->climate).on_receive(data);
  }

  bool receive(const Frame &frame, int preamble_count = 2) { return this->receive(capture(frame, preamble_count)); }

  void expect_state(climate::ClimateMode mode, float temperature, climate::ClimateFanMode fan,
                    climate::ClimateSwingMode swing = climate::CLIMATE_SWING_OFF,
                    climate::ClimatePreset preset = climate::CLIMATE_PRESET_NONE) const {
    EXPECT_EQ(this->climate.mode, mode);
    EXPECT_FLOAT_EQ(this->climate.target_temperature, temperature);
    EXPECT_EQ(this->climate.fan_mode, fan);
    EXPECT_EQ(this->climate.swing_mode, swing);
    EXPECT_EQ(this->climate.preset, preset);
  }
};

TEST(DaikinWrc, TransmitsFourBitNibblesWithTheExpectedEnvelope) {
  Fixture f;
  f.command([](auto &call) { call.set_mode(climate::CLIMATE_MODE_COOL); });
  ASSERT_EQ(f.transmitter.frames.size(), 1u);
  ASSERT_EQ(f.transmitter.carrier_frequencies.size(), 1u);
  EXPECT_EQ(f.transmitter.carrier_frequencies[0], 38000);

  const RawTimings &raw = f.transmitter.frames[0];
  ASSERT_EQ(raw.size(), 138u);
  const RawTimings expected_prefix{HEADER_MARK,         -static_cast<int32_t>(HEADER_SPACE),
                                   HEADER_MARK,         -static_cast<int32_t>(HEADER_SPACE),
                                   MESSAGE_HEADER_MARK, -static_cast<int32_t>(MESSAGE_HEADER_SPACE)};
  EXPECT_TRUE(std::equal(expected_prefix.begin(), expected_prefix.end(), raw.begin()));
  EXPECT_EQ(raw[134], BIT_MARK);
  EXPECT_EQ(raw[135], -static_cast<int32_t>(MESSAGE_SPACE));
  EXPECT_EQ(raw[136], HEADER_MARK);
  EXPECT_EQ(raw[137], -static_cast<int32_t>(END_SPACE));
}

TEST(DaikinWrc, EncodesEveryOperatingMode) {
  struct Case {
    climate::ClimateMode mode;
    uint8_t code;
  };
  static constexpr Case CASES[] = {
      {climate::CLIMATE_MODE_HEAT_COOL, MODE_AUTO}, {climate::CLIMATE_MODE_COOL, MODE_COOL},
      {climate::CLIMATE_MODE_DRY, MODE_DRY},        {climate::CLIMATE_MODE_HEAT, MODE_HEAT},
      {climate::CLIMATE_MODE_FAN_ONLY, MODE_FAN},
  };
  Fixture f;
  for (const Case &c : CASES) {
    SCOPED_TRACE(static_cast<int>(c.mode));
    f.command([&](auto &call) {
      call.set_mode(c.mode);
      call.set_target_temperature(24.0f);
    });
    const Frame frame = f.last_frame();
    EXPECT_EQ(frame[2], c.code);
    EXPECT_EQ(frame[12], 4);
    EXPECT_EQ(frame[13], 2);
    EXPECT_EQ(frame[15], checksum(frame));
  }
}

TEST(DaikinWrc, EncodesEveryFanAndSpecialPreset) {
  struct Case {
    climate::ClimateFanMode fan;
    uint8_t code;
  };
  static constexpr Case CASES[] = {
      {climate::CLIMATE_FAN_QUIET, FAN_SILENT}, {climate::CLIMATE_FAN_AUTO, FAN_AUTO},
      {climate::CLIMATE_FAN_LOW, FAN_LOW},      {climate::CLIMATE_FAN_MEDIUM, FAN_MEDIUM},
      {climate::CLIMATE_FAN_HIGH, FAN_HIGH},
  };
  Fixture f;
  f.command([](auto &call) { call.set_mode(climate::CLIMATE_MODE_COOL); });
  for (const Case &c : CASES) {
    SCOPED_TRACE(static_cast<int>(c.fan));
    f.command([&](auto &call) { call.set_fan_mode(c.fan); });
    EXPECT_EQ(f.last_frame()[3], c.code);
  }

  f.command([](auto &call) { call.set_preset(climate::CLIMATE_PRESET_BOOST); });
  EXPECT_EQ(f.last_frame()[3], FAN_TURBO);
  f.command([](auto &call) {
    call.set_preset(climate::CLIMATE_PRESET_SLEEP);
    call.set_swing_mode(climate::CLIMATE_SWING_VERTICAL);
  });
  EXPECT_EQ(f.last_frame()[14], FLAG_DEFAULT | FLAG_SLEEP | FLAG_SWING);
}

TEST(DaikinWrc, RoundsAndClampsTemperature) {
  struct Case {
    float requested;
    uint8_t encoded;
  };
  static constexpr Case CASES[] = {{16.0f, 16}, {22.6f, 23}, {30.0f, 30}, {10.0f, 16}, {35.0f, 30}};
  Fixture f;
  f.command([](auto &call) { call.set_mode(climate::CLIMATE_MODE_COOL); });
  for (const Case &c : CASES) {
    SCOPED_TRACE(c.requested);
    f.command([&](auto &call) { call.set_target_temperature(c.requested); });
    const Frame frame = f.last_frame();
    EXPECT_EQ(frame[13] * 10 + frame[12], c.encoded);
    EXPECT_EQ(frame[15], checksum(frame));
  }
}

TEST(DaikinWrc, PowerBitOnlyTogglesOnTransitions) {
  Fixture f;
  f.command([](auto &call) { call.set_mode(climate::CLIMATE_MODE_COOL); });
  EXPECT_EQ(f.last_frame()[14], FLAG_DEFAULT | FLAG_POWER_TOGGLE);

  f.command([](auto &call) { call.set_target_temperature(23.0f); });
  EXPECT_EQ(f.last_frame()[14], FLAG_DEFAULT);

  f.command([](auto &call) { call.set_mode(climate::CLIMATE_MODE_OFF); });
  Frame frame = f.last_frame();
  EXPECT_EQ(frame[2], MODE_OFF);
  EXPECT_EQ(frame[14], FLAG_DEFAULT | FLAG_POWER_TOGGLE);

  f.command([](auto &call) { call.set_mode(climate::CLIMATE_MODE_OFF); });
  EXPECT_EQ(f.last_frame()[14], FLAG_DEFAULT);

  f.command([](auto &call) { call.set_mode(climate::CLIMATE_MODE_HEAT); });
  EXPECT_EQ(f.last_frame()[14], FLAG_DEFAULT | FLAG_POWER_TOGGLE);
}

TEST(DaikinWrc, ReceivesEveryOperatingMode) {
  struct Case {
    uint8_t code;
    climate::ClimateMode mode;
    bool uses_temperature;
  };
  static constexpr Case CASES[] = {
      {MODE_AUTO, climate::CLIMATE_MODE_HEAT_COOL, true}, {MODE_COOL, climate::CLIMATE_MODE_COOL, true},
      {MODE_DRY, climate::CLIMATE_MODE_DRY, false},       {MODE_HEAT, climate::CLIMATE_MODE_HEAT, true},
      {MODE_FAN, climate::CLIMATE_MODE_FAN_ONLY, false},
  };
  for (const Case &c : CASES) {
    SCOPED_TRACE(static_cast<int>(c.code));
    Fixture f;
    ASSERT_TRUE(f.receive(make_frame(c.code, FAN_SILENT, 27, FLAG_DEFAULT | FLAG_POWER_TOGGLE | FLAG_SWING)));
    f.expect_state(c.mode, c.uses_temperature ? 27.0f : 24.0f, climate::CLIMATE_FAN_QUIET,
                   climate::CLIMATE_SWING_VERTICAL);
  }
}

TEST(DaikinWrc, ReceivesPowerAsAToggle) {
  Fixture f;
  EXPECT_TRUE(f.receive(make_frame(MODE_COOL, FAN_AUTO, 22, FLAG_DEFAULT)));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_OFF);

  EXPECT_TRUE(f.receive(make_frame(MODE_HEAT, FAN_LOW, 25, FLAG_DEFAULT | FLAG_POWER_TOGGLE)));
  f.expect_state(climate::CLIMATE_MODE_HEAT, 25.0f, climate::CLIMATE_FAN_LOW);

  EXPECT_TRUE(f.receive(make_frame(MODE_COOL, FAN_HIGH, 21, FLAG_DEFAULT)));
  f.expect_state(climate::CLIMATE_MODE_COOL, 21.0f, climate::CLIMATE_FAN_HIGH);

  EXPECT_TRUE(f.receive(make_frame(MODE_OFF, FAN_MEDIUM, 99, FLAG_DEFAULT | FLAG_POWER_TOGGLE)));
  f.expect_state(climate::CLIMATE_MODE_OFF, 21.0f, climate::CLIMATE_FAN_MEDIUM);
}

TEST(DaikinWrc, ReceivesBoostAndSleepPresets) {
  Fixture f;
  ASSERT_TRUE(f.receive(make_frame(MODE_COOL, FAN_TURBO, 23, FLAG_DEFAULT | FLAG_POWER_TOGGLE)));
  f.expect_state(climate::CLIMATE_MODE_COOL, 23.0f, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_OFF,
                 climate::CLIMATE_PRESET_BOOST);

  ASSERT_TRUE(f.receive(make_frame(MODE_COOL, FAN_LOW, 22, FLAG_DEFAULT | FLAG_SLEEP)));
  f.expect_state(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_LOW, climate::CLIMATE_SWING_OFF,
                 climate::CLIMATE_PRESET_SLEEP);
}

TEST(DaikinWrc, TransmittedFramesRoundTrip) {
  Fixture sender;
  sender.command([](auto &call) {
    call.set_mode(climate::CLIMATE_MODE_HEAT);
    call.set_target_temperature(26.0f);
    call.set_fan_mode(climate::CLIMATE_FAN_MEDIUM);
    call.set_swing_mode(climate::CLIMATE_SWING_VERTICAL);
    call.set_preset(climate::CLIMATE_PRESET_SLEEP);
  });
  Fixture receiver;
  ASSERT_TRUE(receiver.receive(sender.transmitter.frames.back()));
  receiver.expect_state(climate::CLIMATE_MODE_HEAT, 26.0f, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_SWING_VERTICAL,
                        climate::CLIMATE_PRESET_SLEEP);
}

TEST(DaikinWrc, RejectsInvalidFramesWithoutChangingState) {
  Fixture f;
  ASSERT_TRUE(f.receive(make_frame(MODE_COOL, FAN_HIGH, 22, FLAG_DEFAULT | FLAG_POWER_TOGGLE)));

  Frame bad = make_frame(MODE_HEAT, FAN_LOW, 25);
  bad[15] ^= 0x1;
  EXPECT_FALSE(f.receive(bad));
  f.expect_state(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);

  EXPECT_FALSE(f.receive(make_frame(0x5, FAN_LOW, 25)));
  f.expect_state(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);

  EXPECT_FALSE(f.receive(make_frame(MODE_HEAT, 0x0, 25)));
  f.expect_state(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);

  for (const uint8_t temperature : {15, 31}) {
    SCOPED_TRACE(static_cast<int>(temperature));
    EXPECT_FALSE(f.receive(make_frame(MODE_HEAT, FAN_LOW, temperature)));
    f.expect_state(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);
  }

  bad = make_frame(MODE_HEAT, FAN_LOW, 25);
  bad[12] = 0xA;
  bad[15] = checksum(bad);
  EXPECT_FALSE(f.receive(bad));
  f.expect_state(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);

  bad = make_frame(MODE_HEAT, FAN_LOW, 25);
  bad[0] = 0x5;
  bad[15] = checksum(bad);
  EXPECT_FALSE(f.receive(bad));
  f.expect_state(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);
}

TEST(DaikinWrc, FindsFramesWithoutUsablePreambles) {
  Fixture f;
  const Frame frame = make_frame(MODE_COOL, FAN_AUTO, 24, FLAG_DEFAULT | FLAG_POWER_TOGGLE);
  EXPECT_TRUE(f.receive(frame, 0));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_COOL);

  RawTimings distorted = capture(make_frame(MODE_HEAT, FAN_LOW, 25));
  distorted[0] = 1000;
  distorted[1] = -1000;
  distorted[2] = 1000;
  distorted[3] = -1000;
  EXPECT_TRUE(f.receive(distorted));
  f.expect_state(climate::CLIMATE_MODE_HEAT, 25.0f, climate::CLIMATE_FAN_LOW);

  RawTimings retry{MESSAGE_HEADER_MARK, -static_cast<int32_t>(MESSAGE_HEADER_SPACE), BIT_MARK,
                   -static_cast<int32_t>(ZERO_SPACE)};
  const RawTimings complete = capture(make_frame(MODE_COOL, FAN_HIGH, 21), 0);
  retry.insert(retry.end(), complete.begin(), complete.end());
  EXPECT_TRUE(f.receive(retry));
  f.expect_state(climate::CLIMATE_MODE_COOL, 21.0f, climate::CLIMATE_FAN_HIGH);
}

TEST(DaikinWrc, RejectsBrokenCaptures) {
  Fixture f;
  const Frame frame = make_frame(MODE_COOL, FAN_AUTO, 24, FLAG_DEFAULT | FLAG_POWER_TOGGLE);
  RawTimings truncated = capture(frame);
  truncated.resize(truncated.size() - 20);
  EXPECT_FALSE(f.receive(truncated));

  RawTimings bad_bit = capture(frame);
  bad_bit[10] = 9000;
  EXPECT_FALSE(f.receive(bad_bit));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_OFF);
}

TEST(DaikinWrc, IgnoresItsOwnTransmission) {
  Fixture f;
  f.command([](auto &call) { call.set_mode(climate::CLIMATE_MODE_COOL); });
  ASSERT_EQ(f.transmitter.frames.size(), 1u);
  EXPECT_FALSE(f.receive(f.transmitter.frames.back()));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_COOL);
}

TEST(DaikinWrc, PowerSensorRestoresTheLastActiveMode) {
  Fixture f(true);
  f.command([](auto &call) {
    call.set_mode(climate::CLIMATE_MODE_COOL);
    call.set_target_temperature(22.0f);
  });
  f.power_sensor.publish_state(false);
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_OFF);
  f.power_sensor.publish_state(true);
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 22.0f);

  f.command([](auto &call) { call.set_mode(climate::CLIMATE_MODE_HEAT); });
  f.power_sensor.publish_state(false);
  f.power_sensor.publish_state(true);
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_HEAT);
}

TEST(DaikinWrc, AppliesInitialPowerSensorState) {
  Fixture on(true, true);
  EXPECT_EQ(on.climate.mode, climate::CLIMATE_MODE_HEAT_COOL);

  Fixture off(true, false);
  EXPECT_EQ(off.climate.mode, climate::CLIMATE_MODE_OFF);
}

}  // namespace esphome::daikin_wrc::testing
