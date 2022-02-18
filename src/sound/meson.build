pcem_private_api += files(
        '../../includes/private/sound/resid-fp/envelope.h',
        '../../includes/private/sound/resid-fp/extfilt.h',
        '../../includes/private/sound/resid-fp/filter.h',
        '../../includes/private/sound/resid-fp/pot.h',
        '../../includes/private/sound/resid-fp/siddefs-fp.h',
        '../../includes/private/sound/resid-fp/sid.h',
        '../../includes/private/sound/resid-fp/voice.h',
        '../../includes/private/sound/resid-fp/wave.h',
        '../../includes/private/sound/sound_ad1848.h',
        '../../includes/private/sound/sound_adlibgold.h',
        '../../includes/private/sound/sound_adlib.h',
        '../../includes/private/sound/sound_audiopci.h',
        '../../includes/private/sound/sound_azt2316a.h',
        '../../includes/private/sound/sound_cms.h',
        '../../includes/private/sound/sound_dbopl.h',
        '../../includes/private/sound/sound_emu8k.h',
        '../../includes/private/sound/sound_gus.h',
        '../../includes/private/sound/sound.h',
        '../../includes/private/sound/sound_mpu401_uart.h',
        '../../includes/private/sound/sound_opl.h',
        '../../includes/private/sound/sound_pas16.h',
        '../../includes/private/sound/sound_ps1.h',
        '../../includes/private/sound/sound_pssj.h',
        '../../includes/private/sound/sound_resid.h',
        '../../includes/private/sound/sound_sb_dsp.h',
        '../../includes/private/sound/sound_sb.h',
        '../../includes/private/sound/sound_sn76489.h',
        '../../includes/private/sound/sound_speaker.h',
        '../../includes/private/sound/sound_ssi2001.h',
        '../../includes/private/sound/sound_wss.h',
        '../../includes/private/sound/sound_ym7128.h'
        )

pcem_src += files(
        'sound.c',
        'sound_ad1848.c',
        'sound_adlib.c',
        'sound_adlibgold.c',
        'sound_audiopci.c',
        'sound_azt2316a.c',
        'sound_cms.c',
        'sound_dbopl.cc',
        'sound_emu8k.c',
        'sound_gus.c',
        'sound_mpu401_uart.c',
        'sound_opl.c',
        'sound_pas16.c',
        'sound_ps1.c',
        'sound_pssj.c',
        'sound_resid.cc',
        'sound_sb.c',
        'sound_sb_dsp.c',
        'sound_sn76489.c',
        'sound_speaker.c',
        'sound_ssi2001.c',
        'sound_wss.c',
        'sound_ym7128.c',
        'soundopenal.c'
        )

# RESID-FP
pcem_src += files(
        'resid-fp/convolve.cc',
        'resid-fp/convolve-sse.cc',
        'resid-fp/envelope.cc',
        'resid-fp/extfilt.cc',
        'resid-fp/filter.cc',
        'resid-fp/pot.cc',
        'resid-fp/sid.cc',
        'resid-fp/voice.cc',
        'resid-fp/wave6581_PS_.cc',
        'resid-fp/wave6581_PST.cc',
        'resid-fp/wave6581_P_T.cc',
        'resid-fp/wave6581__ST.cc',
        'resid-fp/wave8580_PS_.cc',
        'resid-fp/wave8580_PST.cc',
        'resid-fp/wave8580_P_T.cc',
        'resid-fp/wave8580__ST.cc',
        'resid-fp/wave.cc'
        )

if target_machine.system() == 'linux' and get_option('use-alsa')
        alsa = dependency('alsa')
        pcem_dependencies += alsa
        pcem_src += files(
            'midi_alsa.c'
            )
else
        pcem_src += files(
            'sdl2-midi.c'
            )
endif