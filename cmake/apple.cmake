
if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
        list(LENGTH SDL2_INCLUDE_DIRS SDL2_INCLUDE_DIRS_LEN)
        if (SDL2_INCLUDE_DIRS_LEN EQUAL 1)
                get_filename_component(SDL2_INCLUDE_PARENT_DIR ${SDL2_INCLUDE_DIRS} DIRECTORY)
                include_directories(${SDL2_INCLUDE_DIRS} ${SDL2_INCLUDE_PARENT_DIR})
        endif()
        add_compile_definitions(HAVE_UNISTD_H)

        add_compile_definitions(fseeko64=fseeko off64_t=off_t fopen64=fopen ftello64=ftell)
endif()

