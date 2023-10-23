#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

// #include "threads/synch.h"

void syscall_init (void);
struct file *process_get_file (int fd);
int filesize (int fd);
#endif /* userprog/syscall.h */
