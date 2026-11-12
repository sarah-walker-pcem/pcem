include(BundleUtilities)

function(gp_item_default_embedded_path_override item default_embedded_path_var)
        set(${default_embedded_path_var} "@executable_path/../Frameworks" PARENT_SCOPE)
endfunction()

string(REPLACE "|" ";" PCEM_BUNDLE_LIB_DIRS "${LIB_DIRS}")
set(BU_CHMOD_BUNDLE_ITEMS ON)

fixup_bundle("${APP}" "" "${PCEM_BUNDLE_LIB_DIRS}")

set(FRAMEWORKS_DIR "${APP}/Contents/Frameworks")
file(GLOB PCEM_BUNDLE_SDL2 "${FRAMEWORKS_DIR}/libSDL2*.dylib")
foreach(sdl2 ${PCEM_BUNDLE_SDL2})
        file(STRINGS "${sdl2}" PCEM_SDL2_COMPAT REGEX "@loader_path/libSDL3.dylib" LIMIT_COUNT 1)
        if(PCEM_SDL2_COMPAT)
                if(NOT SDL3_LIBRARY OR NOT EXISTS "${SDL3_LIBRARY}")
                        message(FATAL_ERROR "SDL2 is sdl2-compat but the SDL3 library was not found")
                endif()
                get_filename_component(PCEM_SDL3_REAL "${SDL3_LIBRARY}" REALPATH)
                file(COPY_FILE "${PCEM_SDL3_REAL}" "${FRAMEWORKS_DIR}/libSDL3.dylib" ONLY_IF_DIFFERENT)
                file(CHMOD "${FRAMEWORKS_DIR}/libSDL3.dylib" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
                execute_process(COMMAND install_name_tool -id "@executable_path/../Frameworks/libSDL3.dylib"
                                "${FRAMEWORKS_DIR}/libSDL3.dylib" ERROR_QUIET)
        endif()
endforeach()

execute_process(COMMAND codesign --force --deep --sign - "${APP}" RESULT_VARIABLE PCEM_CODESIGN_RESULT)
if(NOT PCEM_CODESIGN_RESULT EQUAL 0)
        message(FATAL_ERROR "codesign failed for ${APP}")
endif()
