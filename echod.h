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

#ifndef _ECHOD_H_
#define _ECHOD_H_

/* We use a numeric port instead of the service name (echo)
   since it will generally bind to port 4 (AppleTalk Echo)
   when it is only defined. */
#ifdef DISCARDD
# define DEFAULT_PORT "9"
#else
# define DEFAULT_PORT "7"
#endif

struct host {
  const char *host;
  const char *port;

  struct host *next;
};

enum srv_flags {
  SRV_DAEMON = 0x1,  /* detach from terminal */
  SRV_INET   = 0x2,  /* listen only on IPv4 */
  SRV_INET6  = 0x4,  /* listen only on IPv6 */
  SRV_UDP    = 0x8,  /* listen on UDP */
  SRV_TCP    = 0x10, /* listen on TCP */
};

/* Hosts list manipulation. */
struct host * add_host(struct host *hosts, const char *host, const char *port);
void free_hosts(struct host *hosts);

/* Bind host and port according to flags.
   Each address binded is forked to a new child, in this case it returns 0.
   The parent returns 1. */
int bind_server(const struct host *hosts, unsigned long flags);

/* Listen on the socket created for this specific child. */
void server(unsigned int max_clients, unsigned int timeout);

#endif /* _ECHOD_H_ */
