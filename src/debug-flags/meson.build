if get_option('buildtype').startswith('debug')
        if get_option('slirp-debug')
                add_project_arguments('-DSLIRP_DEBUG', language: ['c', 'cpp'])
        endif
        if get_option('recompiler-debug')
                add_project_arguments('-DRECOMPILER_DEBUG', language: ['c', 'cpp'])
        endif
        if get_option('ne2000-debug')
                add_project_arguments('-DNE2000_DEBUG', language: ['c', 'cpp'])
        endif
        if get_option('emu8k-debug-registers')
                add_project_arguments('-DEMU8K_DEBUG_REGISTERS', language: ['c', 'cpp'])
        endif
        if get_option('sb-dsp-record-debug')
                add_project_arguments('-DSB_DSP_RECORD_DEBUG', language: ['c', 'cpp'])
        endif
        if get_option('mach64-debug')
                add_project_arguments('-DMACH64_DEBUG', language: ['c', 'cpp'])
        endif
        if get_option('plugin-debug')
                add_project_arguments('-DPLUGIN_DEBUG', language: ['c', 'cpp'])
        endif
        if get_option('extra-debug')
                add_project_arguments('-DDEBUG_EXTRA', language: ['c', 'cpp'])
        endif
        if get_option('printer-debug')
                add_project_arguments('-DPRINTER_DEBUG', language: ['c', 'cpp'])
        endif
else
        add_project_arguments('-DRELEASE_BUILD', language: ['c', 'cpp'])
endif