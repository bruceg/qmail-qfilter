// Copyright (C) 2000 Bruce Guenter <bruceg@em.ca>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

/* Choose TMPDIR carefully.  See README for details. */
#ifndef TMPDIR
#define TMPDIR "/tmp"
#endif

#ifndef BUFSIZE
#define BUFSIZE 4096
#endif

#ifndef QMAIL_QUEUE
#define QMAIL_QUEUE "/var/qmail/bin/qmail-queue"
#endif

#define QQ_OOM 51
#define QQ_WRITE_ERROR 53
#define QQ_INTERNAL 81
#define QQ_BAD_ENV 91

/* remap the appropriate FDs and exec qmail-queue */
int run_qmail_queue(int tmpfd, int envpipe[2])
{
  if(close(envpipe[1]) == -1 ||
     dup2(tmpfd, 0) != 0 || close(tmpfd) == -1 ||
     dup2(envpipe[0], 1) != 1 || close(envpipe[0]) == -1 ||
     close(2) == -1)
    return QQ_WRITE_ERROR;
  execl(QMAIL_QUEUE, QMAIL_QUEUE, 0);
  return QQ_INTERNAL;
}

typedef int bool;
const bool false = 0;
const bool true = 0 == 0;

/* a replacement for setenv(3) for systems that don't have one */
bool mysetenv(const char* key, const char* val, size_t vallen)
{
  char* tmp;
  size_t keylen;
  
  keylen = strlen(key);
  tmp = malloc(keylen + 1 + vallen + 1);
  memcpy(tmp, key, keylen);
  tmp[keylen] = '=';
  memcpy(tmp+keylen+1, val, vallen);
  tmp[keylen+1+vallen] = 0;
  return putenv(tmp) != -1;
}

static const char* env = 0;
static size_t env_len = 0;

/* Parse the sender address into user and host portions */
bool parse_sender(const char* sender)
{
  char* at;

  unsetenv("QMAILNAME");
  
  if(!sender[0])
    return putenv("QMAILUSER=") != -1 &&
      putenv("QMAILHOST=") != -1;

  at = strrchr(sender, '@');
  if(!at)
    return false;

  if(!mysetenv("QMAILUSER", sender, at-sender) ||
     !mysetenv("QMAILHOST", at+1, strlen(at+1)))
    return false;

  return true;
}

struct bufchain
{
  size_t len;
  char* buf;
  struct bufchain* next;
};
typedef struct bufchain bufchain;

/* Read the envelope from FD 1. */
bool read_envelope()
{
  bufchain* head = 0;
  bufchain* tail = 0;
  const char* env_sender;
  char* ptr;
  
  for(;;) {
    char buf[BUFSIZE];
    bufchain* newbuf;
    ssize_t rd;

    rd = read(1, buf, BUFSIZE);
    if(rd == -1)
      return false;
    if(rd == 0)
      break;
    newbuf = malloc(sizeof(bufchain));
    newbuf->len = rd;
    newbuf->buf = malloc(rd);
    memcpy(newbuf->buf, buf, rd);
    if(tail)
      tail->next = newbuf;
    else
      head = newbuf;
    tail = newbuf;
    env_len += rd;
  }
  tail->next = 0;

  /* copy the buffer chain into a single buffer */
  ptr = malloc(env_len);
  env = ptr;
  while(head) {
    bufchain* next = head->next;
    memcpy(ptr, head->buf, head->len);
    ptr += head->len;
    free(head->buf);
    free(head);
    head = next;
  }
  
  if(env[0] != 'F')
    return 1;
  env_sender = env+1;
  
  return parse_sender(env_sender);
}

/* Write out the envelope to a pipe */
int write_envelope(int envpipe[2])
{
  if(close(envpipe[0]) == -1)
    return 1;
  /* Funny logic here to catch short writes */
  while(env_len > 0) {
    ssize_t w = write(envpipe[1], env, env_len);
    if(w == -1)
      return 1;
    env += w;
    env_len -= w;
  }
  if(close(envpipe[1]) == -1)
    return 1;
  return 0;
}

