#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>

#include "config.h"
#include "paths.h"
#include "plugin.h"

void (*_savenvr)();
void (*_dumppic)();
void (*_dumpregs)();

FILE *pclogf = NULL;

uint8_t pclog_start() {
#ifndef RELEASE_BUILD
        if (!pclogf) {
                char buf[1024];
                strcpy(buf, logs_path);
                put_backslash(buf);
                strcat(buf, "pcem.log");
                pclogf = fopen(buf, "wt");

                if (NULL == pclogf) {
                        fprintf(stderr, "Could not open log file for writing: %s", strerror(errno));
                        return 0;
                }
        }
        return 1;
#else
        return 0;
#endif
}

void pclog_flush() {
#ifndef RELEASE_BUILD
        if (pclogf) {
                fflush(pclogf);
        }
#endif
}

void pclog_end() {
#ifndef RELEASE_BUILD
        if (pclogf) {
                fflush(pclogf);
                fclose(pclogf);
                pclogf = NULL;
        }
#endif
}

void error(const char *format, ...) {
#ifndef RELEASE_BUILD
        char buf[1024];
        // return;
        if (!pclog_start()) {
                return;
        }
        // return;
        va_list ap;
        va_start(ap, format);
        vsprintf(buf, format, ap);
        va_end(ap);
        fputs(buf, pclogf);
        fputs(buf, stderr);
        // fflush(pclogf);
#endif
}

void fatal(const char *format, ...) {
#ifndef RELEASE_BUILD
        char buf[1024];
        // return;
        if (!pclog_start()) {
                return;
        }
        // return;
        va_list ap;
        va_start(ap, format);
        vsprintf(buf, format, ap);
        va_end(ap);
        fputs(buf, pclogf);
        fputs(buf, stderr);
        fflush(pclogf);
#endif

        _savenvr();
        _dumppic();
        _dumpregs();
        pclog_end();
        exit(-1);
}

void warning(const char *format, ...) {
        char buf[1024];
        va_list ap;

        va_start(ap, format);
        vsprintf(buf, format, ap);
        va_end(ap);

        // wx_messagebox(NULL, buf, "PCem", WX_MB_OK); //FIX: Fix it
}

void pclog(const char *format, ...) {
#ifndef RELEASE_BUILD
        char buf[1024];
        // return;
        if (!pclog_start()) {
                return;
        }
        // return;
        va_list ap;
        va_start(ap, format);
        vsprintf(buf, format, ap);
        va_end(ap);
        fputs(buf, pclogf);
        fputs(buf, stdout);
        // fflush(pclogf);
#endif
}
