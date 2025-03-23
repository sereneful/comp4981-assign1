#ifndef SERVER_H
#define SERVER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <ndbm.h>  /* Use the system’s native ndbm header */

#define SERVER_PORT       8080
#define BUF_SIZE          1024
#define WORKER_COUNT      3
#define TIME_STR_SIZE     64
#define POST_DELIM_OFFSET 4  /* Offset past "\r\n\r\n" */

/* HTTP response messages */
#define HTTP_OK_MSG       "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\nHello, world!"
#define HTTP_405_MSG      "HTTP/1.1 405 Method Not Allowed\r\n\r\n"
#define HTTP_404_MSG      "HTTP/1.1 404 Not Found\r\n\r\n"

/* Linux-only definitions */
typedef int datum_size;
#define DPT_CAST(ptr) (ptr)

/* Type definition for ndbm data lengths */
typedef int datum_length;

#endif /* SERVER_H */
