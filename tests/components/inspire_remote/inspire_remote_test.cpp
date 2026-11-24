#include <gtest/gtest.h>

#include "common.h"

namespace esphome::inspire_remote::testing {

TEST(InspireRemoteProtocol, TransmitsTheExactWaveformForEveryCommand) {
  static constexpr uint8_t COMMANDS[] = {
      INSPIRE_REMOTE_HIGH,  INSPIRE_REMOTE_MEDIUM, INSPIRE_REMOTE_LOW, INSPIRE_REMOTE_OFF,
      INSPIRE_REMOTE_LIGHT, INSPIRE_REMOTE_2H,     INSPIRE_REMOTE_4H,  INSPIRE_REMOTE_8H,
  };

  CapturingTransmitter transmitter;
  InspireRemote remote;
  remote.set_transmitter(&transmitter);
  for (uint8_t command : COMMANDS) {
    SCOPED_TRACE(command);
    remote.transmit_code(command);
    ASSERT_EQ(transmitter.frames.size(), 1u);
    EXPECT_EQ(transmitter.carrier_frequencies.back(), INSPIRE_REMOTE_IR_FREQUENCY);
    EXPECT_EQ(as_received(transmitter.frames.back()), make_transmission(command));
    transmitter.frames.clear();
    transmitter.carrier_frequencies.clear();
  }
}

TEST(InspireRemoteProtocol, AcceptsAllKnownCommandsWithATightTolerance) {
  static constexpr uint8_t COMMANDS[] = {
      INSPIRE_REMOTE_HIGH,  INSPIRE_REMOTE_MEDIUM, INSPIRE_REMOTE_LOW, INSPIRE_REMOTE_OFF,
      INSPIRE_REMOTE_LIGHT, INSPIRE_REMOTE_2H,     INSPIRE_REMOTE_4H,  INSPIRE_REMOTE_8H,
  };

  for (uint8_t command : COMMANDS) {
    SCOPED_TRACE(command);
    InspireRemote remote;
    EXPECT_TRUE(receive(remote, make_transmission(command)));
    EXPECT_TRUE(receive(remote, as_received(make_transmission(command), true)));
  }
}

TEST(InspireRemoteProtocol, RejectsMalformedAndUnknownFrames) {
  InspireRemote remote;
  EXPECT_FALSE(receive(remote, {}));
  EXPECT_FALSE(receive(remote, make_transmission(INSPIRE_REMOTE_LIGHT, INSPIRE_REMOTE_FIXED + 1)));
  EXPECT_FALSE(receive(remote, make_transmission(INSPIRE_REMOTE_LIGHT, INSPIRE_REMOTE_FIXED, 0x01)));
  EXPECT_FALSE(receive(remote, make_transmission(INSPIRE_REMOTE_LIGHT, INSPIRE_REMOTE_FIXED, INSPIRE_REMOTE_HEADER1,
                                                 INSPIRE_REMOTE_HEADER2 + 1)));
  EXPECT_FALSE(receive(remote, make_transmission(0xFF)));

  auto truncated = make_transmission(INSPIRE_REMOTE_LIGHT);
  truncated.resize(20);
  EXPECT_FALSE(receive(remote, truncated));
}

TEST(InspireRemoteProtocol, SuppressesOnlyThePostTransmissionEchoWindow) {
  const auto waveform = make_transmission(INSPIRE_REMOTE_LIGHT);
  InspireRemote receiver;
  EXPECT_TRUE(receive(receiver, waveform));

  CapturingTransmitter transmitter;
  InspireRemote sender;
  sender.set_transmitter(&transmitter);
  sender.transmit_code(INSPIRE_REMOTE_LIGHT);
  ASSERT_EQ(transmitter.frames.size(), 1u);
  EXPECT_FALSE(receive(sender, as_received(transmitter.frames.back())));
}

}  // namespace esphome::inspire_remote::testing
