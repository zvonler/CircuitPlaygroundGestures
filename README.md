
# Circuit Playground Gestures

This Arduino library implements a gesture recognizer for the Adafruit Circuit
Playground that uses the built-in accelerometer, slide switch, and buttons to
detect user gestures such as button presses, switch changes, shakes and taps,
and more.

## Gestures recognized

The accelerometer is used to detect
  * double taps
  * shakes
  * orientation changes

The slide switch is used to detect
  * switch turned on
  * switch turned off

The two pushbuttons are used to detect
  * either button clicked
  * either button double-clicked
  * either button held for a long press
  * either button clicked while the other is held
  * either button released

## Theory of operation

Client applications can use the gesture recognizer by calling its `begin`
method as part of their `setup()` function, and by calling its `update` method
frequently as part of the client's `loop()` function. The `update` method
returns an enumerated value indicating if any user gestures were detected
during that update cycle.

## Examples

The `GestureToSerial` sketch in the examples directory demonstrates how an
application can use the library to detect user input gestures in the `loop`
function. The sketch prints a description of the received gestures to the serial
port.

Example Serial Monitor output from `GestureToSerial`:

<img width="514" alt="Screen Shot 2023-10-11 at 11 02 58" src="https://github.com/zvonler/CircuitPlaygroundGestures/assets/19316003/110b8e1e-d707-4f08-a2e5-a4feaf620fb8">
