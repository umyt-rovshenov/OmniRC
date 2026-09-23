#include <ctl/Error.h>
#include <ctl/Log.h>
#include <ctl/Result.h>
#include <ctl/Time.h>
#include <stdio.h>
#include <string.h>
#include <unity.h>

using namespace ctl;

void setUp() {}
void tearDown() {}

// --- Error -----------------------------------------------------------------

void test_every_error_has_a_name() {
    const Error all[] = {Error::None,           Error::Unknown,        Error::InvalidArgument,
                         Error::OutOfRange,     Error::NotInitialized, Error::AlreadyInitialized,
                         Error::NotSupported,   Error::Timeout,        Error::Busy,
                         Error::NotFound,       Error::BufferTooSmall, Error::HardwareFault,
                         Error::StorageFailure, Error::ParseFailure,   Error::ValidationFailure,
                         Error::LinkDown};
    for (const Error error : all) {
        TEST_ASSERT_NOT_NULL(toString(error));
        TEST_ASSERT_TRUE(strlen(toString(error)) > 0);
    }
}

// --- Result ----------------------------------------------------------------

void test_result_carries_a_value() {
    Result<int> result(42);
    TEST_ASSERT_TRUE(result.ok());
    TEST_ASSERT_TRUE(static_cast<bool>(result));
    TEST_ASSERT_EQUAL(Error::None, result.error());
    TEST_ASSERT_EQUAL_INT(42, result.value());
}

void test_result_carries_an_error() {
    Result<int> result(Error::Timeout);
    TEST_ASSERT_FALSE(result.ok());
    TEST_ASSERT_FALSE(static_cast<bool>(result));
    TEST_ASSERT_EQUAL(Error::Timeout, result.error());
}

void test_failing_with_no_reason_is_reported_as_unknown() {
    // Error::None would otherwise produce a Result that claims success while
    // holding no value, which is always a bug at the call site.
    Result<int> result(Error::None);
    TEST_ASSERT_FALSE(result.ok());
    TEST_ASSERT_EQUAL(Error::Unknown, result.error());
}

void test_value_or_falls_back_only_on_failure() {
    Result<int> good(7);
    Result<int> bad(Error::NotFound);
    TEST_ASSERT_EQUAL_INT(7, good.valueOr(-1));
    TEST_ASSERT_EQUAL_INT(-1, bad.valueOr(-1));
}

namespace {
/// Counts construction and destruction so the tests can prove that Result
/// manages non-trivial payloads correctly and never leaks one.
struct Tracked {
    static int liveCount;
    int value;

    explicit Tracked(int v) : value(v) { ++liveCount; }
    Tracked(const Tracked& other) : value(other.value) { ++liveCount; }
    Tracked(Tracked&& other) : value(other.value) { ++liveCount; }
    Tracked& operator=(const Tracked&) = default;
    Tracked& operator=(Tracked&&) = default;
    ~Tracked() { --liveCount; }
};
int Tracked::liveCount = 0;
}  // namespace

void test_result_destroys_a_non_trivial_value() {
    Tracked::liveCount = 0;
    {
        Result<Tracked> result{Tracked(5)};
        TEST_ASSERT_TRUE(result.ok());
        TEST_ASSERT_EQUAL_INT(5, result.value().value);
        TEST_ASSERT_TRUE(Tracked::liveCount > 0);
    }
    TEST_ASSERT_EQUAL_INT(0, Tracked::liveCount);
}

void test_result_copies_and_moves_without_leaking() {
    Tracked::liveCount = 0;
    {
        Result<Tracked> original{Tracked(9)};
        Result<Tracked> copy = original;
        Result<Tracked> moved = static_cast<Result<Tracked>&&>(copy);
        TEST_ASSERT_EQUAL_INT(9, moved.value().value);
        TEST_ASSERT_EQUAL_INT(9, original.value().value);
    }
    TEST_ASSERT_EQUAL_INT(0, Tracked::liveCount);
}

void test_a_failed_result_holds_no_value() {
    Tracked::liveCount = 0;
    {
        Result<Tracked> result(Error::HardwareFault);
        TEST_ASSERT_FALSE(result.ok());
        TEST_ASSERT_EQUAL_INT(0, Tracked::liveCount);
    }
    TEST_ASSERT_EQUAL_INT(0, Tracked::liveCount);
}

void test_status_reports_success_and_failure() {
    Status good;
    Status bad(Error::StorageFailure);
    TEST_ASSERT_TRUE(good.ok());
    TEST_ASSERT_EQUAL(Error::None, good.error());
    TEST_ASSERT_FALSE(bad.ok());
    TEST_ASSERT_EQUAL(Error::StorageFailure, bad.error());
}

namespace {
Status failingStep() {
    return Error::NotInitialized;
}
Status succeedingStep() {
    return Status();
}

Status runBoth(bool failFirst) {
    if (failFirst) {
        CTL_TRY(failingStep());
    }
    CTL_TRY(succeedingStep());
    return Status();
}

Result<int> propagatesIntoAValueResult() {
    CTL_TRY(failingStep());
    return 1;
}
}  // namespace

void test_try_macro_propagates_the_first_failure() {
    TEST_ASSERT_TRUE(runBoth(false).ok());

    const Status failed = runBoth(true);
    TEST_ASSERT_FALSE(failed.ok());
    TEST_ASSERT_EQUAL(Error::NotInitialized, failed.error());

    const Result<int> propagated = propagatesIntoAValueResult();
    TEST_ASSERT_FALSE(propagated.ok());
    TEST_ASSERT_EQUAL(Error::NotInitialized, propagated.error());
}

