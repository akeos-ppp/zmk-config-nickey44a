target_sources_ifdef(CONFIG_INPUT_TPS43 app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/tps43.c
    ${CMAKE_CURRENT_LIST_DIR}/src/tps43_idle_sleeper.c
)
