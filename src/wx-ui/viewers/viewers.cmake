set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/viewer.h
        )

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/wx-ui/viewers)

set(PCEM_SRC ${PCEM_SRC}
        wx-ui/viewers/viewer.cc
        wx-ui/viewers/viewer_font.cc
	wx-ui/viewers/viewer_palette.cc
        wx-ui/viewers/viewer_voodoo.cc
	wx-ui/viewers/viewer_vram.cc
        )
