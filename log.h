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

#ifndef _LOG_H_
#define _LOG_H_

#include <syslog.h>
#include <stdlib.h>

#define sysstd_abort(...)  sysstd_err (EXIT_FAILURE, LOG_ERR, __VA_ARGS__)
#define sysstd_abortx(...) sysstd_errx(EXIT_FAILURE, LOG_ERR, __VA_ARGS__)

/* Open the syslog/stdio log interface.
   Syslog must be opened beforehand.
   Message are not shown to stdout if
   their priority is higher than loglevel. */
void sysstd_openlog(const char *ident, int logopt, int facility, int loglevel);

void sysstd_warnx(int priority, const char *fmt, ...);
void sysstd_warn(int priority, const char *fmt, ...);
void sysstd_errx(int eval, int priority, const char *fmt, ...);
void sysstd_err(int eval, int priority, const char *fmt, ...);
void sysstd_log(int priority, const char *fmt, ...);

/* Safe call error action when sysstd has been enabled. */
void safecall_act_sysstd(const char *msg);

#endif /* _LOG_H_ */
