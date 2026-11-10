target_sources_ifdef(CONFIG_NICKEY_BATTERY_VOLTAGE_DIVIDER_THRESHOLDS app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/battery_voltage_divider_thresholds.c
)
