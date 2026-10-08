# Pinch keys

Local MIT-licensed processor for the right TPS43 listener, ordered after
three-finger swipe and before touch-inertia. The vendored driver reports the
IQS5xx zoom gesture amount as REL_MISC when the `zoom` property is set.
This processor accumulates that signed amount and taps zoom-in-keycode once
per `step` positive units and zoom-out-keycode once per `step` negative
units, repeating while the pinch continues. TOUCH up/down clears the
accumulator. REL_MISC is always consumed; every other event passes unchanged.

Nickey44A uses LG(EQUAL)/LG(MINUS) (macOS Cmd+= / Cmd+-) with step=120.
If pinching out zooms out, swap the keycodes. Raise step for fewer zoom
steps per pinch, lower it for more. The binding lives in the shield's
dts/bindings directory, like the other bundled processors.
