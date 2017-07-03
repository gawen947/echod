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

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>
#include <syslog.h>

#ifdef __FreeBSD__
# include <sys/capsicum.h>
#endif

#include "echod.h"
#include "safe-call.h"
#include "common.h"
#include "version.h"
#include "daemon.h"
#include "log.h"

#define BUFFER_SIZE 4096
#define BACKLOG     4

/* Clear the buffer after each request to avoid
   any potential heartbleed vulnerability. */
#ifdef DO_CLEAR_BUFFER
# define clear_buffer() memset(buffer, 0, sizeof(buffer))
#else
# define clear_buffer() (void)0
#endif /* DO_CLEAR_BUFFER */

static int sd; /* socket descriptor */
static int af; /* address family */
static int st; /* socket type */

static unsigned int clients; /* number of clients connected */

static unsigned char buffer[BUFFER_SIZE];

int bind_server(const char *host, const char *port, unsigned long flags)
{
  struct addrinfo *resolution, *r;
  struct addrinfo hints;
  pid_t pid;
  int n, ret, optval = 1;

  memset(&hints, 0, sizeof(hints));
  hints = (struct addrinfo){ .ai_family   = AF_UNSPEC,
                             .ai_flags    = AI_PASSIVE };

  /* select address family and protocol */
  if(flags & SRV_INET)
    hints.ai_family = AF_INET;
  if(flags & SRV_INET6)
    hints.ai_family = AF_INET6;
  if(flags & SRV_UDP) {
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
  }
  if(flags & SRV_TCP) {
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
  }

  n = getaddrinfo(host, port, &hints, &resolution);
  if(n)
    sysstd_abortx("cannot resolve requested address: %s", gai_strerror(n));

  for(r = resolution ; r ; r = r->ai_next) {
    /* filter unwanted addresses */
    switch(r->ai_family) {
    case AF_INET:
    case AF_INET6:
      break;
    default:
      continue;
    }
    switch(r->ai_socktype) {
    case SOCK_DGRAM:
    case SOCK_STREAM:
      break;
    default:
      continue;
    }
    switch(r->ai_protocol) {
    case IPPROTO_UDP:
    case IPPROTO_TCP:
      break;
    default:
      continue;
    }

    /* From here all addresses match the filters applied on command line.
       We bind to the specified address and fork a new child for listening. */
    sd = xsocket(r->ai_family, r->ai_socktype, r->ai_protocol);
    af = r->ai_family;
    st = r->ai_socktype;

    n = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    if(n < 0)
      sysstd_abort("cannot set socket options");

    xbind(sd, r->ai_addr, r->ai_addrlen);

    pid = fork();
    if(!pid) { /* child */
      ret = 0;
      goto EXIT;
    }
    else if(pid < 0) /* error */
      sysstd_abort("fork error");
    /* parent (continue) */
  }

  /* parent */
  ret = 1;
EXIT:
  freeaddrinfo(resolution);
  return ret;
}

static void server_udp(void)
{
#ifdef __FreeBSD__
  cap_rights_t rights;
  cap_rights_init(&rights, CAP_RECV, CAP_SEND, CAP_CONNECT);
  xcap_rights_limit(sd, &rights);
#endif

  /* Sandboxing does not work for UDP yet.
     The sendto() call is rejected in capsicum capability mode.
     But we cannot connect beforehand because we use a single
     thread per UDP sockets. */
#if 0
  sandbox();
#endif

  while(1) {
    struct sockaddr_storage from;
    socklen_t from_len = sizeof(from);
    ssize_t n;

  INTR: /* syscall may be interrupted */
    n = recvfrom(sd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&from, &from_len);
    if(n < 0) {
      if(errno == EINTR)
        goto INTR;
      sysstd_abort("receive error");
    }

    /* answer */
    n = sendto(sd, buffer, n, 0, (struct sockaddr *)&from, from_len);
    if(n < 0)
      sysstd_abort("send error");

    clear_buffer();
  }
}

static void sig_chld(int signum)
{
  UNUSED(signum);
  clients--;
  wait(NULL);
}

