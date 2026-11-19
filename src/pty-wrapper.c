// pty-wrapper.c — gives dropbear a CTTY, then execs it
#define _GNU_SOURCE
#include <pty.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

int main(int argc, char **argv) {
    int master;
    char slave_name[64];
    pid_t pid;

    // Allocate PTY, become session leader
    pid = forkpty(&master, slave_name, NULL, NULL);
    if (pid == -1) return 1;

    if (pid == 0) {
        // CHILD: has CTTY (slave side), exec dropbear
        // dropbear will inherit this CTTY for its child shells
        execvp(argv[1], &argv[1]);
        _exit(127);
    }

    // PARENT: pump data (or just wait)
    // For daemon: close master, waitpid
    close(master);
    waitpid(pid, NULL, 0);
    return 0;
}