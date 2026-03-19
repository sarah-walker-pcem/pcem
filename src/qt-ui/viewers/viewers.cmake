set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/qt-ui/viewer.h
        )

set(PCEM_SRC ${PCEM_SRC}
        qt-ui/viewers/viewer.cc
        qt-ui/viewers/viewer_font.cc
        qt-ui/viewers/viewer_palette.cc
        qt-ui/viewers/viewer_voodoo.cc
        qt-ui/viewers/viewer_vram.cc
        )
