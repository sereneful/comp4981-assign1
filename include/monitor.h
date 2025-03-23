#ifndef MONITOR_H
#define MONITOR_H

#include "server.h"

/**
 * monitor_loop - Main loop for the monitor process.
 * @monitorSock: A file descriptor for the UNIX domain socket used for
 *               communication with the main process.
 *
 * This function forks worker processes and then enters a loop to receive
 * new client file descriptors from the main process and dispatch them to
 * workers in round-robin fashion. It also monitors worker processes and
 * respawns any that terminate. This function never returns.
 */
__attribute__((noreturn)) void monitor_loop(int monitorSock);

#endif /* MONITOR_H */
