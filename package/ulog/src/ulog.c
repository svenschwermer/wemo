/*
 * Copyright (c) 2013 by Belkin International, Inc. All Rights Reserved.
 *
 * This work is subject to U.S. and international copyright laws and
 * treaties. No part of this work may be used, practiced, performed,
 * copied, distributed, revised, modified, translated, abridged, condensed,
 * expanded, collected, compiled, linked, recast, transformed or adapted
 * without the prior written consent of Cisco Systems, Inc. Any use or
 * exploitation of this work without authorization could subject the
 * perpetrator to criminal and civil liability.
 */

/*
===================================================================
    This library provide logging API and routines to filter logs
    based on defined component.subcomponent
===================================================================
*/


#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <pthread.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "defines.h"
#include "ulog.h"

#define ULOG_FILE   "/var/log/messages"
#define ULOG_IDENT  "WEMO"

#define ULOG_CONSOLE            "/tmp/Belkin_settings/debug.console"
#define ULOG_UPNP_CONSOLE       "/tmp/Belkin_settings/debug.upnp.console"
#define ULOG_LINK_CONSOLE       "/tmp/Belkin_settings/link.console"

clock_t t;
struct tms tmp;

typedef struct ucomp {
    char *name;
    int   id;
} ucomp_t;

ucomp_t components[] =
  {
    { "system",   ULOG_SYSTEM },
    { "core",     ULOG_CORE},
    { "app",      ULOG_APP},
    { "lan",      ULOG_LAN },
    { "wlan",     ULOG_WLAN },
    { "config",   ULOG_CONFIG },
    { "service",  ULOG_SERVICE },
    { "upnp",     ULOG_UPNP },
    { "remote",   ULOG_REMOTE },
#ifndef PRODUCT_WeMo_LEDLight
    { "db",       ULOG_DB},
#else
    { "rule",     ULOG_RULE},
#endif
    { "fwupd",    ULOG_FIRMWARE_UPDATE },
    { NULL, -1 },
  };

ucomp_t subcomponents[] =
  {
    { "info",     UL_INFO },
    { "debug",    UL_DEBUG },
    { "error",    UL_ERROR },
    { "status",   UL_STATUS },
    { "sysevent", UL_SYSEVENT },
    { NULL, -1 },
  };

static const char *getsubcomp (int subcomp)
{
    int i = 0;
    while (subcomponents[i].name) {
        if (subcomponents[i].id == subcomp) {
            return subcomponents[i].name;
        }
        i++;
    }
    return "unknown";
}

static const char *getcomp (int comp)
{
    int i = 0;
    while (components[i].name) {
        if (components[i].id == comp) {
            return components[i].name;
        }
        i++;
    }
    return "unknown";
}

/*
 * This mutex serializes syslog() call to make thread-safe.
 * Currently one mutex is used per-process
 */
pthread_mutex_t g_syslog_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef PRODUCT_WeMo_LEDLight
void ulog_set_output()
{
    struct stat     s;

    ulog_debug_brief_output = ulog_debug_brief;
    ulog_debug_output = ulog_debugf;

    //If link.console file is existed, the other console options are ignored...
    if( stat(ULOG_LINK_CONSOLE, &s) == 0 )
    {
        return;
    }

    if (stat(ULOG_CONSOLE, &s) == 0) {
        ulog_debug_brief_output = ulog_debug_console;
    }

    if (stat(ULOG_UPNP_CONSOLE, &s) == 0) {
        ulog_debug_output = ulog_debugf_console;
    }
}

void ulog_init ()
{
    struct stat     s;
    pthread_mutex_lock(&g_syslog_mutex);
    /* If link.console file exists then redirect logs to console*/
    if (stat(ULOG_LINK_CONSOLE, &s) == 0)
    {
        openlog(NULL, LOG_NDELAY|LOG_PERROR, LOG_USER);
    }
    else
    {
        openlog(ULOG_IDENT, LOG_NDELAY, LOG_LOCAL7);
    }
    ulog_set_output();
    pthread_mutex_unlock(&g_syslog_mutex);
}
#endif

/*
 * Logging APIs
 */
void ulog (UCOMP comp, USUBCOMP sub, const char *mesg)
{
    pthread_mutex_lock(&g_syslog_mutex);
    t = times(&tmp);
    syslog(LOG_NOTICE, "%s.%s %s [ %u ]", getcomp(comp), getsubcomp(sub), mesg, (unsigned int)t);
    pthread_mutex_unlock(&g_syslog_mutex);
}

void ulogf (UCOMP comp, USUBCOMP sub, const char *fmt, ...)
{
    va_list ap;
    char sfmt[MAX_DATA_LEN+MAX_FILE_LINE];

    pthread_mutex_lock(&g_syslog_mutex);
    t = times(&tmp);
    snprintf(sfmt, sizeof(sfmt), "%s.%s %s [ %u ]", getcomp(comp), getsubcomp(sub), fmt, (unsigned int)t);

    va_start(ap, fmt);
    vsyslog(LOG_NOTICE, sfmt, ap);
    va_end(ap);
    pthread_mutex_unlock(&g_syslog_mutex);
}

