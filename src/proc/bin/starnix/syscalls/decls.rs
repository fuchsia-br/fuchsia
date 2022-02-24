// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![allow(non_upper_case_globals)]

use paste::paste;

use crate::types::*;

/// Intended to be used with other macros to produce code that needs to handle
/// each syscall.
macro_rules! for_each_syscall {
    {$callback:ident $(,$context:ident)*} => {
        $callback!{
            $($context;)*
            read,
            write,
            open,
            close,
            stat,
            fstat,
            lstat,
            poll,
            lseek,
            mmap,
            mprotect,
            munmap,
            brk,
            rt_sigaction,
            rt_sigprocmask,
            rt_sigreturn,
            ioctl,
            pread64,
            pwrite64,
            readv,
            writev,
            access,
            pipe,
            select,
            sched_yield,
            mremap,
            msync,
            mincore,
            madvise,
            shmget,
            shmat,
            shmctl,
            dup,
            dup2,
            pause,
            nanosleep,
            getitimer,
            alarm,
            setitimer,
            getpid,
            sendfile,
            socket,
            connect,
            accept,
            sendto,
            recvfrom,
            sendmsg,
            recvmsg,
            shutdown,
            bind,
            listen,
            getsockname,
            getpeername,
            socketpair,
            setsockopt,
            getsockopt,
            clone,
            fork,
            vfork,
            execve,
            exit,
            wait4,
            kill,
            uname,
            semget,
            semop,
            semctl,
            shmdt,
            msgget,
            msgsnd,
            msgrcv,
            msgctl,
            fcntl,
            flock,
            fsync,
            fdatasync,
            truncate,
            ftruncate,
            getdents,
            getcwd,
            chdir,
            fchdir,
            rename,
            mkdir,
            rmdir,
            creat,
            link,
            unlink,
            symlink,
            readlink,
            chmod,
            fchmod,
            chown,
            fchown,
            lchown,
            umask,
            gettimeofday,
            getrlimit,
            getrusage,
            sysinfo,
            times,
            ptrace,
            getuid,
            syslog,
            getgid,
            setuid,
            setgid,
            geteuid,
            getegid,
            setpgid,
            getppid,
            getpgrp,
            setsid,
            setreuid,
            setregid,
            getgroups,
            setgroups,
            setresuid,
            getresuid,
            setresgid,
            getresgid,
            getpgid,
            setfsuid,
            setfsgid,
            getsid,
            capget,
            capset,
            rt_sigpending,
            rt_sigtimedwait,
            rt_sigqueueinfo,
            rt_sigsuspend,
            sigaltstack,
            utime,
            mknod,
            uselib,
            personality,
            ustat,
            statfs,
            fstatfs,
            sysfs,
            getpriority,
            setpriority,
            sched_setparam,
            sched_getparam,
            sched_setscheduler,
            sched_getscheduler,
            sched_get_priority_max,
            sched_get_priority_min,
            sched_rr_get_interval,
            mlock,
            munlock,
            mlockall,
            munlockall,
            vhangup,
            modify_ldt,
            pivot_root,
            _sysctl,
            prctl,
            arch_prctl,
            adjtimex,
            setrlimit,
            chroot,
            sync,
            acct,
            settimeofday,
            mount,
            umount2,
            swapon,
            swapoff,
            reboot,
            sethostname,
            setdomainname,
            iopl,
            ioperm,
            create_module,
            init_module,
            delete_module,
            get_kernel_syms,
            query_module,
            quotactl,
            nfsservctl,
            getpmsg,
            putpmsg,
            afs_syscall,
            tuxcall,
            security,
            gettid,
            readahead,
            setxattr,
            lsetxattr,
            fsetxattr,
            getxattr,
            lgetxattr,
            fgetxattr,
            listxattr,
            llistxattr,
            flistxattr,
            removexattr,
            lremovexattr,
            fremovexattr,
            tkill,
            time,
            futex,
            sched_setaffinity,
            sched_getaffinity,
            set_thread_area,
            io_setup,
            io_destroy,
            io_getevents,
            io_submit,
            io_cancel,
            get_thread_area,
            lookup_dcookie,
            epoll_create,
            epoll_ctl_old,
            epoll_wait_old,
            remap_file_pages,
            getdents64,
            set_tid_address,
            restart_syscall,
            semtimedop,
            fadvise64,
            timer_create,
            timer_settime,
            timer_gettime,
            timer_getoverrun,
            timer_delete,
            clock_settime,
            clock_gettime,
            clock_getres,
            clock_nanosleep,
            exit_group,
            epoll_wait,
            epoll_ctl,
            tgkill,
            utimes,
            vserver,
            mbind,
            set_mempolicy,
            get_mempolicy,
            mq_open,
            mq_unlink,
            mq_timedsend,
            mq_timedreceive,
            mq_notify,
            mq_getsetattr,
            kexec_load,
            waitid,
            add_key,
            request_key,
            keyctl,
            ioprio_set,
            ioprio_get,
            inotify_init,
            inotify_add_watch,
            inotify_rm_watch,
            migrate_pages,
            openat,
            mkdirat,
            mknodat,
            fchownat,
            futimesat,
            newfstatat,
            unlinkat,
            renameat,
            linkat,
            symlinkat,
            readlinkat,
            fchmodat,
            faccessat,
            pselect6,
            ppoll,
            unshare,
            set_robust_list,
            get_robust_list,
            splice,
            tee,
            sync_file_range,
            vmsplice,
            move_pages,
            utimensat,
            epoll_pwait,
            signalfd,
            timerfd_create,
            eventfd,
            fallocate,
            timerfd_settime,
            timerfd_gettime,
            accept4,
            signalfd4,
            eventfd2,
            epoll_create1,
            dup3,
            pipe2,
            inotify_init1,
            preadv,
            pwritev,
            rt_tgsigqueueinfo,
            perf_event_open,
            recvmmsg,
            fanotify_init,
            fanotify_mark,
            prlimit64,
            name_to_handle_at,
            open_by_handle_at,
            clock_adjtime,
            syncfs,
            sendmmsg,
            setns,
            getcpu,
            process_vm_readv,
            process_vm_writev,
            kcmp,
            finit_module,
            sched_setattr,
            sched_getattr,
            renameat2,
            seccomp,
            getrandom,
            memfd_create,
            kexec_file_load,
            bpf,
            execveat,
            userfaultfd,
            membarrier,
            mlock2,
            copy_file_range,
            preadv2,
            pwritev2,
            pkey_mprotect,
            pkey_alloc,
            pkey_free,
            statx,
            io_pgetevents,
            rseq,
            pidfd_send_signal,
            io_uring_setup,
            io_uring_enter,
            io_uring_register,
            open_tree,
            move_mount,
            fsopen,
            fsconfig,
            fsmount,
            fspick,
            pidfd_open,
            clone3,
            close_range,
            openat2,
            pidfd_getfd,
            faccessat2,
            process_madvise,
        }
    }
}

