/* Copyright (C) 2004 Bruce Guenter <bruceg@em.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <sysdeps.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

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

#define QQ_DROP_MSG 99

typedef int bool;
const bool false = 0;
const bool true = 0 == 0;

#define MSGIN 0
#define MSGOUT 1
#define ENVIN 3
#define ENVOUT 4

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
int parse_sender(void)
{
  const char* ptr = env;
  char* at;
  size_t len = strlen(ptr);
  
  if(*ptr != 'F')
    return 0;
  ++ptr;
  
  unsetenv("QMAILNAME");
  
  if(!*ptr)
    return (putenv("QMAILUSER=") != -1 && putenv("QMAILHOST=") != -1) ?  2 : 0;

  at = strrchr(ptr, '@');
  if(!at) {
    len = strlen(ptr);
    if(!mysetenv("QMAILUSER", ptr, len) ||
       putenv("QMAILHOST=") == -1)
      return 0;
  }
  else {
    len = strlen(at);
    if(!mysetenv("QMAILUSER", ptr, at-ptr) ||
       !mysetenv("QMAILHOST", at+1, len-1))
      return 0;
    ptr = at;
  }
  return ptr + len + 1 - env;
}

bool parse_rcpts(int offset)
{
  size_t len = env_len - offset;
  const char* ptr = env + offset;
  char* buf = malloc(len);
  char* tmp = buf;
  bool result;
  while(ptr < env + env_len && *ptr == 'T') {
    size_t rcptlen = strlen(++ptr);
    memcpy(tmp, ptr, rcptlen);
    tmp[rcptlen] = '\n';
    tmp += rcptlen + 1;
    ptr += rcptlen + 1;
  }
  *tmp = 0;
  result = mysetenv("QMAILRCPTS", buf, tmp-buf);
  free(buf);
  return result;
}

bool parse_envelope(void)
{
  int rcpts = parse_sender();
  if(!rcpts)
    return false;
  return parse_rcpts(rcpts);
}

struct bufchain
{
  size_t len;
  char* buf;
  struct bufchain* next;
};
typedef struct bufchain bufchain;

/* Read the envelope from FD 1, and parse the sender address */
bool read_envelope()
{
  bufchain* head = 0;
  bufchain* tail = 0;
  char* ptr;
  
  for(;;) {
    char buf[BUFSIZE];
    bufchain* newbuf;
    ssize_t rd;

    rd = read(ENVIN, buf, BUFSIZE);
    if(rd == -1)
      exit(QQ_BAD_ENV);
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
  if (!tail) exit(QQ_BAD_ENV);
  tail->next = 0;
  if (lseek(ENVIN, 0, SEEK_SET) != 0)
    exit(QQ_WRITE_ERROR);

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
  
  return parse_envelope();
}

/* Create a temporary invisible file opened for read/write */
int mktmpfile()
{
  char filename[sizeof(TMPDIR)+19] = TMPDIR "/fixheaders.XXXXXX";
  
  int fd = mkstemp(filename);
  if(fd == -1)
    exit(QQ_WRITE_ERROR);

  /* The following makes the temporary file disappear immediately on
     program exit. */
  if(unlink(filename) == -1)
    exit(QQ_WRITE_ERROR);
  
  return fd;
}

/* Renumber from one FD to another */
void move_fd(int currfd, int newfd)
{
  if (currfd == newfd)
    return;
  if (dup2(currfd, newfd) != newfd)
    exit(QQ_WRITE_ERROR);
  if (close(currfd) != 0)
    exit(QQ_WRITE_ERROR);
}

/* Copy from one FD to a temporary FD */
void copy_fd(int fdin, int fdout)
{
  int tmp = mktmpfile();
  
  /* Copy the message into the temporary file */
  for(;;) {
    char buf[BUFSIZE];
    ssize_t rd = read(fdin, buf, BUFSIZE);
    if(rd == -1)
      exit(QQ_WRITE_ERROR);
    if(rd == 0)
      break;
    if(write(tmp, buf, rd) != rd)
      exit(QQ_WRITE_ERROR);
  }

  close(fdin);
  if (lseek(tmp, 0, SEEK_SET) != 0)
    exit(QQ_WRITE_ERROR);
  move_fd(tmp, fdout);
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
      exit(QQ_INTERNAL);
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

static void mktmpfd(int fd)
{
  int tmp;
  close(fd);
  tmp = mktmpfile();
  move_fd(tmp, fd);
}

static void move_unless_empty(int src, int dst, const void* reopen)
{
  struct stat st;
  if (fstat(src, &st) != 0)
    exit(QQ_INTERNAL);
  if (st.st_size > 0) {
    move_fd(src, dst);
    if (reopen) {
      mktmpfd(src);
      if (lseek(dst, 0, SEEK_SET) != 0)
	exit(QQ_WRITE_ERROR);
    }
  }
  else
    if (!reopen)
      close(src);
}

/* Run each of the filters in sequence */
void run_filters(const command* first)
{
  const command* c;
  
  mktmpfd(MSGOUT);
  mktmpfd(ENVOUT);

  for(c = first; c; c = c->next) {
    pid_t pid;
    int status;

    pid = fork();
    if(pid == -1)
      exit(QQ_OOM);
    if(pid == 0) {
      execvp(c->argv[0], c->argv);
      exit(QQ_INTERNAL);
    }
    if(waitpid(pid, &status, WUNTRACED) == -1)
      exit(QQ_INTERNAL);
    if(!WIFEXITED(status))
      exit(QQ_INTERNAL);
    if(WEXITSTATUS(status))
      exit((WEXITSTATUS(status) == QQ_DROP_MSG) ? 0 : WEXITSTATUS(status));
    move_unless_empty(MSGOUT, MSGIN, c->next);
    move_unless_empty(ENVOUT, ENVIN, c->next);
  }
}

int main(int argc, char* argv[])
{
  const command* filters;
  
  filters = parse_args(argc-1, argv+1);

  copy_fd(0, 0);
  copy_fd(1, ENVIN);
  if(!read_envelope())
    return QQ_BAD_ENV;

  run_filters(filters);

  move_fd(ENVIN, 1);
  execl(QMAIL_QUEUE, QMAIL_QUEUE, 0);
  return QQ_INTERNAL;
}
