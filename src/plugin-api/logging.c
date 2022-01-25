#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "config.h"
#include "paths.h"
#include "nvr.h"
#include "pic.h"
#include "x86.h"

FILE* pclogf;

void fatal(const char* format, ...)
{
        char buf[256];
        //   return;
        if (!pclogf)
        {
                strcpy(buf, logs_path);
                put_backslash(buf);
                strcat(buf, "pcem.log");
                pclogf = fopen(buf, "wt");
        }
        //return;
        va_list ap;
        va_start(ap, format);
        vsprintf(buf, format, ap);
        va_end(ap);
        fputs(buf, pclogf);
        fflush(pclogf);

        savenvr();
        dumppic();
        dumpregs();
        exit(-1);
}

void warning(const char* format, ...)
{
        char buf[1024];
        va_list ap;

        va_start(ap, format);
        vsprintf(buf, format, ap);
        va_end(ap);

        //wx_messagebox(NULL, buf, "PCem", WX_MB_OK); //FIX: Fix it
}

void pclog(const char* format, ...)
{
#ifndef RELEASE_BUILD
        char buf[1024];
        //return;
        if (!pclogf)
        {
                strcpy(buf, logs_path);
                put_backslash(buf);
                strcat(buf, "pcem.log");
                pclogf = fopen(buf, "wt");
        }
        //return;
        va_list ap;
        va_start(ap, format);
        vsprintf(buf, format, ap);
        va_end(ap);
        fputs(buf, pclogf);
        fputs(buf, stdout);
//        fflush(pclogf);
#endif
}