/*
    Copyright 2023 Zach Vonler

    This file is part of CircuitPlaygroundGestures.

    CircuitPlaygroundGestures is free software: you can redistribute it
    and/or modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 3 of the License,
    or (at your option) any later version.

    CircuitPlaygroundGestures is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
    Public License for more details.

    You should have received a copy of the GNU General Public License along with
    CircuitPlaygroundGestures.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef circuit_playground_gestures_h
#define circuit_playground_gestures_h

#include <DebouncedButton.h>

#include "DebouncedSwitch.h"

/*---------------------------------------------------------------------------*/

/**
 * Recognizes gestures input through the Circuit Playground buttons.
 */
class CircuitPlaygroundGestures
{
public:
    /**
     * The gestures this class recognizes.
     */
    enum Gesture {
        NONE,
        ORIENTATION_CHANGED,
        SHAKEN,
        DOUBLE_TAPPED,
        SLIDE_SWITCHED_ON,
        SLIDE_SWITCHED_OFF,
        LEFT_PRESSED,
        RIGHT_PRESSED,
        LEFT_CLICKED,
        RIGHT_CLICKED,
        LEFT_RELEASED,
        RIGHT_RELEASED,
        BOTH_CLICKED,
        BOTH_PRESSED,
        BOTH_RELEASED,
        LEFT_HELD_RIGHT_CLICKED,
        RIGHT_HELD_LEFT_CLICKED,
        LEFT_DOUBLE_CLICKED,
        RIGHT_DOUBLE_CLICKED,
    };

    /**
     * The recognized orientations of the Circuit Playground.
     */
    enum Orientation {
        UNKNOWN_ORIENTATION,
        Z_UP,
        Z_DOWN,
        Y_UP,
        Y_DOWN,
        X_UP,
        X_DOWN
    };

    // Used to direct interrupts to the singleton instance.
    friend struct CPGAccessor;

    // The amount of acceleration above which to ignore taps.
    constexpr static float TAP_IGNORE_THRESHOLD = 16;

    // The number of millis to ignore taps after button or switch activity.
    constexpr static uint32_t TAP_IGNORE_MS = 250;

    // The minimum time between recognizing shake events.
    constexpr static uint32_t SHAKE_RESET_MS = 500;

    // The value in (m/s^2) that must be exceeded to recognize a shake.
    constexpr static float SHAKE_THRESHOLD = 22;

    // The EWMA lambda applied to acceleration readings.
    constexpr static float ACCEL_DECAY = 0.6;

    /**
     * Returns a reference to the CircuitPlaygroundGestures singleton.
     *
     * CircuitPlayground.begin() must be called before invoking any of the
     * methods on the returned instance.
     */
    static CircuitPlaygroundGestures& instance();

    /**
     * Initializes the gesture recognizer. Must be called before any other
     * instance methods.
     */
    void begin();

    /**
     * Updates the button readings and returns a gesture if one was recognized.
     */
    Gesture update(uint32_t tm);

    /**
     * Returns true if both buttons are currently pressed.
     */
    bool both_pressed() const { return _left_button.state() && _right_button.state(); }

    /**
     * Returns the duration of the current gesture.
     */
    uint32_t duration(uint32_t tm) const
    {
        return min(left_duration(tm), right_duration(tm));
    }

    /**
     * Returns the duration of the current left button state.
     */
    uint32_t left_duration(uint32_t tm) const
    {
        return _left_button.duration(tm);
    }

    /**
     * Returns the duration of the current right button state.
     */
    uint32_t right_duration(uint32_t tm) const
    {
        return _right_button.duration(tm);
    }

    /**
     * Returns the current orientation of the Circuit Playground.
     */
    Orientation orientation() const { return _orientation; }

    /**
     * Returns a human-readable description of the gesture.
     */
    static const char* to_string(Gesture gesture);

    /**
     * Returns a human-readable description of the orientation.
     */
    static const char* to_string(Orientation orientation);

private:
    // Client code should use instance() to get the singleton.
    CircuitPlaygroundGestures() { }

    // The copy constructor is deleted to prevent instantiating more than one
    // instance. If you get an error related to trying to call this ctor, you
    // probably have a value assignment where you should have a reference
    // assignment, e.g.
    //     auto cpg = CircuitPlaygroundGestures::instance();
    //  should be
    //     auto& cpg = CircuitPlaygroundGestures::instance();
    CircuitPlaygroundGestures(CircuitPlaygroundGestures const&) = delete;

    void accelerometer_interrupt();
    void slide_switch_changed();
    void left_button_changed();
    void right_button_changed();

    volatile bool _slide_switch_reading = false;
    volatile bool _left_button_reading = false;
    volatile bool _right_button_reading = false;
    volatile bool _accelerometer_interrupted = false;
    DebouncedSwitch _slide_switch;
    DebouncedButton _left_button;
    DebouncedButton _right_button;
    uint32_t _tap_ignore_start_tm = 0;
    uint32_t _shake_reset_start_tm = 0;
    Orientation _orientation = UNKNOWN_ORIENTATION;

    struct {
        float x = 0;
        float y = 0;
        float z = 0;

        float total() const { return sqrt(x*x + y*y + z*z); }

        void add_sample(float x, float y, float z)
        {
            this->x = this->x * ACCEL_DECAY + x * (1 - ACCEL_DECAY);
            this->y = this->y * ACCEL_DECAY + y * (1 - ACCEL_DECAY);
            this->z = this->z * ACCEL_DECAY + z * (1 - ACCEL_DECAY);
        }

    } _acceleration;

    static Orientation orientation_for_int1_src(byte int1_src)
    {
        if (int1_src & 0x20) return Z_UP;
        if (int1_src & 0x10) return Z_DOWN;
        if (int1_src & 0x08) return Y_UP;
        if (int1_src & 0x04) return Y_DOWN;
        if (int1_src & 0x02) return X_UP;
        if (int1_src & 0x01) return X_DOWN;
        return UNKNOWN_ORIENTATION;
    }
};

extern CircuitPlaygroundGestures circuit_playground_gestures;

/*---------------------------------------------------------------------------*/

#endif
