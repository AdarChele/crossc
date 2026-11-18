/*
 * alpine-sshd-wrapper — Socket-activated SSH daemon launcher
 * Listens on 127.0.0.1:2222, on accept() spawns sshd -i inside chroot.
 * Zero idle processes, zero wakelocks. Static binary ~8KB.
 *
 * Build: aarch64-linux-android21-clang -static -Os -s -o alpine-sshd-wrapper alpine-sshd-wrapper.c
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

static void handle_sig(int sig) {
    (void)sig;
    stop = 1;
}

static void reap_children(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

int main(void) {
    // Signal handling
    struct sigaction sa = { .sa_handler = handle_sig, .sa_flags = SA_RESTART };
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGHUP,  &sa, NULL);

    struct sigaction sa_chld = { .sa_handler = reap_children, .sa_flags = SA_RESTART | SA_NOCLDSTOP };
    sigaction(SIGCHLD, &sa_chld, NULL);

    // Create listening socket
    int lfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (lfd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR");
    }

    // Bind to loopback only — no external exposure
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(PORT),
        .sin_addr   = { htonl(INADDR_LOOPBACK) }
    };

    if (bind(lfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(lfd);
        return 2;
    }

    if (listen(lfd, BACKLOG) < 0) {
        perror("listen");
        close(lfd);
        return 3;
    }

    // Optional: drop to unprivileged user inside chroot
    // setgid(1000); setuid(1000);  // uncomment if you create a user

    // Main accept loop
    while (!stop) {
        int cfd = accept(lfd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == ECONNABORTED) {
                continue;
            }
            perror("accept");
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(cfd);
            continue;
        }

        if (pid == 0) {
            // ========== CHILD PROCESS ==========
            close(lfd);  // Child doesn't need listener

            // Move accepted socket to stdio (inetd mode expects fd 0/1/2)
            if (dup2(cfd, STDIN_FILENO) < 0)  _exit(1);
            if (dup2(cfd, STDOUT_FILENO) < 0) _exit(1);
            if (dup2(cfd, STDERR_FILENO) < 0) _exit(1);
            if (cfd > STDERR_FILENO) close(cfd);

            // Enter chroot
            if (chdir(CHROOT_PATH) != 0) {
                perror("chdir");
                _exit(127);
            }
            if (chroot(".") != 0) {
                perror("chroot");
                _exit(127);
            }

            // Execute sshd in inetd mode (-i)
            // -D: don't daemonize, -e: log to stderr, -i: inetd mode
            execl(SSHD_PATH, "sshd", "-D", "-e", "-i", (char*)NULL);

            // If we get here, execl failed
            perror("execl sshd");
            _exit(127);
        }

        // ========== PARENT PROCESS ==========
        close(cfd);  // Parent doesn't need client socket
        // Child reaped by SIGCHLD handler
    }

    close(lfd);
    return 0;
}