static void rename_listen_child(void)
{
  const char *st_s;
  const char *af_s;

  switch(af) {
  case AF_INET:
    af_s = "IPv4";
    break;
  case AF_INET6:
    af_s = "IPv6";
    break;
  default:
    assert(0);
  }

  switch(st) {
  case SOCK_DGRAM:
    st_s = "UDP";
    break;
  case SOCK_STREAM:
    st_s = "TCP";
    break;
  default:
    assert(0);
  }

  setproctitle("listen on %s %s", st_s, af_s);
}

static void rename_client_child(const struct sockaddr *addr)
{
  char client[INET6_ADDRSTRLEN];
  const void *addr_in;

  switch(af) {
  case AF_INET:
    addr_in = &((struct sockaddr_in *)addr)->sin_addr;
    break;
  case AF_INET6:
    addr_in = &((struct sockaddr_in6 *)addr)->sin6_addr;
    break;
  default:
    assert(0);
  }

  inet_ntop(af, addr_in, client, sizeof(client));
  setproctitle("connection from %s", client);
}

static void server_tcp(unsigned int max_clients, unsigned int timeout)
{
  struct timeval timeout_tv;

#ifdef __FreeBSD__
  cap_rights_t rights;
  cap_rights_init(&rights, CAP_LISTEN, CAP_ACCEPT);
  xcap_rights_limit(sd, &rights);
#endif

  xlisten(sd, BACKLOG);

  signal(SIGCHLD, sig_chld);

  timeout   *= 1000; /* ms to us */
  timeout_tv = (struct timeval){ .tv_sec  = timeout / 1000000,
                                 .tv_usec = timeout % 1000000 };

  while(1) {
    struct sockaddr_storage from;
    socklen_t from_len = sizeof(from);
    pid_t pid;
    ssize_t n;
    int fd;

  ACPT_INTR: /* syscall may be interrupted */
    fd = accept(sd, (struct sockaddr *)&from, &from_len);
    if(fd < 0) {
      if(errno == EINTR)
        goto ACPT_INTR;
      sysstd_abort("accept error");
    }

#ifdef __FreeBSD__
    cap_rights_init(&rights, CAP_RECV, CAP_SEND, CAP_SETSOCKOPT);
    xcap_rights_limit(fd, &rights);
#endif

    if(max_clients && clients >= max_clients) {
      close(fd);
      sysstd_log(LOG_DEBUG, "connection dropped: maximum number of clients reached (%d)", clients);
      continue;
    }
    else
      clients++;

    /* fork again to handle connection */
    pid = fork();
    if(!pid) { /* child */
      sandbox();
      rename_client_child((struct sockaddr *)&from);
      close(sd); /* close unused FD */

      /* configure timeout limit */
      if(timeout)
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout_tv, sizeof(struct timeval));

    RECV_INTR: /* syscall may be interrupted */
      n = recv(fd, buffer, BUFFER_SIZE, 0);
      if(n < 0) {
        switch(errno) {
        case EINTR:
          /* syscall interrupted */
          goto RECV_INTR;
        case EAGAIN:
          if(timeout) { /* probably a timeout */
            sysstd_log(LOG_DEBUG, "connection timeout");
            exit(1);
          }
          break;
        default:
          sysstd_abort("receive error");
        }
      }

      /* answer */
      n = send(fd, buffer, n, 0);
      if(n < 0)
        sysstd_abort("send error");

      /* We answered the client.
         Now we can exit. */
      exit(0);
    }
    else if(pid < 0) /* error */
      sysstd_abort("fork error");

    /* parent (continue) */
    close(fd);
  }
}

void server(unsigned int max_clients, unsigned int timeout)
{
  /* reflect address family and socket type in child name */
  rename_listen_child();

  switch(st) {
  case SOCK_DGRAM:
    server_udp();
    break;
  case SOCK_STREAM:
    server_tcp(max_clients, timeout);
    break;
  default:
    assert(0); /* either UDP or TCP */
    break;
  }
}