// --- Interval --------------------------------------------------------------

void test_interval_fires_once_per_period() {
    time::Interval interval(100);
    TEST_ASSERT_FALSE(interval.expired(1000));  // first call starts the period
    TEST_ASSERT_FALSE(interval.expired(1099));
    TEST_ASSERT_TRUE(interval.expired(1100));
    TEST_ASSERT_FALSE(interval.expired(1101));
    TEST_ASSERT_TRUE(interval.expired(1200));
}

void test_interval_does_not_drift_when_a_call_is_late() {
    time::Interval interval(100);
    interval.reset(1000);
    TEST_ASSERT_TRUE(interval.expired(1150));  // 50 ms late
    // The schedule stays anchored to the original phase instead of restarting.
    TEST_ASSERT_TRUE(interval.expired(1200));
}

void test_interval_skips_the_backlog_after_a_stall() {
    time::Interval interval(10);
    interval.reset(0);
    // A 1 second stall must not produce 100 catch-up firings.
    TEST_ASSERT_TRUE(interval.expired(1000));
    TEST_ASSERT_FALSE(interval.expired(1001));
    TEST_ASSERT_TRUE(interval.expired(1010));
}

void test_interval_with_no_period_always_fires() {
    time::Interval interval(0);
    TEST_ASSERT_TRUE(interval.expired(0));
    TEST_ASSERT_TRUE(interval.expired(0));
}

// --- Deadline --------------------------------------------------------------

void test_deadline_expires_after_its_timeout() {
    time::Deadline deadline;
    TEST_ASSERT_FALSE(deadline.armed());
    TEST_ASSERT_FALSE(deadline.expired(1000000));  // a disarmed deadline never expires

    deadline.arm(500, 1000);
    TEST_ASSERT_TRUE(deadline.armed());
    TEST_ASSERT_FALSE(deadline.expired(1499));
    TEST_ASSERT_EQUAL_UINT32(1, deadline.remainingMs(1499));
    TEST_ASSERT_TRUE(deadline.expired(1500));
    TEST_ASSERT_EQUAL_UINT32(0, deadline.remainingMs(1500));

    deadline.disarm();
    TEST_ASSERT_FALSE(deadline.expired(9999));
}

void test_rearming_a_deadline_restarts_it() {
    time::Deadline deadline;
    deadline.arm(100, 0);
    TEST_ASSERT_TRUE(deadline.expired(200));
    deadline.arm(100, 200);
    TEST_ASSERT_FALSE(deadline.expired(250));
    TEST_ASSERT_TRUE(deadline.expired(300));
}

void test_clock_moves_forward() {
    const uint64_t first = time::millis();
    time::delayMs(5);
    TEST_ASSERT_TRUE(time::millis() >= first);
    TEST_ASSERT_TRUE(time::micros() > 0);
}

// --- Log -------------------------------------------------------------------

namespace {
int g_sinkCalls = 0;
log::Level g_lastLevel = log::Level::None;
char g_lastTag[32] = {};
char g_lastMessage[192] = {};

void captureSink(log::Level level, const char* tag, const char* message) {
    ++g_sinkCalls;
    g_lastLevel = level;
    snprintf(g_lastTag, sizeof(g_lastTag), "%s", tag);
    snprintf(g_lastMessage, sizeof(g_lastMessage), "%s", message);
}
}  // namespace

void test_log_formats_and_reaches_the_sink() {
    g_sinkCalls = 0;
    log::setSink(&captureSink);

    CTL_LOGW("radio", "lost %d of %d frames", 3, 100);

    TEST_ASSERT_EQUAL_INT(1, g_sinkCalls);
    TEST_ASSERT_EQUAL(log::Level::Warn, g_lastLevel);
    TEST_ASSERT_EQUAL_STRING("radio", g_lastTag);
    TEST_ASSERT_EQUAL_STRING("lost 3 of 100 frames", g_lastMessage);

    log::setSink(nullptr);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_every_error_has_a_name);

    RUN_TEST(test_result_carries_a_value);
    RUN_TEST(test_result_carries_an_error);
    RUN_TEST(test_failing_with_no_reason_is_reported_as_unknown);
    RUN_TEST(test_value_or_falls_back_only_on_failure);
    RUN_TEST(test_result_destroys_a_non_trivial_value);
    RUN_TEST(test_result_copies_and_moves_without_leaking);
    RUN_TEST(test_a_failed_result_holds_no_value);
    RUN_TEST(test_status_reports_success_and_failure);
    RUN_TEST(test_try_macro_propagates_the_first_failure);

    RUN_TEST(test_interval_fires_once_per_period);
    RUN_TEST(test_interval_does_not_drift_when_a_call_is_late);
    RUN_TEST(test_interval_skips_the_backlog_after_a_stall);
    RUN_TEST(test_interval_with_no_period_always_fires);

    RUN_TEST(test_deadline_expires_after_its_timeout);
    RUN_TEST(test_rearming_a_deadline_restarts_it);
    RUN_TEST(test_clock_moves_forward);

    RUN_TEST(test_log_formats_and_reaches_the_sink);

    return UNITY_END();
}
