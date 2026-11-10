# Only the nemu-link dongle (split central without IQS9151 haptics) needs it;
# the halves build the driver's real &touch_feedback behavior.
if(CONFIG_BOARD_NEMU_LINK_DONGLE AND NOT CONFIG_INPUT_IQS9151_HAPTIC_FEEDBACK_ENABLE)
  target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/behavior_touch_feedback_relay.c)
endif()