/* Create a temporary invisible file opened for read/write */
int mktmpfile()
{
  const char* filename = tempnam(TMPDIR, "fixheaders.");
  int fd = open(filename, O_RDWR | O_CREAT | O_EXCL, 0600);
  if(fd == -1)
    return -1;

  /* The following makes the temporary file disappear immediately on
     program exit. */
  if(unlink(filename) == -1)
    return -1;
  
  return fd;
}

/* Copy the message from FD0 to the first temporary file */
int copy_message()
{
  int tmp = mktmpfile();
  if(tmp == -1)
    return -QQ_WRITE_ERROR;
  
  /* Copy the message into the temporary file */
  for(;;) {
    char buf[BUFSIZE];
    ssize_t rd = read(0, buf, BUFSIZE);
    if(rd == -1)
      return -QQ_WRITE_ERROR;
    if(rd == 0)
      break;
    if(write(tmp, buf, rd) != rd)
      return -QQ_WRITE_ERROR;
  }

  if(lseek(tmp, 0, SEEK_SET) != 0)
    return -QQ_WRITE_ERROR;

  return tmp;
}

/* Wait for qmail-queue to exit, and return an error code */
int wait_qq(pid_t pid, int error)
{
  int status;
  if(waitpid(pid, &status, WUNTRACED) == -1)
    return QQ_INTERNAL;

  if(!WIFEXITED(status))
    error = QQ_INTERNAL;
  else if(WEXITSTATUS(status))
    error = WEXITSTATUS(status);
  
  return error;
}

struct command
{
  char** argv;
  struct command* next;
};
typedef struct command command;

/* Split up the command line into a linked list of seperate commands */
command* parse_args(int argc, char* argv[])
{
  command* tail = 0;
  command* head = 0;
  while(argc > 0) {
    command* cmd;
    int end = 0;
    while(end < argc && strcmp(argv[end], "--"))
      ++end;
    if(end == 0)
      return 0;
    argv[end] = 0;
    cmd = malloc(sizeof(command));
    cmd->argv = argv;
    cmd->next = 0;
    if(tail)
      tail->next = cmd;
    else
      head = cmd;
    tail = cmd;
    ++end;
    argv += end;
    argc -= end;
  }
  return head;
}

/* Run each of the filters in sequence */
int run_filters(command* first, int fdin)
{
  command* c;
  
  for(c = first; c; c = c->next) {
    pid_t pid;
    int status;
    int fdout;

    fdout = mktmpfile();
    if(fdout == -1)
      return -QQ_WRITE_ERROR;
    pid = fork();
    if(pid == -1)
      return -QQ_OOM;
    if(pid == 0) {
      if(close(0) == -1 ||
	 dup2(fdin, 0) != 0 ||
	 close(1) == -1 ||
	 dup2(fdout, 1) != 1 ||
	 close(2) == -1)
	exit(QQ_WRITE_ERROR);
      execvp(c->argv[0], c->argv);
      exit(QQ_INTERNAL);
    }
    if(waitpid(pid, &status, WUNTRACED) == -1)
      return QQ_INTERNAL;
    if(!WIFEXITED(status))
      return -WIFEXITED(status);
    if(WEXITSTATUS(status))
      return -WEXITSTATUS(status);
    close(fdin);
    if(lseek(fdout, 0, SEEK_SET) != 0)
      return -QQ_WRITE_ERROR;
    fdin = fdout;
  }
  return fdin;
}

int main(int argc, char* argv[])
{
  int envpipe[2];
  int tmpfd;
  pid_t queue_pid;
  int error;
  command* filters;
  
  filters = parse_args(argc-1, argv+1);
  if(!filters)
    return QQ_INTERNAL;

  if(pipe(envpipe) == -1)
    return QQ_WRITE_ERROR;

  tmpfd = copy_message();
  if(tmpfd < 0)
    return -tmpfd;

  if(!read_envelope())
    return QQ_BAD_ENV;

  tmpfd = run_filters(filters, tmpfd);
  if(tmpfd < 0)
    return -tmpfd;

  queue_pid = fork();
  switch(queue_pid) {
  case 0:
    return run_qmail_queue(tmpfd, envpipe);
  case -1:
    return QQ_OOM;
  }

  error = write_envelope(envpipe) ? QQ_WRITE_ERROR : 0;
  return wait_qq(queue_pid, error);
}
