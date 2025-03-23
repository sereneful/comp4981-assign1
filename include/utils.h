#ifndef UTILS_H
#define UTILS_H

#include "server.h"
#include <time.h>

/**
 * create_listen_socket - Creates a TCP listening socket.
 *
 * Returns a socket file descriptor bound to SERVER_PORT, or -1 on error.
 */
int create_listen_socket(void);

/**
 * send_fd - Sends a file descriptor over a UNIX domain socket.
 * @sock: The UNIX domain socket file descriptor.
 * @fd:   The file descriptor to send.
 *
 * Returns 0 on success, or -1 on error.
 */
int send_fd(int sock, int fd);

/**
 * recv_fd - Receives a file descriptor from a UNIX domain socket.
 * @sock: The UNIX domain socket file descriptor.
 *
 * Returns the received file descriptor on success, or -1 on error.
 */
int recv_fd(int sock);

/**
 * get_last_mod_time - Retrieves the last modification time of a file.
 * @filename: The file path.
 *
 * Returns the modification time as a time_t, or 0 on error.
 */
time_t get_last_mod_time(const char *filename);

/**
 * format_time_str - Formats a time_t value into a human-readable string.
 * @t:       The time_t value.
 * @buf:     Buffer where the formatted string is stored.
 * @bufSize: Size of the buffer.
 */
void format_time_str(time_t t, char *buf, size_t bufSize);

/**
 * safe_ndbm_fetch - Safely fetches a datum from an ndbm database.
 * @db:     Pointer to an open DBM database.
 * @key:    The key for the datum.
 * @result: Pointer to a datum structure where the result is stored.
 *
 * Calls dbm_fetch and copies the returned aggregate into *result.
 * Diagnostic pragmas are used to suppress warnings about aggregate returns.
 */
void safe_ndbm_fetch(DBM *db, datum key, datum *result);

#endif /* UTILS_H */
