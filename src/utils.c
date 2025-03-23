#include "utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

int create_listen_socket(void)
{
    int                sock;
    struct sockaddr_in srvAddr;
    int                opt;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
    {
        perror("socket");
        return -1;
    }
    opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&srvAddr, 0, sizeof(srvAddr));
    srvAddr.sin_family      = AF_INET;
    srvAddr.sin_port        = htons(SERVER_PORT);
    srvAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sock, (struct sockaddr *)&srvAddr, sizeof(srvAddr)) < 0)
    {
        perror("bind");
        close(sock);
        return -1;
    }
    if(listen(sock, SOMAXCONN) < 0)
    {
        perror("listen");
        close(sock);
        return -1;
    }
    return sock;
}

int send_fd(int sock, int fd)
{
    int             ret;
    struct msghdr   msg;
    struct iovec    io;
    char            dummy;
    char            buf[CMSG_SPACE(sizeof(int))];
    struct cmsghdr *cmsg;

    memset(&msg, 0, sizeof(msg));
    memset(buf, 0, sizeof(buf));

    dummy              = 'X';
    io.iov_base        = &dummy;
    io.iov_len         = sizeof(dummy);
    msg.msg_iov        = &io;
    msg.msg_iovlen     = 1;
    msg.msg_control    = buf;
    msg.msg_controllen = sizeof(buf);

    cmsg             = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type  = SCM_RIGHTS;
    cmsg->cmsg_len   = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));

    ret = (int)sendmsg(sock, &msg, 0);
    if(ret < 0)
    {
        perror("sendmsg");
        return -1;
    }
    return 0;
}

int recv_fd(int sock)
{
    int             receivedFd;
    int             ret;
    struct msghdr   msg;
    struct iovec    io;
    char            m_buffer[1];
    char            ctrl_buf[CMSG_SPACE(sizeof(int))];
    struct cmsghdr *cmsg;

    memset(&msg, 0, sizeof(msg));
    memset(m_buffer, 0, sizeof(m_buffer));
    memset(ctrl_buf, 0, sizeof(ctrl_buf));

    io.iov_base        = m_buffer;
    io.iov_len         = sizeof(m_buffer);
    msg.msg_iov        = &io;
    msg.msg_iovlen     = 1;
    msg.msg_control    = ctrl_buf;
    msg.msg_controllen = sizeof(ctrl_buf);

    ret = (int)recvmsg(sock, &msg, 0);
    if(ret < 0)
    {
        perror("recvmsg");
        return -1;
    }
    cmsg = CMSG_FIRSTHDR(&msg);
    if(cmsg && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS)
    {
        memcpy(&receivedFd, CMSG_DATA(cmsg), sizeof(int));
        return receivedFd;
    }
    return -1;
}

time_t get_last_mod_time(const char *filename)
{
    struct stat st;
    if(stat(filename, &st) == 0)
    {
        return st.st_mtime;
    }
    return 0;
}

void format_time_str(time_t t, char *buf, size_t bufSize)
{
    struct tm tmVal;
    localtime_r(&t, &tmVal);
    strftime(buf, bufSize, "%Y-%m-%d %H:%M:%S", &tmVal);
}

void safe_ndbm_fetch(DBM *db, datum key, datum *result)
{
    {
        datum temp;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waggregate-return"
        temp = dbm_fetch(db, key);
#pragma GCC diagnostic pop
        result->dptr  = temp.dptr;
        result->dsize = temp.dsize;
    }
}
