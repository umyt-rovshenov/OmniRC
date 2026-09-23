#include <controllar/Crc16.h>
#include <controllar/Protocol.h>
#include <controllar/SequenceTracker.h>
#include <string.h>
#include <unity.h>

using namespace controllar;

void setUp() {}
void tearDown() {}

// --- CRC -------------------------------------------------------------------

void test_crc_matches_the_standard_check_value() {
    const char* input = "123456789";
    const uint16_t result = crc16(reinterpret_cast<const uint8_t*>(input), 9);
    TEST_ASSERT_EQUAL_HEX16(0x29B1, result);
}

void test_crc_of_nothing_is_the_seed() {
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, crc16(nullptr, 0));
    const uint8_t data[1] = {0};
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, crc16(data, 0));
}

void test_crc_detects_a_single_flipped_bit() {
    uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    const uint16_t before = crc16(data, sizeof(data));
    data[4] ^= 0x01u;
    TEST_ASSERT_NOT_EQUAL(before, crc16(data, sizeof(data)));
}

// --- Frame sizes -----------------------------------------------------------

void test_frame_sizes_fit_the_radio_payload() {
    TEST_ASSERT_EQUAL_UINT8(7, frameSize(0));
    TEST_ASSERT_EQUAL_UINT8(13, frameSize(3));
    TEST_ASSERT_EQUAL_UINT8(31, frameSize(kMaxChannels));
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(kMaxPayloadSize, kMaxControlFrameSize);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(kMaxPayloadSize, kMaxTelemetryFrameSize);
}

// --- Control frames --------------------------------------------------------

