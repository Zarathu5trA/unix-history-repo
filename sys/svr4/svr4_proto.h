/*
 * System call prototypes.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * $FreeBSD$
 * created from;	@(#)syscalls.master	8.1 (Berkeley) 7/19/93
 */

#ifndef _SVR4_SYSPROTO_H_
#define	_SVR4_SYSPROTO_H_

#include <sys/signal.h>

#include <sys/acl.h>

struct proc;

#define	PAD_(t)	(sizeof(register_t) <= sizeof(t) ? \
		0 : sizeof(register_t) - sizeof(t))

struct	svr4_sys_open_args {
	char *	path;	char path_[PAD_(char *)];
	int	flags;	char flags_[PAD_(int)];
	int	mode;	char mode_[PAD_(int)];
};
struct	svr4_sys_wait_args {
	int *	status;	char status_[PAD_(int *)];
};
struct	svr4_sys_creat_args {
	char *	path;	char path_[PAD_(char *)];
	int	mode;	char mode_[PAD_(int)];
};
struct	svr4_sys_execv_args {
	char *	path;	char path_[PAD_(char *)];
	char **	argp;	char argp_[PAD_(char **)];
};
struct	svr4_sys_time_args {
	time_t *	t;	char t_[PAD_(time_t *)];
};
struct	svr4_sys_mknod_args {
	char *	path;	char path_[PAD_(char *)];
	int	mode;	char mode_[PAD_(int)];
	int	dev;	char dev_[PAD_(int)];
};
struct	svr4_sys_break_args {
	caddr_t	nsize;	char nsize_[PAD_(caddr_t)];
};
struct	svr4_sys_stat_args {
	char *	path;	char path_[PAD_(char *)];
	struct svr4_stat *	ub;	char ub_[PAD_(struct svr4_stat *)];
};
struct	svr4_sys_alarm_args {
	unsigned	sec;	char sec_[PAD_(unsigned)];
};
struct	svr4_sys_fstat_args {
	int	fd;	char fd_[PAD_(int)];
	struct svr4_stat *	sb;	char sb_[PAD_(struct svr4_stat *)];
};
struct	svr4_sys_pause_args {
	register_t dummy;
};
struct	svr4_sys_utime_args {
	char *	path;	char path_[PAD_(char *)];
	struct svr4_utimbuf *	ubuf;	char ubuf_[PAD_(struct svr4_utimbuf *)];
};
struct	svr4_sys_access_args {
	char *	path;	char path_[PAD_(char *)];
	int	flags;	char flags_[PAD_(int)];
};
struct	svr4_sys_nice_args {
	int	prio;	char prio_[PAD_(int)];
};
struct	svr4_sys_kill_args {
	int	pid;	char pid_[PAD_(int)];
	int	signum;	char signum_[PAD_(int)];
};
struct	svr4_sys_pgrpsys_args {
	int	cmd;	char cmd_[PAD_(int)];
	int	pid;	char pid_[PAD_(int)];
	int	pgid;	char pgid_[PAD_(int)];
};
struct	svr4_sys_times_args {
	struct tms *	tp;	char tp_[PAD_(struct tms *)];
};
struct	svr4_sys_signal_args {
	int	signum;	char signum_[PAD_(int)];
	svr4_sig_t	handler;	char handler_[PAD_(svr4_sig_t)];
};
#if defined(NOTYET)
struct	svr4_sys_msgsys_args {
	int	what;	char what_[PAD_(int)];
	int	a2;	char a2_[PAD_(int)];
	int	a3;	char a3_[PAD_(int)];
	int	a4;	char a4_[PAD_(int)];
	int	a5;	char a5_[PAD_(int)];
};
#else
#endif
struct	svr4_sys_sysarch_args {
	int	op;	char op_[PAD_(int)];
	void *	a1;	char a1_[PAD_(void *)];
};
struct	svr4_sys_ioctl_args {
	int	fd;	char fd_[PAD_(int)];
	u_long	com;	char com_[PAD_(u_long)];
	caddr_t	data;	char data_[PAD_(caddr_t)];
};
struct	svr4_sys_utssys_args {
	void *	a1;	char a1_[PAD_(void *)];
	void *	a2;	char a2_[PAD_(void *)];
	int	sel;	char sel_[PAD_(int)];
	void *	a3;	char a3_[PAD_(void *)];
};
struct	svr4_sys_execve_args {
	char *	path;	char path_[PAD_(char *)];
	char **	argp;	char argp_[PAD_(char **)];
	char **	envp;	char envp_[PAD_(char **)];
};
struct	svr4_sys_fcntl_args {
	int	fd;	char fd_[PAD_(int)];
	int	cmd;	char cmd_[PAD_(int)];
	char *	arg;	char arg_[PAD_(char *)];
};
struct	svr4_sys_ulimit_args {
	int	cmd;	char cmd_[PAD_(int)];
	long	newlimit;	char newlimit_[PAD_(long)];
};
struct	svr4_sys_getdents_args {
	int	fd;	char fd_[PAD_(int)];
	char *	buf;	char buf_[PAD_(char *)];
	int	nbytes;	char nbytes_[PAD_(int)];
};
struct	svr4_sys_getmsg_args {
	int	fd;	char fd_[PAD_(int)];
	struct svr4_strbuf *	ctl;	char ctl_[PAD_(struct svr4_strbuf *)];
	struct svr4_strbuf *	dat;	char dat_[PAD_(struct svr4_strbuf *)];
	int *	flags;	char flags_[PAD_(int *)];
};
struct	svr4_sys_putmsg_args {
	int	fd;	char fd_[PAD_(int)];
	struct svr4_strbuf *	ctl;	char ctl_[PAD_(struct svr4_strbuf *)];
	struct svr4_strbuf *	dat;	char dat_[PAD_(struct svr4_strbuf *)];
	int	flags;	char flags_[PAD_(int)];
};
struct	svr4_sys_poll_args {
	struct pollfd *	fds;	char fds_[PAD_(struct pollfd *)];
	unsigned int	nfds;	char nfds_[PAD_(unsigned int)];
	int	timeout;	char timeout_[PAD_(int)];
};
struct	svr4_sys_lstat_args {
	char *	path;	char path_[PAD_(char *)];
	struct svr4_stat *	ub;	char ub_[PAD_(struct svr4_stat *)];
};
struct	svr4_sys_sigprocmask_args {
	int	how;	char how_[PAD_(int)];
	svr4_sigset_t *	set;	char set_[PAD_(svr4_sigset_t *)];
	svr4_sigset_t *	oset;	char oset_[PAD_(svr4_sigset_t *)];
};
struct	svr4_sys_sigsuspend_args {
	svr4_sigset_t *	ss;	char ss_[PAD_(svr4_sigset_t *)];
};
struct	svr4_sys_sigaltstack_args {
	struct svr4_sigaltstack *	nss;	char nss_[PAD_(struct svr4_sigaltstack *)];
	struct svr4_sigaltstack *	oss;	char oss_[PAD_(struct svr4_sigaltstack *)];
};
struct	svr4_sys_sigaction_args {
	int	signum;	char signum_[PAD_(int)];
	struct svr4_sigaction *	nsa;	char nsa_[PAD_(struct svr4_sigaction *)];
	struct svr4_sigaction *	osa;	char osa_[PAD_(struct svr4_sigaction *)];
};
struct	svr4_sys_sigpending_args {
	int	what;	char what_[PAD_(int)];
	svr4_sigset_t *	mask;	char mask_[PAD_(svr4_sigset_t *)];
};
struct	svr4_sys_context_args {
	int	func;	char func_[PAD_(int)];
	struct svr4_ucontext *	uc;	char uc_[PAD_(struct svr4_ucontext *)];
};
struct	svr4_sys_statvfs_args {
	char *	path;	char path_[PAD_(char *)];
	struct svr4_statvfs *	fs;	char fs_[PAD_(struct svr4_statvfs *)];
};
struct	svr4_sys_fstatvfs_args {
	int	fd;	char fd_[PAD_(int)];
	struct svr4_statvfs *	fs;	char fs_[PAD_(struct svr4_statvfs *)];
};
struct	svr4_sys_waitsys_args {
	int	grp;	char grp_[PAD_(int)];
	int	id;	char id_[PAD_(int)];
	union svr4_siginfo *	info;	char info_[PAD_(union svr4_siginfo *)];
	int	options;	char options_[PAD_(int)];
};
struct	svr4_sys_hrtsys_args {
	int	cmd;	char cmd_[PAD_(int)];
	int	fun;	char fun_[PAD_(int)];
	int	sub;	char sub_[PAD_(int)];
	void *	rv1;	char rv1_[PAD_(void *)];
	void *	rv2;	char rv2_[PAD_(void *)];
};
struct	svr4_sys_pathconf_args {
	char *	path;	char path_[PAD_(char *)];
	int	name;	char name_[PAD_(int)];
};
struct	svr4_sys_mmap_args {
	caddr_t	addr;	char addr_[PAD_(caddr_t)];
	svr4_size_t	len;	char len_[PAD_(svr4_size_t)];
	int	prot;	char prot_[PAD_(int)];
	int	flags;	char flags_[PAD_(int)];
	int	fd;	char fd_[PAD_(int)];
	svr4_off_t	pos;	char pos_[PAD_(svr4_off_t)];
};
struct	svr4_sys_fpathconf_args {
	int	fd;	char fd_[PAD_(int)];
	int	name;	char name_[PAD_(int)];
};
struct	svr4_sys_xstat_args {
	int	two;	char two_[PAD_(int)];
	char *	path;	char path_[PAD_(char *)];
	struct svr4_xstat *	ub;	char ub_[PAD_(struct svr4_xstat *)];
};
struct	svr4_sys_lxstat_args {
	int	two;	char two_[PAD_(int)];
	char *	path;	char path_[PAD_(char *)];
	struct svr4_xstat *	ub;	char ub_[PAD_(struct svr4_xstat *)];
};
struct	svr4_sys_fxstat_args {
	int	two;	char two_[PAD_(int)];
	int	fd;	char fd_[PAD_(int)];
	struct svr4_xstat *	sb;	char sb_[PAD_(struct svr4_xstat *)];
};
struct	svr4_sys_xmknod_args {
	int	two;	char two_[PAD_(int)];
	char *	path;	char path_[PAD_(char *)];
	svr4_mode_t	mode;	char mode_[PAD_(svr4_mode_t)];
	svr4_dev_t	dev;	char dev_[PAD_(svr4_dev_t)];
};
struct	svr4_sys_setrlimit_args {
	int	which;	char which_[PAD_(int)];
	const struct svr4_rlimit *	rlp;	char rlp_[PAD_(const struct svr4_rlimit *)];
};
struct	svr4_sys_getrlimit_args {
	int	which;	char which_[PAD_(int)];
	struct svr4_rlimit *	rlp;	char rlp_[PAD_(struct svr4_rlimit *)];
};
struct	svr4_sys_memcntl_args {
	void *	addr;	char addr_[PAD_(void *)];
	svr4_size_t	len;	char len_[PAD_(svr4_size_t)];
	int	cmd;	char cmd_[PAD_(int)];
	void *	arg;	char arg_[PAD_(void *)];
	int	attr;	char attr_[PAD_(int)];
	int	mask;	char mask_[PAD_(int)];
};
struct	svr4_sys_uname_args {
	struct svr4_utsname *	name;	char name_[PAD_(struct svr4_utsname *)];
	int	dummy;	char dummy_[PAD_(int)];
};
struct	svr4_sys_sysconfig_args {
	int	name;	char name_[PAD_(int)];
};
struct	svr4_sys_systeminfo_args {
	int	what;	char what_[PAD_(int)];
	char *	buf;	char buf_[PAD_(char *)];
	long	len;	char len_[PAD_(long)];
};
struct	svr4_sys_fchroot_args {
	int	fd;	char fd_[PAD_(int)];
};
struct	svr4_sys_utimes_args {
	char *	path;	char path_[PAD_(char *)];
	struct timeval *	tptr;	char tptr_[PAD_(struct timeval *)];
};
struct	svr4_sys_vhangup_args {
	register_t dummy;
};
struct	svr4_sys_gettimeofday_args {
	struct timeval *	tp;	char tp_[PAD_(struct timeval *)];
};
struct	svr4_sys_llseek_args {
	int	fd;	char fd_[PAD_(int)];
	long	offset1;	char offset1_[PAD_(long)];
	long	offset2;	char offset2_[PAD_(long)];
	int	whence;	char whence_[PAD_(int)];
};
struct	svr4_sys_acl_args {
	char *	path;	char path_[PAD_(char *)];
	int	cmd;	char cmd_[PAD_(int)];
	int	num;	char num_[PAD_(int)];
	struct svr4_aclent *	buf;	char buf_[PAD_(struct svr4_aclent *)];
};
struct	svr4_sys_auditsys_args {
	int	code;	char code_[PAD_(int)];
	int	a1;	char a1_[PAD_(int)];
	int	a2;	char a2_[PAD_(int)];
	int	a3;	char a3_[PAD_(int)];
	int	a4;	char a4_[PAD_(int)];
	int	a5;	char a5_[PAD_(int)];
};
struct	svr4_sys_facl_args {
	int	fd;	char fd_[PAD_(int)];
	int	cmd;	char cmd_[PAD_(int)];
	int	num;	char num_[PAD_(int)];
	struct svr4_aclent *	buf;	char buf_[PAD_(struct svr4_aclent *)];
};
struct	svr4_sys_resolvepath_args {
	const char *	path;	char path_[PAD_(const char *)];
	char *	buf;	char buf_[PAD_(char *)];
	size_t	bufsiz;	char bufsiz_[PAD_(size_t)];
};
struct	svr4_sys_getdents64_args {
	int	fd;	char fd_[PAD_(int)];
	struct svr4_dirent64 *	dp;	char dp_[PAD_(struct svr4_dirent64 *)];
	int	nbytes;	char nbytes_[PAD_(int)];
};
struct	svr4_sys_mmap64_args {
	void *	addr;	char addr_[PAD_(void *)];
	svr4_size_t	len;	char len_[PAD_(svr4_size_t)];
	int	prot;	char prot_[PAD_(int)];
	int	flags;	char flags_[PAD_(int)];
	int	fd;	char fd_[PAD_(int)];
	svr4_off64_t	pos;	char pos_[PAD_(svr4_off64_t)];
};
struct	svr4_sys_stat64_args {
	char *	path;	char path_[PAD_(char *)];
	struct svr4_stat64 *	sb;	char sb_[PAD_(struct svr4_stat64 *)];
};
struct	svr4_sys_lstat64_args {
	char *	path;	char path_[PAD_(char *)];
	struct svr4_stat64 *	sb;	char sb_[PAD_(struct svr4_stat64 *)];
};
struct	svr4_sys_fstat64_args {
	int	fd;	char fd_[PAD_(int)];
	struct svr4_stat64 *	sb;	char sb_[PAD_(struct svr4_stat64 *)];
};
struct	svr4_sys_statvfs64_args {
	char *	path;	char path_[PAD_(char *)];
	struct svr4_statvfs64 *	fs;	char fs_[PAD_(struct svr4_statvfs64 *)];
};
struct	svr4_sys_fstatvfs64_args {
	int	fd;	char fd_[PAD_(int)];
	struct svr4_statvfs64 *	fs;	char fs_[PAD_(struct svr4_statvfs64 *)];
};
struct	svr4_sys_setrlimit64_args {
	int	which;	char which_[PAD_(int)];
	const struct svr4_rlimit64 *	rlp;	char rlp_[PAD_(const struct svr4_rlimit64 *)];
};
struct	svr4_sys_getrlimit64_args {
	int	which;	char which_[PAD_(int)];
	struct svr4_rlimit64 *	rlp;	char rlp_[PAD_(struct svr4_rlimit64 *)];
};
struct	svr4_sys_creat64_args {
	char *	path;	char path_[PAD_(char *)];
	int	mode;	char mode_[PAD_(int)];
};
struct	svr4_sys_open64_args {
	char *	path;	char path_[PAD_(char *)];
	int	flags;	char flags_[PAD_(int)];
	int	mode;	char mode_[PAD_(int)];
};
struct	svr4_sys_socket_args {
	int	domain;	char domain_[PAD_(int)];
	int	type;	char type_[PAD_(int)];
	int	protocol;	char protocol_[PAD_(int)];
};
struct	svr4_sys_recv_args {
	int	s;	char s_[PAD_(int)];
	caddr_t	buf;	char buf_[PAD_(caddr_t)];
	int	len;	char len_[PAD_(int)];
	int	flags;	char flags_[PAD_(int)];
};
struct	svr4_sys_send_args {
	int	s;	char s_[PAD_(int)];
	caddr_t	buf;	char buf_[PAD_(caddr_t)];
	int	len;	char len_[PAD_(int)];
	int	flags;	char flags_[PAD_(int)];
};
struct	svr4_sys_sendto_args {
	int	s;	char s_[PAD_(int)];
	void *	buf;	char buf_[PAD_(void *)];
	size_t	len;	char len_[PAD_(size_t)];
	int	flags;	char flags_[PAD_(int)];
	struct sockaddr *	to;	char to_[PAD_(struct sockaddr *)];
	int	tolen;	char tolen_[PAD_(int)];
};
int	svr4_sys_open __P((struct proc *, struct svr4_sys_open_args *));
int	svr4_sys_wait __P((struct proc *, struct svr4_sys_wait_args *));
int	svr4_sys_creat __P((struct proc *, struct svr4_sys_creat_args *));
int	svr4_sys_execv __P((struct proc *, struct svr4_sys_execv_args *));
int	svr4_sys_time __P((struct proc *, struct svr4_sys_time_args *));
int	svr4_sys_mknod __P((struct proc *, struct svr4_sys_mknod_args *));
int	svr4_sys_break __P((struct proc *, struct svr4_sys_break_args *));
int	svr4_sys_stat __P((struct proc *, struct svr4_sys_stat_args *));
int	svr4_sys_alarm __P((struct proc *, struct svr4_sys_alarm_args *));
int	svr4_sys_fstat __P((struct proc *, struct svr4_sys_fstat_args *));
int	svr4_sys_pause __P((struct proc *, struct svr4_sys_pause_args *));
int	svr4_sys_utime __P((struct proc *, struct svr4_sys_utime_args *));
int	svr4_sys_access __P((struct proc *, struct svr4_sys_access_args *));
int	svr4_sys_nice __P((struct proc *, struct svr4_sys_nice_args *));
int	svr4_sys_kill __P((struct proc *, struct svr4_sys_kill_args *));
int	svr4_sys_pgrpsys __P((struct proc *, struct svr4_sys_pgrpsys_args *));
int	svr4_sys_times __P((struct proc *, struct svr4_sys_times_args *));
int	svr4_sys_signal __P((struct proc *, struct svr4_sys_signal_args *));
#if defined(NOTYET)
int	svr4_sys_msgsys __P((struct proc *, struct svr4_sys_msgsys_args *));
#else
#endif
int	svr4_sys_sysarch __P((struct proc *, struct svr4_sys_sysarch_args *));
int	svr4_sys_ioctl __P((struct proc *, struct svr4_sys_ioctl_args *));
int	svr4_sys_utssys __P((struct proc *, struct svr4_sys_utssys_args *));
int	svr4_sys_execve __P((struct proc *, struct svr4_sys_execve_args *));
int	svr4_sys_fcntl __P((struct proc *, struct svr4_sys_fcntl_args *));
int	svr4_sys_ulimit __P((struct proc *, struct svr4_sys_ulimit_args *));
int	svr4_sys_getdents __P((struct proc *, struct svr4_sys_getdents_args *));
int	svr4_sys_getmsg __P((struct proc *, struct svr4_sys_getmsg_args *));
int	svr4_sys_putmsg __P((struct proc *, struct svr4_sys_putmsg_args *));
int	svr4_sys_poll __P((struct proc *, struct svr4_sys_poll_args *));
int	svr4_sys_lstat __P((struct proc *, struct svr4_sys_lstat_args *));
int	svr4_sys_sigprocmask __P((struct proc *, struct svr4_sys_sigprocmask_args *));
int	svr4_sys_sigsuspend __P((struct proc *, struct svr4_sys_sigsuspend_args *));
int	svr4_sys_sigaltstack __P((struct proc *, struct svr4_sys_sigaltstack_args *));
int	svr4_sys_sigaction __P((struct proc *, struct svr4_sys_sigaction_args *));
int	svr4_sys_sigpending __P((struct proc *, struct svr4_sys_sigpending_args *));
int	svr4_sys_context __P((struct proc *, struct svr4_sys_context_args *));
int	svr4_sys_statvfs __P((struct proc *, struct svr4_sys_statvfs_args *));
int	svr4_sys_fstatvfs __P((struct proc *, struct svr4_sys_fstatvfs_args *));
int	svr4_sys_waitsys __P((struct proc *, struct svr4_sys_waitsys_args *));
int	svr4_sys_hrtsys __P((struct proc *, struct svr4_sys_hrtsys_args *));
int	svr4_sys_pathconf __P((struct proc *, struct svr4_sys_pathconf_args *));
int	svr4_sys_mmap __P((struct proc *, struct svr4_sys_mmap_args *));
int	svr4_sys_fpathconf __P((struct proc *, struct svr4_sys_fpathconf_args *));
int	svr4_sys_xstat __P((struct proc *, struct svr4_sys_xstat_args *));
int	svr4_sys_lxstat __P((struct proc *, struct svr4_sys_lxstat_args *));
int	svr4_sys_fxstat __P((struct proc *, struct svr4_sys_fxstat_args *));
int	svr4_sys_xmknod __P((struct proc *, struct svr4_sys_xmknod_args *));
int	svr4_sys_setrlimit __P((struct proc *, struct svr4_sys_setrlimit_args *));
int	svr4_sys_getrlimit __P((struct proc *, struct svr4_sys_getrlimit_args *));
int	svr4_sys_memcntl __P((struct proc *, struct svr4_sys_memcntl_args *));
int	svr4_sys_uname __P((struct proc *, struct svr4_sys_uname_args *));
int	svr4_sys_sysconfig __P((struct proc *, struct svr4_sys_sysconfig_args *));
int	svr4_sys_systeminfo __P((struct proc *, struct svr4_sys_systeminfo_args *));
int	svr4_sys_fchroot __P((struct proc *, struct svr4_sys_fchroot_args *));
int	svr4_sys_utimes __P((struct proc *, struct svr4_sys_utimes_args *));
int	svr4_sys_vhangup __P((struct proc *, struct svr4_sys_vhangup_args *));
int	svr4_sys_gettimeofday __P((struct proc *, struct svr4_sys_gettimeofday_args *));
int	svr4_sys_llseek __P((struct proc *, struct svr4_sys_llseek_args *));
int	svr4_sys_acl __P((struct proc *, struct svr4_sys_acl_args *));
int	svr4_sys_auditsys __P((struct proc *, struct svr4_sys_auditsys_args *));
int	svr4_sys_facl __P((struct proc *, struct svr4_sys_facl_args *));
int	svr4_sys_resolvepath __P((struct proc *, struct svr4_sys_resolvepath_args *));
int	svr4_sys_getdents64 __P((struct proc *, struct svr4_sys_getdents64_args *));
int	svr4_sys_mmap64 __P((struct proc *, struct svr4_sys_mmap64_args *));
int	svr4_sys_stat64 __P((struct proc *, struct svr4_sys_stat64_args *));
int	svr4_sys_lstat64 __P((struct proc *, struct svr4_sys_lstat64_args *));
int	svr4_sys_fstat64 __P((struct proc *, struct svr4_sys_fstat64_args *));
int	svr4_sys_statvfs64 __P((struct proc *, struct svr4_sys_statvfs64_args *));
int	svr4_sys_fstatvfs64 __P((struct proc *, struct svr4_sys_fstatvfs64_args *));
int	svr4_sys_setrlimit64 __P((struct proc *, struct svr4_sys_setrlimit64_args *));
int	svr4_sys_getrlimit64 __P((struct proc *, struct svr4_sys_getrlimit64_args *));
int	svr4_sys_creat64 __P((struct proc *, struct svr4_sys_creat64_args *));
int	svr4_sys_open64 __P((struct proc *, struct svr4_sys_open64_args *));
int	svr4_sys_socket __P((struct proc *, struct svr4_sys_socket_args *));
int	svr4_sys_recv __P((struct proc *, struct svr4_sys_recv_args *));
int	svr4_sys_send __P((struct proc *, struct svr4_sys_send_args *));
int	svr4_sys_sendto __P((struct proc *, struct svr4_sys_sendto_args *));

#ifdef COMPAT_43

#if defined(NOTYET)
#else
#endif

#endif /* COMPAT_43 */

#undef PAD_

#endif /* !_SVR4_SYSPROTO_H_ */
