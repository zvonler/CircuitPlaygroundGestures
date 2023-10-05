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

#include "CircuitPlaygroundGestures.h"

#include <Adafruit_CircuitPlayground.h>
#include <Wire.h>

/*---------------------------------------------------------------------------*/

namespace {

unsigned int readRegister(byte reg) {
    Wire1.beginTransmission(CPLAY_LIS3DH_ADDRESS);
    Wire1.write(reg);
    Wire1.endTransmission();
    Wire1.requestFrom(CPLAY_LIS3DH_ADDRESS, 1);
    return Wire1.read();
}

void writeRegister(byte reg, byte data) {
    Wire1.beginTransmission(CPLAY_LIS3DH_ADDRESS);
    Wire1.write(reg);
    Wire1.write(data);
    Wire1.endTransmission();
}

} // anonymous namespace

/*---------------------------------------------------------------------------*/

/**
 * This struct exists as a friend to CircuitPlaygroundGestures so that the
 * methods it calls on CircuitPlaygroundGestures do not have to be public.
 */
struct CPGAccessor
{
    static void accelerometer_ISR()
    {
        CircuitPlaygroundGestures::instance().accelerometer_interrupt();
    }

    static void slide_switch_ISR()
    {
        CircuitPlaygroundGestures::instance().slide_switch_changed();
    }

    static void left_button_ISR()
    {
        CircuitPlaygroundGestures::instance().left_button_changed();
    }

    static void right_button_ISR()
    {
        CircuitPlaygroundGestures::instance().right_button_changed();
    }
};

/*---------------------------------------------------------------------------*/

CircuitPlaygroundGestures&
CircuitPlaygroundGestures::instance()
{
    static CircuitPlaygroundGestures cpg;
    return cpg;
}

void
CircuitPlaygroundGestures::begin()
{
    Adafruit_CPlay_LIS3DH& lis = CircuitPlayground.lis;

    writeRegister(LIS3DH_REG_CTRL1, 0x77);        // Enable X, Y, Z axes with ODR = 400Hz normal mode
    writeRegister(LIS3DH_REG_CTRL2, 0xC4);        // HPF auto-reset on interrupt and enabled for CLICK
    writeRegister(LIS3DH_REG_CTRL3, 0xC0);        // Click and IA1 interrupt signal routed to INT1 pin
    writeRegister(LIS3DH_REG_CTRL4, 0x00);        // Full Scale = +/-2 g
    writeRegister(LIS3DH_REG_CTRL5, 0x00);        // No interrupt latching

    writeRegister(LIS3DH_REG_INT1DUR, 0x7F);
    writeRegister(LIS3DH_REG_INT1CFG, 0xFF);      // 6D orientation detection on all axes
    writeRegister(LIS3DH_REG_INT1THS, 0x32);

    writeRegister(LIS3DH_REG_CLICKCFG, 0x2A);     // Double tap on any axis
    writeRegister(LIS3DH_REG_CLICKTHS, 0x48);
    writeRegister(LIS3DH_REG_TIMELIMIT, 0x06);
    writeRegister(LIS3DH_REG_TIMELATENCY, 0x50);
    writeRegister(LIS3DH_REG_TIMEWINDOW, 0x70);

    attachInterrupt(digitalPinToInterrupt(CPLAY_LIS3DH_INTERRUPT), CPGAccessor::accelerometer_ISR, RISING);

    _slide_switch_reading = digitalRead(CPLAY_SLIDESWITCHPIN);
    attachInterrupt(digitalPinToInterrupt(CPLAY_SLIDESWITCHPIN), CPGAccessor::slide_switch_ISR, CHANGE);

    _left_button_reading = digitalRead(CPLAY_LEFTBUTTON);
    attachInterrupt(digitalPinToInterrupt(CPLAY_LEFTBUTTON), CPGAccessor::left_button_ISR, CHANGE);

    _right_button_reading = digitalRead(CPLAY_RIGHTBUTTON);
    attachInterrupt(digitalPinToInterrupt(CPLAY_RIGHTBUTTON), CPGAccessor::right_button_ISR, CHANGE);
}

