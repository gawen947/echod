/* Copyright (c) 2017, David Hauweele <david@hauweele.net>
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
   ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <err.h>

#include "log.h"

static int std_loglevel;

void sysstd_openlog(const char *ident, int logopt, int facility, int loglevel)
{
  openlog(ident, logopt, facility);
  setlogmask(LOG_UPTO(loglevel));

  std_loglevel = loglevel;
}

static void sysstd_w(void (*w)(const char *fmt, va_list args),
                     int priority, const char *fmt, va_list args)
{
  vsyslog(priority, fmt, args);

  if(priority <= std_loglevel)
    w(fmt, args);
}

static void sysstd_e(void (*e)(int eval, const char *fmt, va_list args),
                     int eval, int priority, const char *fmt, va_list args)
{
  vsyslog(priority, fmt, args);

  if(priority <= std_loglevel)
    e(eval, fmt, args);
}

void sysstd_warnx(int priority, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  sysstd_w(vwarnx, priority, fmt, args);
}

void sysstd_warn(int priority, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  /* FIXME: add current error message to syslog with ": %m" */
  sysstd_w(vwarn, priority, fmt, args);
}

void sysstd_errx(int eval, int priority, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  sysstd_e(verrx, eval, priority, fmt, args);
}

void sysstd_err(int eval, int priority, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  /* FIXME: add current error message to syslog with ": %m" */
  sysstd_e(verr, eval, priority, fmt, args);
}

void sysstd_log(int priority, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  if(priority <= std_loglevel) {
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
  }
}
