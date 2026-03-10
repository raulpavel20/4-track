set(candidate_paths
    "${BUILD_DIR}/FourTrackTape_artefacts/4-Track.app/Contents/Info.plist"
    "${BUILD_DIR}/FourTrackTape_artefacts/Release/4-Track.app/Contents/Info.plist"
    "${BUILD_DIR}/FourTrackTape_artefacts/Debug/4-Track.app/Contents/Info.plist"
    "${BUILD_DIR}/FourTrackTape_artefacts/RelWithDebInfo/4-Track.app/Contents/Info.plist"
    "${BUILD_DIR}/FourTrackTape_artefacts/MinSizeRel/4-Track.app/Contents/Info.plist"
)

unset(PLIST_PATH)

foreach(candidate_path IN LISTS candidate_paths)
    if(EXISTS "${candidate_path}")
        set(PLIST_PATH "${candidate_path}")
        break()
    endif()
endforeach()

if(NOT DEFINED PLIST_PATH)
    message(FATAL_ERROR "Missing plist in ${BUILD_DIR}/FourTrackTape_artefacts")
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
