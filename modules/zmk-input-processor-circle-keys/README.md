# Circle keys

Local MIT-licensed processor. It observes one-finger REL_X/REL_Y on a
central-side listener and taps `clockwise-keycode` or
`counterclockwise-keycode` for every `step-degrees` of accumulated turning.
Motion is grouped into segments of `segment-distance`; the heading change
between consecutive segments is summed. A change larger than
`max-turn-degrees` (corner or reversal) clears the sum, so straight lines and
zigzags do not fire. TOUCH events and a motion gap of `idle-reset-ms` reset
all state. Events always continue unchanged; Nickey44A zeroes X/Y afterwards.

Coordinates are host-style (X right, Y down), so a positive heading change is
clockwise. Nickey44A maps the left pad to C_VOL_UP (clockwise) and C_VOL_DN.
If the direction is reversed on hardware, swap the keycodes. Lower
step-degrees for faster volume changes; raise segment-distance if jitter
fires steps. The binding is mirrored in the shield's dts/bindings directory.
