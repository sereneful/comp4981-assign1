#include "monitor.h"
#include "server.h"
#include "utils.h"
#include "worker.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h> /* for socketpair, AF_UNIX, SOCK_STREAM */
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

__attribute__((noreturn)) void monitor_loop(int monitorSock)
{
    int    workerSockets[WORKER_COUNT][2];
    pid_t  workerPIDs[WORKER_COUNT];
    int    rrIndex = 0; /* Round-robin index */
    fd_set monSet;
    int    maxFD = monitorSock;
    int    i;

    for(i = 0; i < WORKER_COUNT; i++)
    {
        if(socketpair(AF_UNIX, SOCK_STREAM, 0, workerSockets[i]) < 0)
        {
            perror("monitor socketpair");
            exit(EXIT_FAILURE);
        }
        {
            pid_t pid = fork();
            if(pid < 0)
            {
                perror("fork worker");
                exit(EXIT_FAILURE);
            }
            if(pid == 0)
            {
                close(workerSockets[i][0]);
                worker_loop(workerSockets[i][1]);
                /* No exit needed here as worker_loop is noreturn */
            }
            else
            {
                workerPIDs[i] = pid;
                close(workerSockets[i][1]);
                if(workerSockets[i][0] > maxFD)
                {
                    maxFD = workerSockets[i][0];
                }
            }
        }
    }

    while(1)
    {
        int   ret;
        int   status;
        pid_t deadPid;
        FD_ZERO(&monSet);
        FD_SET(monitorSock, &monSet);
        for(i = 0; i < WORKER_COUNT; i++)
        {
            FD_SET(workerSockets[i][0], &monSet);
        }
        ret = select(maxFD + 1, &monSet, NULL, NULL, NULL);
        if(ret < 0)
        {
            perror("monitor select");
            continue;
        }
        if(FD_ISSET(monitorSock, &monSet))
        {
            int cliFd = recv_fd(monitorSock);    // Narrowed scope for cliFd
            if(cliFd >= 0)
            {
                if(send_fd(workerSockets[rrIndex][0], cliFd) < 0)
                {
                    close(cliFd);
                }
                printf("Monitor: Dispatched client FD %d to worker %d\n", cliFd, rrIndex);
                rrIndex = (rrIndex + 1) % WORKER_COUNT;
                close(cliFd);
            }
        }
        for(i = 0; i < WORKER_COUNT; i++)
        {
            if(FD_ISSET(workerSockets[i][0], &monSet))
            {
                int procFd = recv_fd(workerSockets[i][0]);    // Declare procFd in this narrow scope
                if(procFd >= 0)
                {
                    if(send_fd(monitorSock, procFd) < 0)
                    {
                        close(procFd);
                    }
                    printf("Monitor: Received processed FD %d from worker %d\n", procFd, i);
                    close(procFd);
                }
            }
        }
        deadPid = waitpid(-1, &status, WNOHANG);
        if(deadPid > 0)
        {
            for(int j = 0; j < WORKER_COUNT; j++)    // Declare 'j' in the for-loop header
            {
                if(workerPIDs[j] == deadPid)
                {
                    printf("Monitor: Worker %d (PID %d) terminated; respawning...\n", j, (int)deadPid);
                    close(workerSockets[j][0]);
                    if(socketpair(AF_UNIX, SOCK_STREAM, 0, workerSockets[j]) < 0)
                    {
                        perror("socketpair respawn");
                        continue;
                    }
                    {
                        pid_t newPid = fork();    // Narrowed scope for newPid
                        if(newPid < 0)
                        {
                            perror("fork respawn");
                            continue;
                        }
                        if(newPid == 0)
                        {
                            close(workerSockets[j][0]);
                            worker_loop(workerSockets[j][1]);
                        }
                        else
                        {
                            workerPIDs[j] = newPid;
                            close(workerSockets[j][1]);
                        }
                    }
                    break;
                }
            }
        }
    }
}
