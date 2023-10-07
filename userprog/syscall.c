#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h"
#include "userprog/process.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);
int write(int fd, const void *buffer, unsigned int size);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081			/* Segment selector msr */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f UNUSED)
{
	// TODO: Your implementation goes here.
	struct thread *curr_t = thread_current();
	switch (f->R.rax)
	{
	case SYS_HALT: // power off! 핀토스를 종료, 강종 느낌
	{
		power_off();
		break;
	}
	case SYS_EXIT:
		// 유저: exit(EXIT_NUM, status) 호출
		f->R.rax = f->R.rdi;
		// 프로세스 디스크립터에 exit_status값 저장
		curr_t->exit_status = f->R.rdi;
		printf("%s: exit(%d)\n", curr_t->name, f->R.rdi);
		thread_exit();
		break;
	case SYS_FORK:
		// struct thread* curr_t = thread_current();
		f->R.rax = process_fork(f->R.rdi, f);
		break;
	case SYS_EXEC:
		// exec();
		break;
	case SYS_WAIT:
		while (1)
		{
			;
		}
		break;
	case SYS_CREATE:
		// create();
		break;
	case SYS_REMOVE:
		// remove();
		break;
	case SYS_OPEN:
		// open();
		break;
	case SYS_FILESIZE:
		// filesize();
		break;
	case SYS_READ:
		// read();
		break;
	case SYS_WRITE:
		write(f->R.rdi, f->R.rsi, f->R.rdx);
		// printf("%s", f->R.rsi);
		// write();
		break;
	case SYS_SEEK:
		// seek();
		break;
	case SYS_TELL:

		break;
	case SYS_CLOSE:

		break;
	default:
		break;
	}
}

int write(int fd, const void *buffer, unsigned int size)
{
	if (fd == 1)
	{
		putbuf(buffer, size);
		return size;
	}
	return -1;
}
/*TODO : 프로세스 디스크립터를 삭제하지 않도록 수정*/
//prev가 뭐야
//initial_thread가 뭐야
void thread_schedule_tail(struct thread *prev)
{
	struct thread *cur = thread_current();
	ASSERT(intr_get_level() == INTR_OFF);
	/* Mark us as running. */
	cur->status = THREAD_RUNNING;
	if (prev != NULL && prev->status == THREAD_DYING &&
			prev != initial_thread)
	{
		ASSERT(prev != cur);
		palloc_free_page(prev); /* 프로세스 디스크립터 삭제 */
	}
}