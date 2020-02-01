#pragma once
#include "sys/types.h"

int sys_kill(pid_t pid, int signum);
void sys_exit(int status);
void sys_sigentry(uintptr_t entry);
void sys_sigreturn(void);
int sys_execve(const char *path, const char **argp, const char **envp);
pid_t sys_getpid(void);

uid_t sys_getuid(void);
gid_t sys_getgid(void);
int sys_setuid(uid_t uid);
int sys_setgid(gid_t gid);
