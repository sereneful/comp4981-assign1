#ifndef WORKER_H
#define WORKER_H

/**
 * worker_loop - Main loop for the worker process.
 * @workerSock: The file descriptor for the UNIX domain socket used for
 *              communication with the monitor process.
 *
 * This function enters an infinite loop to receive client file descriptors,
 * process HTTP requests (GET, HEAD, POST), and then return the client FD to the monitor.
 * This function never returns.
 */
__attribute__((noreturn)) void worker_loop(int workerSock);

#endif /* WORKER_H */
