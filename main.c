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

#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <err.h>

#include "string-utils.h"
#include "version.h"
#include "xatoi.h"
#include "echod.h"
#include "daemon.h"
#include "common.h"
#include "help.h"
#include "log.h"

static void sig_quit(int signum)
{
  UNUSED(signum);

  syslog(LOG_NOTICE, "exiting...");
  exit(EXIT_SUCCESS);
}

static void setup_siglist(int signals[], struct sigaction *act, int size)
{
  int i;

  sigfillset(&act->sa_mask);
  for(i = 0 ; i < size ; i++)
    sigaction(signals[i], act, NULL);
}

static void setup_signals(void)
{
  struct sigaction act_quit = { .sa_handler = sig_quit, .sa_flags = 0 };
  struct sigaction act_ign  = { .sa_handler = SIG_IGN, .sa_flags = 0 };

  int signals_quit[] = {
    SIGINT,
    SIGTERM
  };

  int signals_ign[] = {
    SIGCHLD
  };

  setup_siglist(signals_quit, &act_quit, sizeof_array(signals_quit));
  setup_siglist(signals_ign, &act_ign, sizeof_array(signals_quit));
}

static void print_help(const char *name)
{
  struct opt_help messages[] = {
    { 'h', "help",        "Show this help message" },
    { 'V', "version",     "Show version information" },
#ifdef COMMIT
    { 0,   "commit",      "Display commit information" },
#endif /* COMMIT */
    { 'd', "daemon",      "Detach from controlling terminal" },
    { 'U', "user",        "Relinquish privileges to user" },
    { 'p', "pid",         "Write PID to file" },
    { 'l', "log-level",   "Syslog level from 1 to 8 (default: 7)" },
    { 'c', "max-clients", "Maximum number of simultaneous TCP clients (default: 64)" },
    { 'T', "timeout",     "Timeout for TCP clients (default: 100ms)" },
    { '4', "inet",        "Listen on IPv4 only" },
    { '6', "inet6",       "Listen on IPv6 only" },
    { 'u', "udp",         "Listen on UDP only" },
    { 't', "tcp",         "Listen on TCP only" },
    { 0, NULL, NULL }
  };

  help(name, "[OPTIONS] [host][/port]", messages);
}

