/*
 * DEBUG VERSION — with verbose logging to catch segfault location
 * Build: clang -static -Os -g -o alpine-sshd-wrapper-debug alpine-sshd-wrapper-debug.c
 */

#define _GNU_SOURCE
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/prctl.h>

#define CHROOT_PATH "/mnt/alpine"
#define SSHD_PATH   "/usr/sbin/sshd"
#define PORT        2222
#define BACKLOG     5

static volatile sig_atomic_t stop = 0;

static void log_msg(const char *msg) {
    write(STDERR_FILENO, msg, strlen(msg));
}

static void log_int(const char *prefix, int val) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%s: %d\n", prefix, val);
    write(STDERR_FILENO, buf, len);
}

static void handle_sig(int sig) {
    (void)sig;
    stop = 1;
}

static void reap_children(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

int main(void) {
    log_msg("=== WRAPPER START ===\n");

    // Signal handling
    log_msg("Setting up signals...\n");
    struct sigaction sa = { .sa_handler = handle_sig, .sa_flags = SA_RESTART };
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGHUP,  &sa, NULL);

    struct sigaction sa_chld = { .sa_handler = reap_children, .sa_flags = SA_RESTART | SA_NOCLDSTOP };
    sigaction(SIGCHLD, &sa_chld, NULL);
    log_msg("Signals done\n");

    // Create listening socket
    log_msg("Creating socket...\n");
    int lfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    log_int("socket() =", lfd);
    if (lfd < 0) {
        log_msg("socket failed\n");
        return 1;
    }

    int opt = 1;
    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_msg("setsockopt failed\n");
    }

    log_msg("Binding to 127.0.0.1:2222...\n");
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(PORT),
        .sin_addr   = { htonl(INADDR_LOOPBACK) }
    };

    if (bind(lfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_msg("bind failed\n");
        close(lfd);
        return 2;
    }
    log_msg("bind ok\n");

    if (listen(lfd, BACKLOG) < 0) {
        log_msg("listen failed\n");
        close(lfd);
        return 3;
    }
    log_msg("listen ok\n");

    // Check chroot path exists
    log_msg("Checking chroot path...\n");
    if (access(CHROOT_PATH, F_OK) != 0) {
        log_msg("CHROOT PATH DOES NOT EXIST: ");
        log_msg(CHROOT_PATH);
        log_msg("\n");
        // Don't exit - just log and continue for debug
    } else {
        log_msg("Chroot path exists\n");
    }

    // Main accept loop
    log_msg("Entering accept loop...\n");
    while (!stop) {
        log_msg("accept() blocking...\n");
        int cfd = accept(lfd, NULL, NULL);
        log_int("accept() =", cfd);
        if (cfd < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == ECONNABORTED) {
                log_msg("accept interrupted, continuing\n");
                continue;
            }
            log_msg("accept error\n");
            break;
        }

        log_msg("fork()...\n");
        pid_t pid = fork();
        log_int("fork() =", pid);
        if (pid < 0) {
            log_msg("fork failed\n");
            close(cfd);
            continue;
        }

        if (pid == 0) {
            // ========== CHILD PROCESS ==========
            log_msg("[CHILD] Started\n");
            close(lfd);

            log_msg("[CHILD] dup2 socket to stdio...\n");
            if (dup2(cfd, STDIN_FILENO) < 0)  { log_msg("[CHILD] dup2 stdin failed\n"); _exit(1); }
            if (dup2(cfd, STDOUT_FILENO) < 0) { log_msg("[CHILD] dup2 stdout failed\n"); _exit(1); }
            if (dup2(cfd, STDERR_FILENO) < 0) { log_msg("[CHILD] dup2 stderr failed\n"); _exit(1); }
            if (cfd > STDERR_FILENO) close(cfd);

            log_msg("[CHILD] chdir to chroot...\n");
            if (chdir(CHROOT_PATH) != 0) {
                log_msg("[CHILD] chdir failed: ");
                log_msg(strerror(errno));
                log_msg("\n");
                _exit(127);
            }
            log_msg("[CHILD] chroot...\n");
            if (chroot(".") != 0) {
                log_msg("[CHILD] chroot failed: ");
                log_msg(strerror(errno));
                log_msg("\n");
                _exit(127);
            }

            log_msg("[CHILD] exec sshd...\n");
            execl(SSHD_PATH, "sshd", "-D", "-e", "-i", (char*)NULL);

            log_msg("[CHILD] execl failed: ");
            log_msg(strerror(errno));
            log_msg("\n");
            _exit(127);
        }

        // ========== PARENT PROCESS ==========
        log_msg("[PARENT] Closing client fd, continuing\n");
        close(cfd);
    }

    log_msg("Exiting main loop\n");
    close(lfd);
    return 0;
}