# Pinch zoom (held button + scroll)

Local MIT-licensed processor for the right TPS43 listener, ordered after
three-finger swipe and before touch-inertia. The vendored driver's
`soft-pinch` reports the change of the distance between two fingers as
REL_MISC, in permille of the X resolution (the controller's own `zoom`
gesture did not work on the Nickey44A pad).

This processor accumulates that signed amount and sends one vertical wheel
tick per `step` units (fingers apart = wheel up) while holding
`hold-buttons` (mouse button flags) and/or `modifier-keycode`. Both are
optional. They are pressed with the first tick and released by the next
non-pinch event (touch up, a finger lifting into cursor motion, a click). The
wheel goes straight to the HID report so touch-inertia never continues it
after lift. REL_MISC is always consumed; every other event passes unchanged.

Nickey44A uses hold-buttons = <MB5> with step=30, the same output as the
LalaPad Gen2 (IQS9151) Mac layer. For Option+scroll, use
modifier-keycode = <LALT> instead. If the zoom direction is reversed, add
`invert-wheel`. Raise step for slower zoom, lower it for faster. The compatible
keeps its original name (`zmk,input-processor-pinch-keys`); the binding lives
in the shield's dts/bindings directory, like the other bundled processors.
