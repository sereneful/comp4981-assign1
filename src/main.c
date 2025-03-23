#include "monitor.h"
#include "server.h"
#include "utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

static volatile sig_atomic_t stopServer = 0;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void setup_signal_handler(void);
void sig_handler(int sig);

int main(int argc, char *argv[])
{
    int                listenSock;
    int                clientSock;
    struct sockaddr_in cliAddr;
    socklen_t          cliLen;
    int                domainPair[2]; /* UNIX domain socket pair for main<->monitor communication */
    pid_t              monitorPid;
    fd_set             mainSet;

    /* Unused parameters */
    (void)argc;
    (void)argv;

    listenSock = create_listen_socket();
    if(listenSock < 0)
    {
        exit(EXIT_FAILURE);
    }

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, domainPair) < 0)
    {
        perror("socketpair");
        close(listenSock);
        exit(EXIT_FAILURE);
    }

    monitorPid = fork();
    if(monitorPid < 0)
    {
        perror("fork");
        close(listenSock);
        exit(EXIT_FAILURE);
    }
    else if(monitorPid == 0)
    {
        /* Monitor process */
        close(domainPair[0]);
        monitor_loop(domainPair[1]);    // monitor_loop is noreturn—no code after this call is needed.
    }
    /* Main process */
    close(domainPair[1]);

    setup_signal_handler();

    while(!stopServer)
    {
        int maxFD = (listenSock > domainPair[0]) ? listenSock : domainPair[0];
        FD_ZERO(&mainSet);
        FD_SET(listenSock, &mainSet);
        FD_SET(domainPair[0], &mainSet);

        if(select(maxFD + 1, &mainSet, NULL, NULL, NULL) < 0)
        {
            if(errno == EINTR)
            {
                continue;
            }
            perror("select");
            break;
        }

        if(FD_ISSET(listenSock, &mainSet))
        {
            cliLen     = sizeof(cliAddr);
            clientSock = accept(listenSock, (struct sockaddr *)&cliAddr, &cliLen);
            if(clientSock < 0)
            {
                perror("accept");
                continue;
            }
            if(send_fd(domainPair[0], clientSock) < 0)
            {
                close(clientSock);
                continue;
            }
            close(clientSock);
        }

        if(FD_ISSET(domainPair[0], &mainSet))
        {
            int retSock = recv_fd(domainPair[0]);
            if(retSock >= 0)
            {
                close(retSock);
            }
        }
    }

    close(listenSock);
    close(domainPair[0]);
    return 0;
}

void setup_signal_handler(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigaction(SIGINT, &sa, NULL);
}

void sig_handler(int sig)
{
    (void)sig;
    stopServer = 1;
}
