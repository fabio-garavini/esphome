#include <gtest/gtest.h>

#include "esphome/components/inspire_remote/light/inspire_light.h"

#include "../common.h"

namespace esphome::inspire_remote::testing {

namespace {

struct LightFixture {
  CapturingTransmitter transmitter;
  InspireRemote remote;
  InspireLight output;
  light::LightState state{&output};

  explicit LightFixture(bool restored_on = false) {
    this->remote.set_transmitter(&this->transmitter);
    this->output.set_parent(&this->remote);
    this->state.set_save_enabled(false);
    if (restored_on) {
      this->state.set_state_callback([](light::LightStateRTCState &values, bool restored) { values.state = true; });
    }
    this->state.setup();
  }

  void settle() {
    for (uint8_t pass = 0; pass < 3; pass++)
      this->state.loop();
  }

  void command(bool on) {
    auto call = this->state.make_call();
    call.set_state(on);
    call.perform();
    this->settle();
  }
};

}  // namespace

TEST(InspireRemoteLight, RestoreAndIdempotentCommandsDoNotToggle) {
  LightFixture fixture;
  fixture.settle();
  EXPECT_TRUE(fixture.transmitter.frames.empty());

  fixture.command(false);
  EXPECT_TRUE(fixture.transmitter.frames.empty());
  fixture.command(true);
  ASSERT_EQ(fixture.transmitter.frames.size(), 1u);
  EXPECT_EQ(as_received(fixture.transmitter.frames.back()), make_transmission(INSPIRE_REMOTE_LIGHT));

  fixture.command(true);
  EXPECT_EQ(fixture.transmitter.frames.size(), 1u);
  fixture.command(false);
  EXPECT_EQ(fixture.transmitter.frames.size(), 2u);
}

TEST(InspireRemoteLight, RestoredOnIsAdoptedWithoutTransmission) {
  LightFixture fixture(true);
  fixture.settle();
  EXPECT_TRUE(fixture.state.remote_values.is_on());
  EXPECT_TRUE(fixture.transmitter.frames.empty());

  fixture.command(false);
  EXPECT_EQ(fixture.transmitter.frames.size(), 1u);
}

TEST(InspireRemoteLight, BootCommandBeforeTheFirstWriteIsNotSwallowed) {
  LightFixture fixture;
  auto call = fixture.state.make_call();
  call.set_state(true);
  call.perform();
  fixture.settle();

  EXPECT_TRUE(fixture.state.remote_values.is_on());
  EXPECT_EQ(fixture.transmitter.frames.size(), 1u);
}

TEST(InspireRemoteLight, PhysicalRemoteUpdatesWithoutEchoingAToggle) {
  LightFixture fixture;
  fixture.settle();
  ASSERT_TRUE(receive(fixture.remote, make_transmission(INSPIRE_REMOTE_LIGHT)));
  fixture.settle();

  EXPECT_TRUE(fixture.state.remote_values.is_on());
  EXPECT_TRUE(fixture.transmitter.frames.empty());

  ASSERT_TRUE(receive(fixture.remote, make_transmission(INSPIRE_REMOTE_LIGHT)));
  fixture.settle();
  EXPECT_FALSE(fixture.state.remote_values.is_on());
  EXPECT_TRUE(fixture.transmitter.frames.empty());
}

TEST(InspireRemoteLight, LocalCommandImmediatelyAfterPhysicalRemoteIsPreserved) {
  LightFixture fixture;
  fixture.settle();
  ASSERT_TRUE(receive(fixture.remote, make_transmission(INSPIRE_REMOTE_LIGHT)));
  fixture.command(false);

  EXPECT_FALSE(fixture.state.remote_values.is_on());
  ASSERT_EQ(fixture.transmitter.frames.size(), 1u);
  EXPECT_EQ(as_received(fixture.transmitter.frames.back()), make_transmission(INSPIRE_REMOTE_LIGHT));
}

}  // namespace esphome::inspire_remote::testing
