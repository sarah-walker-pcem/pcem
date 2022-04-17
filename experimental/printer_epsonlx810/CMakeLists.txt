find_package(Freetype REQUIRED)
include_directories(${FREETYPE_INCLUDE_DIRS})

set(EPSON_LX810_HEADER
        ${CMAKE_SOURCE_DIR}/includes/private/lpt/lpt_epsonlx810.h
        )

set(EPSON_LX810_SRC
        wx-imagesave.cc
        lpt_epsonlx810.c
        plugin.c
        )

add_library(printer_epsonlx810 SHARED ${EPSON_LX810_SRC} ${EPSON_LX810_HEADER})
target_link_libraries(printer_epsonlx810 ${PCEM_LIBRARIES} Freetype::Freetype)
set_target_properties(printer_epsonlx810 PROPERTIES SUFFIX ".pplg")