int main(int argc, char *argv[])
{
  const char    *prog_name;
  const char    *pid_file     = NULL;
  const char    *user         = NULL;
  const char    *host         = NULL;
  const char    *port         = NULL;
  unsigned long  server_flags = 0;
  unsigned int   max_clients  = 64;
  unsigned int   timeout      = 100;
  unsigned int   loglevel     = LOG_NOTICE;
  int            exit_status  = EXIT_FAILURE;
  int            only_udp  = 0, only_tcp   = 0;
  int            only_inet = 0, only_inet6 = 0;
  int            n;

  enum opt {
    OPT_COMMIT = 0x100
  };

  struct option opts[] = {
    { "help", no_argument, NULL, 'h' },
    { "version", no_argument, NULL, 'V' },
#ifdef COMMIT
    { "commit", no_argument, NULL, OPT_COMMIT },
#endif /* COMMIT */
    { "daemon", no_argument, NULL, 'd' },
    { "user", required_argument, NULL, 'U' },
    { "pid", required_argument, NULL, 'p' },
    { "log-level", required_argument, NULL, 'l' },
    { "max-clients", required_argument, NULL, 'c' },
    { "timeout", required_argument, NULL, 'T' },
    { "inet", no_argument, NULL, '4' },
    { "inet6", no_argument, NULL, '6' },
    { "udp", no_argument, NULL, 'u' },
    { "tcp", no_argument, NULL, 't' },
    { NULL, 0, NULL, 0 }
  };

#ifdef __Linux__
  setproctitle_init(argc, argv, environ); /* libbsd needs that */
#endif
  prog_name = basename(argv[0]);

  while(1) {
    int c = getopt_long(argc, argv, "hVdU:p:l:c:T:46ut", opts, NULL);

    if(c == -1)
      break;

    switch(c) {
    case 'd':
      server_flags |= SRV_DAEMON;
      break;
    case 'U':
      user = optarg;
      break;
    case 'p':
      pid_file = optarg;
      break;
    case 'l':
      loglevel = xatou(optarg, &n);
      if(n)
        errx(EXIT_FAILURE, "invalid log level");
      switch(loglevel) {
      case 1:
        loglevel = LOG_EMERG;
        break;
      case 2:
        loglevel = LOG_ALERT;
        break;
      case 3:
        loglevel = LOG_CRIT;
        break;
      case 4:
        loglevel = LOG_ERR;
        break;
      case 5:
        loglevel = LOG_WARNING;
        break;
      case 6:
        loglevel = LOG_NOTICE;
        break;
      case 7:
        loglevel = LOG_INFO;
        break;
      case 8:
        loglevel = LOG_DEBUG;
        break;
      default:
        errx(EXIT_FAILURE, "invalid log level");
      }
      break;
    case 'c':
      max_clients = xatou(optarg, &n);
      if(n)
        errx(EXIT_FAILURE, "invalid maximum number of clients");
      break;
    case 'T':
      timeout = xatou(optarg, &n);
      if(n)
        errx(EXIT_FAILURE, "invalid timeout value");
      break;
    case '4':
      only_inet  = 1;
      break;
    case '6':
      only_inet6 = 1;
      break;
    case 'u':
      only_udp = 1;
      break;
    case 't':
      only_tcp = 1;
      break;
    case 'V':
      version();
      exit_status = EXIT_SUCCESS;
      goto EXIT;
#ifdef COMMIT
    case OPT_COMMIT:
      commit();
      exit_status = EXIT_SUCCESS;
      goto EXIT;
#endif /* COMMIT */
    case 'h':
      exit_status = EXIT_SUCCESS;
    default:
      print_help(prog_name);
      goto EXIT;
    }
  }

  argc -= optind;
  argv += optind;

  /* parse address and port number */
  if(argc == 1) {
    host = strtok(argv[0], "/");
    port = strtok(NULL, "/");
  } else if(argc > 1) {
    print_help(prog_name);
    goto EXIT;
  }

  /* some users may use '*' for ADDR_ANY */
  if(host && !strcmp(host, "*"))
    host = NULL;

  /* configure default port */
  if(!port)
    port = DEFAULT_PORT;

  /* By default we listen on both TCP/UDP. The TCP/UDP only
     flags will force the selection of either TCP or UDP.
     When both flags are specified, this is the same as
     the default. The same applies for INET/INET6. */
  if(!(only_udp && only_tcp)) {
    if(only_udp)
      server_flags |= SRV_UDP;
    if(only_tcp)
      server_flags |= SRV_TCP;
  }
  if(!(only_inet && only_inet6)) {
    if(only_inet)
      server_flags |= SRV_INET;
    if(only_inet6)
      server_flags |= SRV_INET6;
  }

  /* syslog and start notification */
  sysstd_openlog(prog_name, LOG_PID, LOG_DAEMON | LOG_LOCAL0, loglevel);
  sysstd_log(LOG_NOTICE, PACKAGE_VERSION " starting...");

  /* daemon mode */
  if(server_flags & SRV_DAEMON) {
    if(daemon(0, 0) < 0)
      sysstd_abort("cannot switch to daemon mode");
    sysstd_log(LOG_INFO, "switched to daemon mode");
  }

  /* setup:
      - write pid
      - bind to privilegied port
      - drop privileges
      - setup signals
  */
  if(pid_file)
    write_pid(pid_file);

  /* bind before we drop privileges */
  n = bind_server(host, port, server_flags);

  if(user) {
    drop_privileges(user);
    sysstd_log(LOG_INFO, "privileges dropped to %s", user);
  }

  setup_signals();

  if(!n) /* child */
    server(max_clients, timeout);
  else /* parent */
    while(wait(NULL) > 0);

EXIT:
  exit(exit_status);
}