/// A system call declaration.
///
/// Describes the name of the syscall and its number.
///
/// TODO: Add information about the number of arguments (and their types) so
/// that we can make strace more useful.
pub struct SyscallDecl {
    pub name: &'static str,
    pub number: u64,
}

/// A macro for declaring a const SyscallDecl for a given syscall.
///
/// The constant will be called DECL_<SYSCALL>.
macro_rules! syscall_decl {
    {$($name:ident,)*} => {
        paste! {
            $(pub const [<DECL_ $name:upper>]: SyscallDecl = SyscallDecl { name: stringify!($name), number: [<__NR_ $name>] as u64};)*
        }
    }
}

// Produce each syscall declaration.
for_each_syscall! {syscall_decl}

/// A declaration for an unknown syscall.
///
/// Useful so that functions that return a SyscallDecl have a sentinel
/// to return when they cannot find an appropriate syscall.
pub const DECL_UNKNOWN: SyscallDecl = SyscallDecl { name: "<unknown>", number: 0xFFFF };

/// A macro for the body of SyscallDecl::from_number.
///
/// Evaluates to the &'static SyscallDecl for the given number or to
/// &DECL_UNKNOWN if the number is unknown.
macro_rules! syscall_match {
    {$number:ident; $($name:ident,)*} => {
        paste! {
            match $number as u32 {
                $([<__NR_ $name>] => &[<DECL_ $name:upper>],)*
                _ => &DECL_UNKNOWN,
            }
        }
    }
}

impl SyscallDecl {
    /// The SyscallDecl for the given syscall number.
    ///
    /// Returns &DECL_UNKNOWN if the given syscall number is not known.
    pub fn from_number(number: u64) -> &'static SyscallDecl {
        for_each_syscall! { syscall_match, number }
    }
}