#include <unistd.h>
#include <string.h>
#include <signal.h>
#include "syscall.h"
#include "libc.h"
#include "pthread_impl.h"

static void dummy(int x)
{
}

weak_alias(dummy, __fork_handler);

void prefork_lock();
void postfork_unlock_parent();
void postfork_unlock_child();

pid_t fork(void)
{
	pid_t ret;
	sigset_t set;
	__fork_handler(-1);
	__block_all_sigs(&set);
  // Important that we can't become MT while acquiring locks
  // (last paragraph here: http://www.openwall.com/lists/musl/2017/06/18/9 )
	prefork_lock();
#ifdef SYS_fork
	ret = __syscall(SYS_fork);
#else
	ret = __syscall(SYS_clone, SIGCHLD, 0);
#endif
	if (!ret) {
		pthread_t self = __pthread_self();
		self->tid = __syscall(SYS_gettid);
		self->robust_list.off = 0;
		self->robust_list.pending = 0;
		libc.threads_minus_1 = 0;
		postfork_unlock_child();
	} else {
		postfork_unlock_parent();
	}
	__restore_sigs(&set);
	__fork_handler(!ret);
	return __syscall_ret(ret);
}
