# Touch-release wheel inertia

An independent MIT-licensed ZMK input processor. It observes a relative wheel
axis and a touch key without modifying or consuming input events. A real touch
release starts integer-only Q8 inertia; a new touch cancels it immediately.
The source is newly written from the requested algorithm, without copying
IQS7211E source, comments, or function organization.

## Integration

This directory is a standalone Zephyr module. In Nickey44A it is temporarily
bundled through the configuration repository's CMakeLists.txt and dts_root.
There is no invented remote commit SHA in west.yml.

To publish, create `nixiy/zmk-input-processor-touch-inertia`, commit the contents
of this directory at its root, and push. Add the project to west.yml with the
actual full commit SHA and the nixiy remote. Then remove the configuration
repository's bundling CMakeLists.txt and its `cmake`/`dts_root` module entries.
Use either the bundled module or the west project, never both.

```dts
touch_inertia: touch_inertia {
    compatible = "zmk,input-processor-touch-inertia";
    #input-processor-cells = <0>;
    wheel-code = <INPUT_REL_WHEEL>;
    touch-code = <INPUT_BTN_TOUCH>;
    tick-ms = <10>;
    decay-permille = <920>;
    stop-threshold-q8 = <96>;
    ema-new-permille = <250>;
};
```

Place the node only on the central side, and append it to input-processors
immediately after orientation correction. HWHEEL and REL_MISC are untouched
when wheel-code is INPUT_REL_WHEEL. Touch events also pass through unchanged.
Use one instance for one listener/device; state and synchronization are per
instance. Split peripherals with no instance compile no mouse HID code.

## Behavior and limits

Nonzero wheel samples during touch are normalized to wheel units per tick.
The first sample uses delta times 256; subsequent samples use measured uptime
intervals clamped to at least 1 ms. EMA applies even to the first sample.
Release without a wheel sample does nothing. Repeated release events do not
restart inertia. Wheel input cancels an existing coast, including zero input.

Each delayed work tick accumulates fractional motion, emits only vertical
scroll directly through ZMK HID, and multiplies speed by decay-permille/1000.
Direct HID output bypasses downstream processors. Do not place a wheel scaler
after this processor if normal and inertial wheel units must match.
Reports are limited to +/-127; excess is retained while inertia is running.
The instance mutex serializes touch reset with work/report emission. Input
callbacks must run in thread context (the normal Zephyr input listener path).

tick-ms=0 becomes 1. Negative tick values, decay outside 0..999, nonpositive
stop threshold, and EMA weights outside 0..1000 fail at compile time.
No floating point, release timeout, flick classification, or axis guessing.

## Tuning and hardware checks

Defaults: 10 ms, decay 920, threshold 96 Q8 (0.375 units/tick), EMA weight 250.
Raise decay to 940/960 for longer inertia; lower to 900/880 for shorter inertia.
Raise threshold to 128 for an earlier stop; lower to 64 for a longer tail.
Lower EMA weight to 200 for more smoothing; raise to 350 for quicker response.

Verify slow scroll, both flick directions, exponential stopping, re-touch
cancellation, and reversal after cancellation. Tap, hold, touch alone, pinch
alone, and horizontal swipe alone must never initiate vertical inertia.
Confirm Back/Forward and pinch zoom remain unaffected if processors exist.
Check deep sleep/wake followed by scrolling and flicking. No activity-state
subscription is included; add one only if hardware testing demonstrates a
stale state across sleep. Firmware build success does not verify touch feel
or power-management behavior.
