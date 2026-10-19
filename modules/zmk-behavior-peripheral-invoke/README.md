# zmk-behavior-peripheral-invoke

Runs one source-local behavior (for example `&bootloader` or `&sys_reset`) on a
fixed split peripheral. ZMK combos always use the central as their event
source, so `&bootloader` in a combo can only reset the central; wrapping it in
this behavior sends it to the peripheral instead.

```dts
boot_left: boot_left {
    compatible = "zmk,behavior-peripheral-invoke";
    #binding-cells = <0>;
    bindings = <&bootloader>;
    peripheral = <0>;   /* index of the peripheral (first and only = 0) */
};
```

The peripheral must be connected to the central for the invocation to arrive.
