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

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <err.h>

#include "safe-call.h"

#if !defined(daemon)
/* for targets that do not implement daemon() */
int daemon(int nochdir, int noclose)
{
  int fd;
  switch(fork()) {
  case -1: /* error */
    return -1;
  case 0:  /* child */
    break;
  default: /* parent */
    exit(0);
  }

  if(setsid() == -1)
    return -1;

  if(!nochdir)
    chdir("/");

  if(!noclose && (fd = open("/dev/null", O_RDWR, 0)) != -1) {
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if(fd > 2)
      close(fd);
  }
  return 0;
}
#endif /* daemon() */

void write_pid(const char *pid_file)
{
  char buf[32];
  int fd = xopen(pid_file, O_WRONLY | O_TRUNC | O_CREAT, 0660);

  sprintf(buf, "%d\n", getpid());

  write(fd, buf, strlen(buf));

  close(fd);
}

void drop_privileges(const char *user, const char *group)
{
  struct passwd *user_pwd    = getpwnam(user);
  struct group  *group_pwd   = getgrnam(group);

  if(!user_pwd)
    errx(EXIT_FAILURE, "invalid user");
  if(!group_pwd)
    errx(EXIT_FAILURE, "invalid group");

  /* Note that we expect /var/empty to exist.
     It's not always the case on Linux. */
  if(chroot("/var/empty") || chdir("/"))
    errx(EXIT_FAILURE, "cannot chroot");

  if(setgid(group_pwd->gr_gid) ||
     setuid(user_pwd->pw_uid))
    err(EXIT_FAILURE, "cannot drop privileges");
}
