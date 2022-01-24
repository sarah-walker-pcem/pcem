set(PCEM_SRC_SOUND
        sound/sound.c
        sound/sound_ad1848.c
        sound/sound_adlib.c
        sound/sound_adlibgold.c
        sound/sound_audiopci.c
        sound/sound_azt2316a.c
        sound/sound_cms.c
        sound/sound_dbopl.cc
        sound/sound_emu8k.c
        sound/sound_gus.c
        sound/sound_mpu401_uart.c
        sound/sound_opl.c
        sound/sound_pas16.c
        sound/sound_ps1.c
        sound/sound_pssj.c
        sound/sound_resid.cc
        sound/sound_sb.c
        sound/sound_sb_dsp.c
        sound/sound_sn76489.c
        sound/sound_speaker.c
        sound/sound_ssi2001.c
        sound/sound_wss.c
        sound/sound_ym7128.c
        sound/soundopenal.c
        )

# RESID-FP
set(PCEM_SRC_SOUND ${PCEM_SRC_SOUND}
        sound/resid-fp/convolve.cc
        sound/resid-fp/convolve-sse.cc
        sound/resid-fp/envelope.cc
        sound/resid-fp/extfilt.cc
        sound/resid-fp/filter.cc
        sound/resid-fp/pot.cc
        sound/resid-fp/sid.cc
        sound/resid-fp/voice.cc
        sound/resid-fp/wave6581_PS_.cc
        sound/resid-fp/wave6581_PST.cc
        sound/resid-fp/wave6581_P_T.cc
        sound/resid-fp/wave6581__ST.cc
        sound/resid-fp/wave8580_PS_.cc
        sound/resid-fp/wave8580_PST.cc
        sound/resid-fp/wave8580_P_T.cc
        sound/resid-fp/wave8580__ST.cc
        sound/resid-fp/wave.cc
        )

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux" AND USE_ALSA)
        set(PCEM_SRC_SOUND ${PCEM_SRC_SOUND}
                sound/midi_alsa.c
                )
        set(PCEM_ADDITIONAL_LIBS ${PCEM_ADDITIONAL_LIBS} ${ALSA_LIBRARIES})
else()
        set(PCEM_SRC_SOUND ${PCEM_SRC_SOUND}
                sound/sdl2-midi.c
                )
endif()