CircuitPlaygroundGestures::Gesture
CircuitPlaygroundGestures::update(uint32_t tm)
{
    auto status_reg = readRegister(LIS3DH_REG_STATUS2);

    if (_accelerometer_interrupted) {
        _accelerometer_interrupted = false;

        auto click_src = readRegister(LIS3DH_REG_CLICKSRC);
        if (click_src & 0x60) {
            // Only return a tapped event if enough time has elapsed since the
            // last button or switch input.
            if (tm - _tap_ignore_start_tm > TAP_IGNORE_MS)
                return DOUBLE_TAPPED;
        }

        auto int1_src = readRegister(LIS3DH_REG_INT1SRC);
        if (int1_src & 0x3F) {
            auto new_orientation = orientation_for_int1_src(int1_src);
            if (_orientation != new_orientation) {
                _orientation = new_orientation;
                return ORIENTATION_CHANGED;
            }
        }
    } else if (status_reg & 0x08) { // This bit indicates new data is available on all three axes
        Adafruit_CPlay_LIS3DH& lis = CircuitPlayground.lis;
        sensors_event_t event;
        lis.getEvent(&event);
        auto &acc = event.acceleration;
        _acceleration.add_sample(acc.x, acc.y, acc.z);

        if (_acceleration.total() > TAP_IGNORE_THRESHOLD) {
            // Anytime acceleration is over threshold ignore taps
            _tap_ignore_start_tm = tm;
        }

        if (_acceleration.total() > SHAKE_THRESHOLD) {
            if (tm - _shake_reset_start_tm >= SHAKE_RESET_MS) {
                _shake_reset_start_tm = tm;
                return SHAKEN;
            }

            // Ignore other inputs while being shaken
            return NONE;
        }
    }

    if (_left_button.input_pending() || _right_button.input_pending()) {
        // Begin ignoring taps because the buttons see activity.
        _tap_ignore_start_tm = tm;
    }

    auto switch_input = _slide_switch.update(_slide_switch_reading, tm);
    if (switch_input != DebouncedSwitch::NONE) {
        // Prevent reacting to taps that are really switch moves.
        _tap_ignore_start_tm = tm;
        return switch_input == DebouncedSwitch::SWITCHED_ON ? SLIDE_SWITCHED_ON : SLIDE_SWITCHED_OFF;
    }

    auto left_input = _left_button.update(_left_button_reading, tm);
    auto right_input = _right_button.update(_right_button_reading, tm);

    if (left_input == DebouncedButton::NONE && right_input == DebouncedButton::NONE)
        return NONE;

    // One or more of the buttons had some kind of input, so note this time to
    // prevent reacting to taps that are really button presses/releases.
    _tap_ignore_start_tm = tm;

    auto left_held = _left_button.state() && _left_button.duration(tm) > DebouncedButton::CLICKED_CUTOFF_MS;
    auto right_held = _right_button.state() && _right_button.duration(tm) > DebouncedButton::CLICKED_CUTOFF_MS;

    if (right_input == DebouncedButton::NONE) {
        if (left_input == DebouncedButton::CLICK)
            return right_held ? RIGHT_HELD_LEFT_CLICKED : LEFT_CLICKED;
        if (left_input == DebouncedButton::LONG_PRESS)
            return right_held ? BOTH_PRESSED : LEFT_PRESSED;
        if (left_input == DebouncedButton::RELEASE)
            return LEFT_RELEASED;
        if (left_input == DebouncedButton::DOUBLE_CLICK)
            return LEFT_DOUBLE_CLICKED;
        return NONE;
    }

    if (left_input == DebouncedButton::NONE) {
        if (right_input == DebouncedButton::CLICK)
            return left_held ? LEFT_HELD_RIGHT_CLICKED : RIGHT_CLICKED;
        if (right_input == DebouncedButton::LONG_PRESS)
            return left_held ? BOTH_PRESSED : RIGHT_PRESSED;
        if (right_input == DebouncedButton::RELEASE)
            return RIGHT_RELEASED;
        if (right_input == DebouncedButton::DOUBLE_CLICK)
            return RIGHT_DOUBLE_CLICKED;
        return NONE;
    }

    // If both buttons found an input, prioritize the right one but check for
    // the same simultaneous input on left.
    if (right_input == DebouncedButton::LONG_PRESS)
        return left_input == DebouncedButton::LONG_PRESS ? BOTH_PRESSED : RIGHT_PRESSED;
    if (right_input == DebouncedButton::RELEASE)
        return left_input == DebouncedButton::RELEASE ? BOTH_RELEASED : RIGHT_RELEASED;
    if (right_input == DebouncedButton::CLICK)
        return left_input == DebouncedButton::CLICK ? BOTH_CLICKED : RIGHT_CLICKED;

    // Other inputs are ignored
    return NONE;
}

const char*
CircuitPlaygroundGestures::to_string(Gesture gesture)
{
    switch (gesture) {
        case NONE:                    return "none";
        case ORIENTATION_CHANGED:     return "orientation changed";
        case SHAKEN:                  return "shaken";
        case DOUBLE_TAPPED:           return "double tapped";
        case SLIDE_SWITCHED_ON:       return "slide switch turned on";
        case SLIDE_SWITCHED_OFF:      return "slide switch turned off";
        case LEFT_PRESSED:            return "left button pressed";
        case RIGHT_PRESSED:           return "right button pressed";
        case LEFT_CLICKED:            return "left button clicked";
        case RIGHT_CLICKED:           return "right button clicked";
        case LEFT_RELEASED:           return "left button released";
        case RIGHT_RELEASED:          return "right button released";
        case BOTH_CLICKED:            return "both buttons clicked";
        case BOTH_PRESSED:            return "both buttons pressed";
        case BOTH_RELEASED:           return "both buttons released";
        case LEFT_HELD_RIGHT_CLICKED: return "right button clicked while left button pressed";
        case RIGHT_HELD_LEFT_CLICKED: return "left button clicked while right button pressed";
        case LEFT_DOUBLE_CLICKED:     return "left button double clicked";
        case RIGHT_DOUBLE_CLICKED:    return "right button double clicked";
        default:                      return "unknown";
    }
    // Unreachable
}

const char*
CircuitPlaygroundGestures::to_string(Orientation orientation)
{
    if (orientation == Z_UP)   return "Z UP";
    if (orientation == Z_DOWN) return "Z DOWN";
    if (orientation == Y_UP)   return "Y UP";
    if (orientation == Y_DOWN) return "Y DOWN";
    if (orientation == X_UP)   return "X UP";
    if (orientation == X_DOWN) return "X DOWN";
    return "UNKNOWN";
}

void
CircuitPlaygroundGestures::accelerometer_interrupt()
{
    _accelerometer_interrupted = true;
}

void
CircuitPlaygroundGestures::slide_switch_changed()
{
    _slide_switch_reading = digitalRead(CPLAY_SLIDESWITCHPIN);
}

void
CircuitPlaygroundGestures::left_button_changed()
{
    _left_button_reading = digitalRead(CPLAY_LEFTBUTTON);
}

void
CircuitPlaygroundGestures::right_button_changed()
{
    _right_button_reading = digitalRead(CPLAY_RIGHTBUTTON);
}

/*---------------------------------------------------------------------------*/