void ulog_debug (UCOMP comp, USUBCOMP sub, const char *mesg)
{
    pthread_mutex_lock(&g_syslog_mutex);
    t = times(&tmp);
    syslog(LOG_DEBUG, "%s.%s %s [ %u ]", getcomp(comp), getsubcomp(sub), mesg, (unsigned int)t);
    pthread_mutex_unlock(&g_syslog_mutex);
}

void ulog_debug_packet (UCOMP comp, USUBCOMP sub, const char *mesg, int length)
{
    int pos = 0;
    char szDebugMsg[129] = {0,};

    pthread_mutex_lock(&g_syslog_mutex);
    if( mesg && length )
    {
        t = times(&tmp);
        while( length > 0 )
        {
            memset(szDebugMsg, 0x00, sizeof(szDebugMsg));
            strncpy(szDebugMsg, &mesg[pos], 128);
            syslog(LOG_DEBUG, "%s.%s %s [ %u ]", getcomp(comp), getsubcomp(sub), szDebugMsg, (unsigned int)t);
            pos += 128;
            length -= 128;
        }
    }
    pthread_mutex_unlock(&g_syslog_mutex);
}
#ifdef PRODUCT_WeMo_LEDLight
void ulog_debugf (UCOMP comp, USUBCOMP sub, const char *fmt, ...)
{
    va_list ap;
    char sfmt[MAX_DATA_LEN+MAX_FILE_LINE];

    time_t now;
    struct tm * ts;

    pthread_mutex_lock(&g_syslog_mutex);
    // t = times(&tmp);
    // snprintf(sfmt, sizeof(sfmt), "%s.%s %s [ %u ]", getcomp(comp), getsubcomp(sub), fmt, (unsigned int)t);
    // snprintf(sfmt, sizeof(sfmt), "%s.%s %s", getcomp(comp), getsubcomp(sub), fmt);
    time(&now);
    ts = localtime(&now);

    snprintf(sfmt, sizeof(sfmt), "%d:%d:%d | %s.%s %s\n", ts->tm_hour, ts->tm_min, ts->tm_sec,
            getcomp(comp), getsubcomp(sub), fmt);

    va_start(ap, fmt);
    vsyslog(LOG_DEBUG, sfmt, ap);
    va_end(ap);
    pthread_mutex_unlock(&g_syslog_mutex);
}

void ulog_debugf_console (UCOMP comp, USUBCOMP sub, const char *fmt, ...)
{
    va_list ap;
    char sfmt[MAX_DATA_LEN+MAX_FILE_LINE];

	time_t now;
	struct tm * ts;

    pthread_mutex_lock(&g_syslog_mutex);

	time(&now);
	ts = localtime(&now);

    snprintf(sfmt, sizeof(sfmt), "%d:%d:%d | %s.%s %s\n", ts->tm_hour, ts->tm_min, ts->tm_sec,
            getcomp(comp), getsubcomp(sub), fmt);

    va_start(ap, fmt);
    vprintf(sfmt, ap);
    va_end(ap);

    pthread_mutex_unlock(&g_syslog_mutex);
}

void ulog_debug_brief (const char* tag, const char *fmt, ...)
{
    va_list ap;
    char sfmt[MAX_DATA_LEN+MAX_FILE_LINE];

    pthread_mutex_lock(&g_syslog_mutex);
    snprintf(sfmt, sizeof(sfmt), "%s %s", tag, fmt);
    va_start(ap, fmt);
    vsyslog(LOG_DEBUG, sfmt, ap);
    va_end(ap);
    pthread_mutex_unlock(&g_syslog_mutex);
}

void ulog_debug_console(const char* tag, const char *fmt, ...)
{
    va_list list;
    char    format[256];
	time_t now;
	struct tm * ts;
	time(&now);
	ts = localtime(&now);

    pthread_mutex_lock(&g_syslog_mutex);
    snprintf(format, sizeof(format), "%d:%d:%d | %s %s", ts->tm_hour, ts->tm_min, ts->tm_sec, tag, fmt);
    va_start(list, fmt);
    vprintf(format, list);
    va_end(list);
    pthread_mutex_unlock(&g_syslog_mutex);
}
#endif

void ulog_error (UCOMP comp, USUBCOMP sub, const char *mesg)
{
    pthread_mutex_lock(&g_syslog_mutex);
    t = times(&tmp);
    syslog(LOG_ERR, "%s.%s %s [ %u ]", getcomp(comp), getsubcomp(sub), mesg, (unsigned int)t);
    pthread_mutex_unlock(&g_syslog_mutex);
}

