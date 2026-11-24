#include <gtest/gtest.h>

#include "esphome/components/inspire_remote/fan/inspire_fan.h"

#include "../common.h"

namespace esphome::inspire_remote::testing {

namespace {

struct FanFixture {
  CapturingTransmitter transmitter;
  InspireRemote remote;
  InspireFan fan;

  explicit FanFixture(uint8_t speed_count = 3) : fan(speed_count) {
    this->remote.set_transmitter(&this->transmitter);
    this->remote.set_fan(&this->fan);
    this->fan.set_parent(&this->remote);
  }

  void expect_last_command(uint8_t command) {
    ASSERT_FALSE(this->transmitter.frames.empty());
    EXPECT_EQ(as_received(this->transmitter.frames.back()), make_transmission(command));
  }
};

}  // namespace

TEST(InspireRemoteFan, LocalControlsMapToAbsoluteCommands) {
  FanFixture fixture;
  EXPECT_EQ(fixture.fan.get_traits().supported_speed_count(), 3);

  fixture.fan.turn_on().set_speed(1).perform();
  EXPECT_TRUE(fixture.fan.state);
  EXPECT_EQ(fixture.fan.speed, 1);
  fixture.expect_last_command(INSPIRE_REMOTE_LOW);

  fixture.fan.turn_on().set_speed(2).perform();
  fixture.expect_last_command(INSPIRE_REMOTE_MEDIUM);
  fixture.fan.turn_on().set_speed(3).perform();
  fixture.expect_last_command(INSPIRE_REMOTE_HIGH);

  fixture.fan.turn_off().perform();
  EXPECT_FALSE(fixture.fan.state);
  EXPECT_EQ(fixture.fan.speed, 3);
  fixture.expect_last_command(INSPIRE_REMOTE_OFF);
}

TEST(InspireRemoteFan, RestoreIsValidatedAndReplayed) {
  FanFixture fixture;
  fixture.fan.set_restore_mode(fan::FanRestoreMode::ALWAYS_ON);
  fixture.fan.setup();

  EXPECT_TRUE(fixture.fan.state);
  EXPECT_EQ(fixture.fan.speed, 3);
  fixture.expect_last_command(INSPIRE_REMOTE_HIGH);
}

TEST(InspireRemoteFan, PhysicalRemoteUpdatesAndDeduplicatesState) {
  FanFixture fixture(2);
  int publications = 0;
  fixture.fan.add_on_state_callback([&publications]() { publications++; });

  ASSERT_TRUE(receive(fixture.remote, make_transmission(INSPIRE_REMOTE_HIGH)));
  EXPECT_TRUE(fixture.fan.state);
  EXPECT_EQ(fixture.fan.speed, 2);
  EXPECT_EQ(publications, 1);
  EXPECT_TRUE(fixture.transmitter.frames.empty());

  ASSERT_TRUE(receive(fixture.remote, make_transmission(INSPIRE_REMOTE_HIGH)));
  EXPECT_EQ(publications, 1);
  ASSERT_TRUE(receive(fixture.remote, make_transmission(INSPIRE_REMOTE_OFF)));
  EXPECT_FALSE(fixture.fan.state);
  EXPECT_EQ(fixture.fan.speed, 2);
  EXPECT_EQ(publications, 2);

  ASSERT_TRUE(receive(fixture.remote, make_transmission(INSPIRE_REMOTE_LOW)));
  EXPECT_TRUE(fixture.fan.state);
  EXPECT_EQ(fixture.fan.speed, 1);
  EXPECT_EQ(publications, 3);
}

}  // namespace esphome::inspire_remote::testing
