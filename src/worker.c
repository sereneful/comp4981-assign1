#include "worker.h"
#include "server.h"
#include "utils.h"
#include <arpa/inet.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* Named constants to avoid magic numbers */
#define MAGIC_POST_LENGTH 5
#define MAGIC_HEAD_LENGTH 5
#define MAX_METHOD_LENGTH 8
#define MAX_PATH_LENGTH 256
#define MAX_FULLPATH_LENGTH 512

/* Local function prototypes */
static int handle_client_request(int clientSock, void **libHandle, time_t *libLastMod);
static int serve_file_response(int clientSock, const char *req);
static int process_post_request(const char *req);

__attribute__((noreturn)) void worker_loop(int workerSock)
{
    void  *libHandle;
    time_t libLastMod;

    libHandle = dlopen("../http.so", RTLD_NOW);
    if(!libHandle)
    {
        fprintf(stderr, "Worker: Failed to load shared library: %s\n", dlerror());
        exit(EXIT_FAILURE);
    }
    libLastMod = get_last_mod_time("../http.so");

    while(1)
    {
        { /* Begin inner block to narrow scope of clientFd and ret */
            int clientFd = recv_fd(workerSock);
            int ret;
            if(clientFd < 0)
            {
                perror("Worker: Failed to receive client FD");
                continue; /* Continue the while loop */
            }
            ret = handle_client_request(clientFd, &libHandle, &libLastMod);
            if(ret != 0)
            {
                fprintf(stderr, "Worker: Error processing client request\n");
            }
            if(send_fd(workerSock, clientFd) < 0)
            {
                perror("Worker: Failed to return client FD");
            }
            close(clientFd);
        } /* End inner block; clientFd and ret go out of scope here */
    }
    dlclose(libHandle); /* Unreachable */
}

static int handle_client_request(int clientSock, void **libHandle, time_t *libLastMod)
{
    char    reqBuf[BUF_SIZE];
    ssize_t bytesRead;
    int     ret;
    void (*httpHandler)(const char *);
    time_t currMod;

    union
    {
        void *p;
        void (*f)(const char *);
    } funcptr;

    memset(reqBuf, 0, BUF_SIZE);
    bytesRead = read(clientSock, reqBuf, BUF_SIZE);
    if(bytesRead < 0)
    {
        perror("Worker: read error");
        return 1;
    }

    if(strncmp(reqBuf, "POST ", MAGIC_POST_LENGTH) == 0)
    {
        printf("Worker: POST request received\n");
        ret = process_post_request(reqBuf);
        if(ret != 0)
        {
            fprintf(stderr, "Worker: POST processing error\n");
        }
    }
    else if((strncmp(reqBuf, "GET ", 4) == 0) || (strncmp(reqBuf, "HEAD ", MAGIC_HEAD_LENGTH) == 0))
    {
        printf("Worker: GET/HEAD request received\n");
        ret = serve_file_response(clientSock, reqBuf);
        if(ret != 0)
        {
            fprintf(stderr, "Worker: File serving error\n");
        }
        return 0;
    }
    else
    {
        send(clientSock, HTTP_405_MSG, strlen(HTTP_405_MSG), 0);
        return 0;
    }

    currMod = get_last_mod_time("../http.so");
    if(currMod > *libLastMod)
    {
        printf("Worker: Reloading updated shared library...\n");
        if(*libHandle)
        {
            dlclose(*libHandle);
        }
        *libHandle = dlopen("../http.so", RTLD_NOW);
        if(!*libHandle)
        {
            fprintf(stderr, "Worker: Reload error: %s\n", dlerror());
            return 1;
        }
        *libLastMod = currMod;
    }
    funcptr.p   = dlsym(*libHandle, "handle_http_request");
    httpHandler = funcptr.f;
    if(!httpHandler)
    {
        fprintf(stderr, "Worker: dlsym error: %s\n", dlerror());
        return 1;
    }
    httpHandler(reqBuf);
    send(clientSock, HTTP_OK_MSG, strlen(HTTP_OK_MSG), 0);
    return 0;
}

static int serve_file_response(int clientSock, const char *req)
{
    char  method[MAX_METHOD_LENGTH];
    char  reqPath[MAX_PATH_LENGTH];
    char  fullPath[MAX_FULLPATH_LENGTH];
    FILE *fp;
    long  fileSize;

    memset(method, 0, sizeof(method));
    memset(reqPath, 0, sizeof(reqPath));
    memset(fullPath, 0, sizeof(fullPath));
    fp = NULL;

    /* Use width specifiers to prevent buffer overflow:
       %7s for method (max 7 characters plus null),
       %255s for reqPath (max 255 characters plus null) */
    sscanf(req, "%7s %255s", method, reqPath);
    snprintf(fullPath, sizeof(fullPath), "./www%s", reqPath);

    /* Use mode "rbe" to set O_CLOEXEC if supported */
    fp = fopen(fullPath, "rbe");
    if(!fp)
    {
        send(clientSock, HTTP_404_MSG, strlen(HTTP_404_MSG), 0);
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    {
        char header[MAX_PATH_LENGTH];
        memset(header, 0, sizeof(header));
        snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\nContent-Type: text/html\r\n\r\n", fileSize);
        send(clientSock, header, strlen(header), 0);
    }
    if(strcmp(method, "HEAD") != 0)
    {
        /* Declare fileBuffer and readBytes only for this block */
        char   fileBuffer[BUF_SIZE];
        size_t readBytes;
        while((readBytes = fread(fileBuffer, 1, BUF_SIZE, fp)) > 0)
        {
            send(clientSock, fileBuffer, readBytes, 0);
        }
    }
    fclose(fp);
    return 0;
}

static int process_post_request(const char *req)
{
    DBM  *db;
    datum key;
    datum value;
    datum fetched;
    /* cppcheck-suppress constVariable */
    char dbFile[] = "requests_db"; /* Non-const array is required by the ndbm API */
    /* cppcheck-suppress constVariable */
    char  keyStr[] = "post_data";
    char *body;
    int   ret;

    body = strstr(req, "\r\n\r\n");
    if(body)
    {
        body += POST_DELIM_OFFSET;
        printf("Worker: POST body: %s\n", body);
    }
    else
    {
        printf("Worker: No POST data found\n");
        return 1;
    }
    db = dbm_open(dbFile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(!db)
    {
        perror("dbm_open");
        return 1;
    }
    key.dptr    = keyStr;
    key.dsize   = (datum_length)strlen(keyStr) + 1;
    value.dptr  = body;
    value.dsize = (datum_length)strlen(body) + 1;
    ret         = dbm_store(db, key, value, DBM_REPLACE);
    if(ret != 0)
    {
        perror("dbm_store");
        dbm_close(db);
        return 1;
    }
    safe_ndbm_fetch(db, key, &fetched);
    if(fetched.dptr)
    {
        printf("Worker: Stored POST data: %s\n", DPT_CAST(fetched.dptr));
    }
    else
    {
        printf("Worker: POST data not retrieved\n");
    }
    dbm_close(db);
    return 0;
}
