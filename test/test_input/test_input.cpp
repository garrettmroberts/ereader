#include <unity.h>

#include "../../src/input.cpp"

namespace {

Input::Gesture edge(int pin, int level) {
    hostSetPin(pin, level);
    hostMillis += 1;
    return Input::read();
}

Input::Gesture commit() {
    hostMillis += Input::kDebounceMs + 1;
    return Input::read();
}

void down(int pin) {
    const Input::Gesture started = edge(pin, LOW);
    TEST_ASSERT_TRUE(started.prev == Input::Press::None);
    TEST_ASSERT_TRUE(started.next == Input::Press::None);
    TEST_ASSERT_FALSE(started.bothLong);

    const Input::Gesture held = commit();
    TEST_ASSERT_TRUE(held.prev == Input::Press::None);
    TEST_ASSERT_TRUE(held.next == Input::Press::None);
    TEST_ASSERT_FALSE(held.bothLong);
}

Input::Gesture up(int pin) {
    const Input::Gesture released = edge(pin, HIGH);
    TEST_ASSERT_TRUE(released.prev == Input::Press::None);
    TEST_ASSERT_TRUE(released.next == Input::Press::None);
    TEST_ASSERT_FALSE(released.bothLong);
    return commit();
}

bool isNone(const Input::Gesture& gesture) {
    return gesture.prev == Input::Press::None && gesture.next == Input::Press::None && !gesture.bothLong;
}

}  // namespace

extern "C" void setUp(void) {
    hostMillis = 1000;
    hostResetPins();
    Input::resetForTest();
}

extern "C" void tearDown(void) {}

extern "C" void test_init_enables_pullups(void) {
    Input::init();
    TEST_ASSERT_EQUAL_INT(2, hostPinModes.size());
    TEST_ASSERT_EQUAL_INT(Input::kPrevPin, hostPinModes[0].pin);
    TEST_ASSERT_EQUAL_INT(INPUT_PULLUP, hostPinModes[0].mode);
    TEST_ASSERT_EQUAL_INT(Input::kNextPin, hostPinModes[1].pin);
    TEST_ASSERT_EQUAL_INT(INPUT_PULLUP, hostPinModes[1].mode);
}

extern "C" void test_short_press_on_each_button(void) {
    down(Input::kPrevPin);
    const Input::Gesture prev = up(Input::kPrevPin);
    TEST_ASSERT_TRUE(prev.prev == Input::Press::Short);
    TEST_ASSERT_TRUE(prev.next == Input::Press::None);
    TEST_ASSERT_FALSE(prev.bothLong);

    down(Input::kNextPin);
    const Input::Gesture next = up(Input::kNextPin);
    TEST_ASSERT_TRUE(next.prev == Input::Press::None);
    TEST_ASSERT_TRUE(next.next == Input::Press::Short);
    TEST_ASSERT_FALSE(next.bothLong);
}

extern "C" void test_bounce_does_not_count_as_a_press(void) {
    edge(Input::kPrevPin, LOW);
    hostMillis += 20;
    edge(Input::kPrevPin, HIGH);
    const Input::Gesture settled = commit();
    TEST_ASSERT_TRUE(isNone(settled));
}

extern "C" void test_long_press_fires_at_the_threshold_and_does_not_repeat(void) {
    down(Input::kPrevPin);
    hostMillis += Input::kLongPressMs - 1;
    TEST_ASSERT_TRUE(isNone(Input::read()));

    hostMillis += 1;
    const Input::Gesture fired = Input::read();
    TEST_ASSERT_TRUE(fired.prev == Input::Press::Long);
    TEST_ASSERT_TRUE(fired.next == Input::Press::None);
    TEST_ASSERT_FALSE(fired.bothLong);

    hostMillis += Input::kLongPressMs;
    TEST_ASSERT_TRUE(isNone(Input::read()));

    const Input::Gesture released = up(Input::kPrevPin);
    TEST_ASSERT_TRUE(isNone(released));
}

extern "C" void test_both_long_press_suppresses_single_button_gestures(void) {
    down(Input::kPrevPin);
    down(Input::kNextPin);
    hostMillis += Input::kLongPressMs;

    const Input::Gesture both = Input::read();
    TEST_ASSERT_TRUE(both.prev == Input::Press::None);
    TEST_ASSERT_TRUE(both.next == Input::Press::None);
    TEST_ASSERT_TRUE(both.bothLong);

    TEST_ASSERT_TRUE(isNone(up(Input::kPrevPin)));
    TEST_ASSERT_TRUE(isNone(up(Input::kNextPin)));
}

extern "C" void test_long_press_waits_while_the_other_button_is_held(void) {
    down(Input::kPrevPin);
    down(Input::kNextPin);
    // The second press takes a debounce interval, so only the first button is due.
    hostMillis += Input::kLongPressMs - (Input::kDebounceMs + 2);
    TEST_ASSERT_TRUE(isNone(Input::read()));

    // Releasing the second button before its own long press is a short press.
    const Input::Gesture gesture = up(Input::kNextPin);
    TEST_ASSERT_TRUE(gesture.prev == Input::Press::Long);
    TEST_ASSERT_TRUE(gesture.next == Input::Press::Short);
    TEST_ASSERT_FALSE(gesture.bothLong);
    TEST_ASSERT_TRUE(isNone(up(Input::kPrevPin)));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_init_enables_pullups);
    RUN_TEST(test_short_press_on_each_button);
    RUN_TEST(test_bounce_does_not_count_as_a_press);
    RUN_TEST(test_long_press_fires_at_the_threshold_and_does_not_repeat);
    RUN_TEST(test_both_long_press_suppresses_single_button_gestures);
    RUN_TEST(test_long_press_waits_while_the_other_button_is_held);
    return UNITY_END();
}
