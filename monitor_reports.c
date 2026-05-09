#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "fs_utils.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested = 0;

static void handle_monitor_signal(int signo)
{
    if (signo == SIGUSR1) {
        static const char message[] =
            "monitor_reports: new report added (SIGUSR1)\n";
        (void)write(STDOUT_FILENO, message, sizeof(message) - 1);
    } else if (signo == SIGINT) {
        static const char message[] =
            "monitor_reports: SIGINT received, exiting\n";
        (void)write(STDOUT_FILENO, message, sizeof(message) - 1);
        stop_requested = 1;
    }
}

static int install_handler(int signo)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_monitor_signal;
    sigemptyset(&action.sa_mask);

    if (sigaction(signo, &action, NULL) == -1) {
        cm_errno("sigaction");
        return -1;
    }

    return 0;
}

static int write_pid_file(void)
{
    int fd;

    fd = open(CM_MONITOR_PID_FILE,
              O_WRONLY | O_CREAT | O_TRUNC,
              CM_LOG_MODE);
    if (fd == -1) {
        cm_errno(CM_MONITOR_PID_FILE);
        return -1;
    }

    if (cm_writef(fd, "%ld\n", (long)getpid()) == -1 ||
        fsync(fd) == -1) {
        cm_errno(CM_MONITOR_PID_FILE);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int main(void)
{
    if (install_handler(SIGUSR1) == -1 ||
        install_handler(SIGINT) == -1 ||
        write_pid_file() == -1) {
        return 1;
    }

    cm_writef(STDOUT_FILENO,
              "monitor_reports: PID %ld written to %s\n",
              (long)getpid(),
              CM_MONITOR_PID_FILE);

    while (!stop_requested) {
        pause();
    }

    if (unlink(CM_MONITOR_PID_FILE) == -1 && errno != ENOENT) {
        cm_errno(CM_MONITOR_PID_FILE);
        return 1;
    }

    return 0;
}