void test_control_frame_survives_a_round_trip() {
    ControlFrame sent;
    sent.seq = 42;
    sent.flags = ControlFlag::kArmed;
    sent.channelCount = 4;
    sent.channels[0] = 0;
    sent.channels[1] = 1000;
    sent.channels[2] = -1000;
    sent.channels[3] = 1;

    uint8_t buffer[kMaxPayloadSize] = {};
    const uint8_t written = encodeControl(sent, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_UINT8(frameSize(4), written);
    TEST_ASSERT_EQUAL_HEX8(kControlMagic, buffer[0]);
    TEST_ASSERT_EQUAL_UINT8(kProtocolVersion, buffer[1]);

    ControlFrame received;
    TEST_ASSERT_EQUAL(DecodeError::Ok, decodeControl(buffer, written, received));
    TEST_ASSERT_EQUAL_UINT8(sent.seq, received.seq);
    TEST_ASSERT_EQUAL_UINT8(sent.flags, received.flags);
    TEST_ASSERT_EQUAL_UINT8(sent.channelCount, received.channelCount);
    for (uint8_t i = 0; i < sent.channelCount; ++i) {
        TEST_ASSERT_EQUAL_INT16(sent.channels[i], received.channels[i]);
    }
}

void test_control_frame_with_no_channels_is_valid() {
    ControlFrame sent;
    sent.seq = 7;

    uint8_t buffer[kMaxPayloadSize] = {};
    const uint8_t written = encodeControl(sent, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_UINT8(kMinFrameSize, written);

    ControlFrame received;
    received.channelCount = 9;  // must be overwritten
    TEST_ASSERT_EQUAL(DecodeError::Ok, decodeControl(buffer, written, received));
    TEST_ASSERT_EQUAL_UINT8(0, received.channelCount);
    TEST_ASSERT_EQUAL_UINT8(7, received.seq);
}

void test_control_frame_carries_extreme_values_on_every_channel() {
    ControlFrame sent;
    sent.seq = 255;
    sent.flags = ControlFlag::kArmed | ControlFlag::kFailsafeRequest;
    sent.channelCount = kMaxChannels;
    for (uint8_t i = 0; i < kMaxChannels; ++i) {
        sent.channels[i] = (i % 2 == 0) ? INT16_MIN : INT16_MAX;
    }

    uint8_t buffer[kMaxPayloadSize] = {};
    const uint8_t written = encodeControl(sent, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_UINT8(kMaxControlFrameSize, written);

    ControlFrame received;
    TEST_ASSERT_EQUAL(DecodeError::Ok, decodeControl(buffer, written, received));
    for (uint8_t i = 0; i < kMaxChannels; ++i) {
        TEST_ASSERT_EQUAL_INT16(sent.channels[i], received.channels[i]);
    }
}

void test_encode_refuses_a_buffer_that_is_too_small() {
    ControlFrame frame;
    frame.channelCount = 4;

    uint8_t buffer[kMaxPayloadSize];
    memset(buffer, 0xAA, sizeof(buffer));

    TEST_ASSERT_EQUAL_UINT8(0, encodeControl(frame, buffer, frameSize(4) - 1));
    // Nothing may be written when encoding fails.
    TEST_ASSERT_EQUAL_HEX8(0xAA, buffer[0]);
}

void test_encode_refuses_too_many_channels() {
    ControlFrame frame;
    frame.channelCount = kMaxChannels + 1;

    uint8_t buffer[kMaxPayloadSize] = {};
    TEST_ASSERT_EQUAL_UINT8(0, encodeControl(frame, buffer, sizeof(buffer)));
}

void test_encode_refuses_a_null_buffer() {
    ControlFrame frame;
    TEST_ASSERT_EQUAL_UINT8(0, encodeControl(frame, nullptr, 32));
}

// --- Decoder rejection -----------------------------------------------------

namespace {
/// Builds a valid 3-channel control frame for the rejection tests to corrupt.
uint8_t makeValidControl(uint8_t* buffer, uint8_t capacity) {
    ControlFrame frame;
    frame.seq = 3;
    frame.channelCount = 3;
    frame.channels[0] = 100;
    frame.channels[1] = -200;
    frame.channels[2] = 300;
    return encodeControl(frame, buffer, capacity);
}
}  // namespace

void test_decoder_rejects_a_short_buffer() {
    uint8_t buffer[kMaxPayloadSize] = {};
    const uint8_t written = makeValidControl(buffer, sizeof(buffer));
    ControlFrame out;
    TEST_ASSERT_EQUAL(DecodeError::TooShort, decodeControl(buffer, kMinFrameSize - 1, out));
    TEST_ASSERT_EQUAL(DecodeError::TooShort, decodeControl(buffer, 0, out));
    TEST_ASSERT_EQUAL(DecodeError::TooShort, decodeControl(nullptr, written, out));
}

void test_decoder_rejects_the_wrong_frame_type() {
    uint8_t buffer[kMaxPayloadSize] = {};
    const uint8_t written = makeValidControl(buffer, sizeof(buffer));

    // A control frame must never parse as telemetry, or a misrouted packet
    // would be read as vehicle data.
    TelemetryFrame telemetry;
    TEST_ASSERT_EQUAL(DecodeError::BadMagic, decodeTelemetry(buffer, written, telemetry));
}

void test_decoder_rejects_a_different_protocol_version() {
    uint8_t buffer[kMaxPayloadSize] = {};
    const uint8_t written = makeValidControl(buffer, sizeof(buffer));
    buffer[1] = kProtocolVersion + 1;

    ControlFrame out;
    TEST_ASSERT_EQUAL(DecodeError::UnsupportedVersion, decodeControl(buffer, written, out));
}

void test_decoder_rejects_an_impossible_channel_count() {
    uint8_t buffer[kMaxPayloadSize] = {};
    const uint8_t written = makeValidControl(buffer, sizeof(buffer));
    buffer[4] = kMaxChannels + 1;

    ControlFrame out;
    TEST_ASSERT_EQUAL(DecodeError::BadCount, decodeControl(buffer, written, out));
}

void test_decoder_rejects_a_length_that_contradicts_the_count() {
    uint8_t buffer[kMaxPayloadSize] = {};
    const uint8_t written = makeValidControl(buffer, sizeof(buffer));

    ControlFrame out;
    TEST_ASSERT_EQUAL(DecodeError::LengthMismatch, decodeControl(buffer, written + 1, out));
    TEST_ASSERT_EQUAL(DecodeError::LengthMismatch, decodeControl(buffer, written - 1, out));
}

void test_decoder_rejects_a_corrupted_payload() {
    uint8_t buffer[kMaxPayloadSize] = {};
    const uint8_t written = makeValidControl(buffer, sizeof(buffer));
    buffer[6] ^= 0x01u;  // flip one bit inside the channel data

    ControlFrame out;
    TEST_ASSERT_EQUAL(DecodeError::BadCrc, decodeControl(buffer, written, out));
}

void test_a_failed_decode_leaves_the_output_untouched() {
    uint8_t buffer[kMaxPayloadSize] = {};
    const uint8_t written = makeValidControl(buffer, sizeof(buffer));
    buffer[6] ^= 0x01u;

    ControlFrame out;
    out.seq = 77;
    out.channelCount = 2;
    out.channels[0] = 1234;

    TEST_ASSERT_EQUAL(DecodeError::BadCrc, decodeControl(buffer, written, out));
    // Stale data is far safer than half-parsed data on a control link.
    TEST_ASSERT_EQUAL_UINT8(77, out.seq);
    TEST_ASSERT_EQUAL_UINT8(2, out.channelCount);
    TEST_ASSERT_EQUAL_INT16(1234, out.channels[0]);
}

void test_every_decode_error_has_a_name() {
    const DecodeError all[] = {DecodeError::Ok,       DecodeError::TooShort,
                               DecodeError::BadMagic, DecodeError::UnsupportedVersion,
                               DecodeError::BadCount, DecodeError::LengthMismatch,
                               DecodeError::BadCrc};
    for (const DecodeError error : all) {
        TEST_ASSERT_NOT_NULL(toString(error));
        TEST_ASSERT_TRUE(strlen(toString(error)) > 0);
    }
}

// --- Telemetry frames ------------------------------------------------------

void test_telemetry_frame_survives_a_round_trip() {
    TelemetryFrame sent;
    sent.ackSeq = 200;
    sent.status = TelemetryStatus::kArmed | TelemetryStatus::kLowBattery;
    sent.valueCount = 3;
    sent.values[0] = 1180;  // 11.80 V at a scale of 0.01
    sent.values[1] = -45;
    sent.values[2] = 32000;

    uint8_t buffer[kMaxPayloadSize] = {};
    const uint8_t written = encodeTelemetry(sent, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_UINT8(frameSize(3), written);
    TEST_ASSERT_EQUAL_HEX8(kTelemetryMagic, buffer[0]);

    TelemetryFrame received;
    TEST_ASSERT_EQUAL(DecodeError::Ok, decodeTelemetry(buffer, written, received));
    TEST_ASSERT_EQUAL_UINT8(sent.ackSeq, received.ackSeq);
    TEST_ASSERT_EQUAL_UINT8(sent.status, received.status);
    TEST_ASSERT_EQUAL_UINT8(sent.valueCount, received.valueCount);
    for (uint8_t i = 0; i < sent.valueCount; ++i) {
        TEST_ASSERT_EQUAL_INT16(sent.values[i], received.values[i]);
    }
}

void test_telemetry_frame_does_not_parse_as_control() {
    TelemetryFrame sent;
    sent.valueCount = 2;

    uint8_t buffer[kMaxPayloadSize] = {};
    const uint8_t written = encodeTelemetry(sent, buffer, sizeof(buffer));

    ControlFrame out;
    TEST_ASSERT_EQUAL(DecodeError::BadMagic, decodeControl(buffer, written, out));
}

// --- Sequence tracking -----------------------------------------------------

void test_tracker_reports_nothing_before_the_first_frame() {
    SequenceTracker tracker;
    TEST_ASSERT_FALSE(tracker.synced());
    TEST_ASSERT_EQUAL_UINT8(0, tracker.lossPercent());
    TEST_ASSERT_EQUAL_UINT32(0, tracker.totalReceived());
}

void test_tracker_sees_no_loss_on_a_clean_link() {
    SequenceTracker tracker;
    for (uint16_t i = 0; i < 50; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, tracker.onFrame(static_cast<uint8_t>(i)));
    }
    TEST_ASSERT_TRUE(tracker.synced());
    TEST_ASSERT_EQUAL_UINT8(0, tracker.lossPercent());
    TEST_ASSERT_EQUAL_UINT32(0, tracker.totalLost());
    TEST_ASSERT_EQUAL_UINT32(50, tracker.totalReceived());
}

void test_tracker_counts_a_gap_as_loss() {
    SequenceTracker tracker;
    tracker.onFrame(10);
    TEST_ASSERT_EQUAL_UINT8(2, tracker.onFrame(13));  // 11 and 12 went missing
    TEST_ASSERT_EQUAL_UINT32(2, tracker.totalLost());
    TEST_ASSERT_EQUAL_UINT32(2, tracker.totalReceived());
}

void test_tracker_handles_the_sequence_wrapping_past_255() {
    SequenceTracker tracker;
    tracker.onFrame(254);
    TEST_ASSERT_EQUAL_UINT8(0, tracker.onFrame(255));
    TEST_ASSERT_EQUAL_UINT8(0, tracker.onFrame(0));
    TEST_ASSERT_EQUAL_UINT8(0, tracker.onFrame(1));
    TEST_ASSERT_EQUAL_UINT8(1, tracker.onFrame(3));
    TEST_ASSERT_EQUAL_UINT32(1, tracker.totalLost());
}

void test_tracker_ignores_duplicates() {
    SequenceTracker tracker;
    tracker.onFrame(5);
    const uint32_t received = tracker.totalReceived();
    TEST_ASSERT_EQUAL_UINT8(0, tracker.onFrame(5));
    TEST_ASSERT_EQUAL_UINT32(received, tracker.totalReceived());
}

void test_tracker_treats_a_long_outage_as_a_resync() {
    SequenceTracker tracker;
    tracker.onFrame(0);
    // The peer restarted or the link was down; inventing 200 lost frames would
    // make the statistics useless.
    TEST_ASSERT_EQUAL_UINT8(0, tracker.onFrame(200));
    TEST_ASSERT_EQUAL_UINT32(0, tracker.totalLost());
    TEST_ASSERT_EQUAL_UINT8(0, tracker.lossPercent());
}

void test_tracker_computes_a_loss_percentage() {
    SequenceTracker tracker;
    // Receive every other frame: 50% loss.
    for (uint16_t i = 0; i < 40; ++i) {
        tracker.onFrame(static_cast<uint8_t>(i * 2));
    }
    TEST_ASSERT_UINT8_WITHIN(2, 50, tracker.lossPercent());
}

void test_tracker_window_decays_so_old_problems_fade() {
    SequenceTracker tracker;
    // A bad patch first.
    for (uint16_t i = 0; i < 60; ++i) {
        tracker.onFrame(static_cast<uint8_t>(i * 3));
    }
    TEST_ASSERT_TRUE(tracker.lossPercent() > 50);

    // Then a long clean run. Recent conditions should dominate.
    uint8_t seq = 0;
    for (uint16_t i = 0; i < 400; ++i) {
        tracker.onFrame(seq++);
    }
    TEST_ASSERT_TRUE(tracker.lossPercent() < 5);
}

void test_tracker_reset_clears_everything() {
    SequenceTracker tracker;
    tracker.onFrame(1);
    tracker.onFrame(10);
    tracker.reset();
    TEST_ASSERT_FALSE(tracker.synced());
    TEST_ASSERT_EQUAL_UINT32(0, tracker.totalReceived());
    TEST_ASSERT_EQUAL_UINT32(0, tracker.totalLost());
    TEST_ASSERT_EQUAL_UINT8(0, tracker.lossPercent());
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_crc_matches_the_standard_check_value);
    RUN_TEST(test_crc_of_nothing_is_the_seed);
    RUN_TEST(test_crc_detects_a_single_flipped_bit);

    RUN_TEST(test_frame_sizes_fit_the_radio_payload);

    RUN_TEST(test_control_frame_survives_a_round_trip);
    RUN_TEST(test_control_frame_with_no_channels_is_valid);
    RUN_TEST(test_control_frame_carries_extreme_values_on_every_channel);
    RUN_TEST(test_encode_refuses_a_buffer_that_is_too_small);
    RUN_TEST(test_encode_refuses_too_many_channels);
    RUN_TEST(test_encode_refuses_a_null_buffer);

    RUN_TEST(test_decoder_rejects_a_short_buffer);
    RUN_TEST(test_decoder_rejects_the_wrong_frame_type);
    RUN_TEST(test_decoder_rejects_a_different_protocol_version);
    RUN_TEST(test_decoder_rejects_an_impossible_channel_count);
    RUN_TEST(test_decoder_rejects_a_length_that_contradicts_the_count);
    RUN_TEST(test_decoder_rejects_a_corrupted_payload);
    RUN_TEST(test_a_failed_decode_leaves_the_output_untouched);
    RUN_TEST(test_every_decode_error_has_a_name);

    RUN_TEST(test_telemetry_frame_survives_a_round_trip);
    RUN_TEST(test_telemetry_frame_does_not_parse_as_control);

    RUN_TEST(test_tracker_reports_nothing_before_the_first_frame);
    RUN_TEST(test_tracker_sees_no_loss_on_a_clean_link);
    RUN_TEST(test_tracker_counts_a_gap_as_loss);
    RUN_TEST(test_tracker_handles_the_sequence_wrapping_past_255);
    RUN_TEST(test_tracker_ignores_duplicates);
    RUN_TEST(test_tracker_treats_a_long_outage_as_a_resync);
    RUN_TEST(test_tracker_computes_a_loss_percentage);
    RUN_TEST(test_tracker_window_decays_so_old_problems_fade);
    RUN_TEST(test_tracker_reset_clears_everything);

    return UNITY_END();
}
