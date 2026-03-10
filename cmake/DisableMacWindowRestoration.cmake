if(NOT EXISTS "${PLIST_PATH}")
    message(FATAL_ERROR "Missing plist: ${PLIST_PATH}")
endif()

execute_process(
    COMMAND /usr/libexec/PlistBuddy -c "Delete :NSQuitAlwaysKeepsWindows" "${PLIST_PATH}"
    RESULT_VARIABLE delete_result
    OUTPUT_QUIET
    ERROR_QUIET
)

execute_process(
    COMMAND /usr/libexec/PlistBuddy -c "Add :NSQuitAlwaysKeepsWindows bool false" "${PLIST_PATH}"
    RESULT_VARIABLE add_result
)

if(NOT add_result EQUAL 0)
    message(FATAL_ERROR "Failed to update plist: ${PLIST_PATH}")
endif()