void ulog_errorf (UCOMP comp, USUBCOMP sub, const char *fmt, ...)
{
    va_list ap;
    char sfmt[MAX_DATA_LEN+MAX_FILE_LINE];

    pthread_mutex_lock(&g_syslog_mutex);
    t = times(&tmp);
    snprintf(sfmt, sizeof(sfmt), "%s.%s %s [ %u ]", getcomp(comp), getsubcomp(sub), fmt, (unsigned int)t);

    va_start(ap, fmt);
    vsyslog(LOG_ERR, sfmt, ap);
    va_end(ap);
    pthread_mutex_unlock(&g_syslog_mutex);
}

void
ulog_get_mesgs (UCOMP comp, USUBCOMP sub, char *mesgbuf, unsigned int size)
{
    FILE *fp;
    char match_string[MAX_DATA_LEN+MAX_FILE_LINE], linebuf[MAX_DATA_LEN+MAX_FILE_LINE];

    if (NULL == mesgbuf) {
        return;
    }
    mesgbuf[0] = '\0';

    snprintf(match_string, sizeof(match_string), "%s: %s.%s",
             ULOG_IDENT, getcomp(comp), getsubcomp(sub));

    if (NULL == (fp = fopen(ULOG_FILE, "r"))) {
        return;
    }

    int lsz, remaining_sz = size;

    while (fgets(linebuf, sizeof(linebuf), fp) != NULL) {
        if (strstr(linebuf, match_string)) {
            lsz = strlen(linebuf);
            if (lsz >= remaining_sz) {
                break;  // no more room
            }
            strcat(mesgbuf, linebuf);
            remaining_sz -= lsz;
        }
    }

    fclose(fp);
}

/*
 * Log and Command APIs
 */
void ulog_runcmd (UCOMP comp, USUBCOMP sub, const char *cmd)
{
    pthread_mutex_lock(&g_syslog_mutex);
    syslog(LOG_NOTICE, "%s.%s %s", getcomp(comp), getsubcomp(sub), cmd);
    pthread_mutex_unlock(&g_syslog_mutex);
    if (cmd) {
        system(cmd);
    }
}

int ulog_runcmdf (UCOMP comp, USUBCOMP sub, const char *fmt, ...)
{
    va_list ap, ap_copy;
    char sfmt[MAX_DATA_LEN+MAX_FILE_LINE];

    pthread_mutex_lock(&g_syslog_mutex);
    snprintf(sfmt, sizeof(sfmt), "%s.%s %s", getcomp(comp), getsubcomp(sub), fmt);

    va_copy(ap_copy, ap);

    va_start(ap, fmt);
    vsyslog(LOG_NOTICE, sfmt, ap);
    va_end(ap);
    pthread_mutex_unlock(&g_syslog_mutex);

    va_start(ap_copy, fmt);
    vsnprintf(sfmt, sizeof(sfmt), fmt, ap_copy);
    va_end(ap_copy);
    return system(sfmt);
}

/*
 * Async-signal-safe logging APIs
 *
 * Parameters
 *  fd - output file descriptor (if not sure, use 1. mesg will go to standard output)
 *
 *  equivalent to following formatting,
 *  snprintf(sfmt, sizeof(sfmt), "%s.%s %s", getcomp(comp), getsubcomp(sub), mesg);
 */
void ulog_safe (int fd, UCOMP comp, USUBCOMP sub, const char *mesg)
{
    char sfmt[MAX_DATA_LEN+MAX_FILE_LINE];
    const char *p;
    unsigned int sz = sizeof(sfmt) - 1;

    if (!(fd < 0) && mesg) {
        int i = 0;
        if (ULOG_UNKNOWN != comp) {
            p = getcomp(comp);
            while (p && *p && i < sz) {
                sfmt[i++] = *p++;
            }
            if (UL_UNKNOWN != sub) {
                if (i < sz) {
                    sfmt[i++] = '.';
                }
                if (i < sz) {
                    p = getsubcomp(sub);
                    while (p && *p && i < sz) {
                        sfmt[i++] = *p++;
                    }
                }
            }
            if (i < sz) {
                sfmt[i++] = ' ';
            }
        }
        if (i < sz) {
            p = mesg;
            while (p && *p && i < sz) {
                sfmt[i++] = *p++;
            }
        }
        sfmt[i] = '\0';
        write(fd, sfmt, i);
    }
}


#ifdef SAMPLE_MAIN
main()
{
    ulog_init();
    ulog(ULOG_SYSTEM, UL_SYSCFG, "Hello world");
    closelog();

    char buf[1024];
    buf[0]='\0';
    ulog_get_mesgs(ULOG_SYSTEM, UL_SYSCFG, buf, sizeof(buf));
    printf("ulog_get_mesgs buf - \n%s\n", buf);
}
#endif
