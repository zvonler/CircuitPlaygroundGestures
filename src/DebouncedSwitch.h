
#ifndef debounced_switch_h
#define debounced_switch_h

#include <Arduino.h>

/*---------------------------------------------------------------------------*/

/**
 * Represents a debounced, two-position slide switch.
 */
class DebouncedSwitch
{
public:
    enum Input {
        NONE,
        SWITCHED_ON,
        SWITCHED_OFF,
    };

private:
    // The switch's state must be different for at least this long to cause
    // the debounced state to change.
    static const uint32_t DEBOUNCE_MS = 20;

    bool _on_state;
    bool _debounced_reading = false;
    bool _prev_reading = false;
    uint32_t _last_reading_change_tm = 0;

public:
    /**
     * Creates a DebouncedSwitch with the specified polarity.
     */
    DebouncedSwitch(bool on_state = true)
        : _on_state(on_state)
    { }

    /**
     * Returns true if the switch is in its on state, false otherwise.
     */
    bool state() const { return _debounced_reading == _on_state; }

    /**
     */
    Input update(bool reading, uint32_t tm)
    {
        if (_prev_reading != reading) {
            // Reset the timer whenever the reading doesn't match previous
            _last_reading_change_tm = tm;
            _prev_reading = reading;
            return NONE;
        }

        if (_debounced_reading != reading) {
            // Check if the new reading has persisted long enough
            auto reading_duration = tm - _last_reading_change_tm;
            if (reading_duration > DEBOUNCE_MS) {
                _debounced_reading = reading;
                return _debounced_reading == _on_state ? SWITCHED_ON : SWITCHED_OFF;
            }
        }

        return NONE;
    }
};

/*---------------------------------------------------------------------------*/

#endif
