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
#include <stdbool.h>
#include <stdarg.h>
#include <syslog.h>
#include <err.h>

#include "log.h"
#include "common.h"

#define MAX_MSG_SIZE 4095

static char msg_buf[MAX_MSG_SIZE];
static int  std_loglevel;

void safecall_act_sysstd(const char *msg)
{
  sysstd_err(EXIT_FAILURE, LOG_ERR, "%s", msg);
}

void sysstd_openlog(const char *ident, int logopt, int facility, int loglevel)
{
  openlog(ident, logopt, facility);
  setlogmask(LOG_UPTO(loglevel));

  std_loglevel = loglevel;
}

/* Send a message to syslog. Also appends the current error message when
   display_error is true. */
static void sysstd_vsyslog(bool display_error, int priority, const char *fmt, va_list args)
{
  if(display_error) {
    vsnprintf(msg_buf, MAX_MSG_SIZE, fmt, args);
    syslog(priority, "%s: %m", msg_buf);
  }
  else
    vsyslog(priority, fmt, args);
}

/* Redefinition of verr/vwarn functions with a uniform signature
   that we use for call indirection in the generic sysstd_output
   function below. */
static void sysstd_uniform_verrx(int eval, const char *fmt, va_list args)
{
  verrx(eval, fmt, args);
}

static void sysstd_uniform_verr(int eval, const char *fmt, va_list args)
{
  verr(eval, fmt, args);
}

static void sysstd_uniform_vwarnx(int eval, const char *fmt, va_list args)
{
  UNUSED(eval);
  vwarnx(fmt, args);
}

static void sysstd_uniform_vwarn(int eval, const char *fmt, va_list args)
{
  UNUSED(eval);
  vwarn(fmt, args);
}

static void sysstd_uniform_vlog(int eval, const char *fmt, va_list args)
{
  UNUSED(eval);
  vfprintf(stderr, fmt, args);
  fputc('\n', stderr);
}

/* Display message on standard output and also log it into syslog.
   This is a generic function that we use in the err/warn wrappers. */
static void sysstd_output(void (*out)(int eval, const char *fmt, va_list args),
                          bool display_error, int eval, int priority, const char *fmt, va_list args)
{
  va_list args_cpy;
  va_copy(args_cpy, args);

  sysstd_vsyslog(display_error, priority, fmt, args);

  if(priority <= std_loglevel)
    out(eval, fmt, args_cpy);

  va_end(args_cpy);
}

void sysstd_warnx(int priority, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  sysstd_output(sysstd_uniform_vwarnx,
                false, 0, priority, fmt, args);

  va_end(args);
}

void sysstd_warn(int priority, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  sysstd_output(sysstd_uniform_vwarn,
                true, 0, priority, fmt, args);

  va_end(args);
}

void sysstd_errx(int eval, int priority, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  sysstd_output(sysstd_uniform_verrx,
                false, eval, priority, fmt, args);

  va_end(args);
}

void sysstd_err(int eval, int priority, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  sysstd_output(sysstd_uniform_verr,
                true, eval, priority, fmt, args);

  va_end(args);
}

void sysstd_log(int priority, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  sysstd_output(sysstd_uniform_vlog,
                false, 0, priority, fmt, args);

  va_end(args);
}
