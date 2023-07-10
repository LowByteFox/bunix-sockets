#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

typedef struct {
    int serverFd;
    struct sockaddr_un serverAddr;
    char *path;
    pthread_t threadId;
    void (*callback)(int);
} UNIXSock;

UNIXSock *initSocket(char *path) {
    UNIXSock* ptr = malloc(sizeof(UNIXSock));
    ptr->serverFd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    ptr->serverAddr.sun_family = AF_UNIX;
    strcpy(ptr->serverAddr.sun_path, path);

    if (bind(ptr->serverFd, (struct sockaddr*) &ptr->serverAddr,
             sizeof(ptr->serverAddr)) == -1) {
        perror("Failed to bind socket");
        return NULL;
    }

    return ptr;
}

void *threadServe(void *a);

void runServer(UNIXSock *ptr, void (*callback)(int)) {
    if (listen(ptr->serverFd, SOMAXCONN) == -1) {
        perror("Failed to listen");
        return;
    }

    pthread_t id;
    ptr->callback = callback;
    pthread_create(&id, NULL, threadServe, ptr);
    ptr->threadId = id;
}

void terminate(UNIXSock *ptr) {
    pthread_cancel(ptr->threadId);

    close(ptr->serverFd);
    unlink(ptr->path);
}

void *threadServe(void *a) {
    UNIXSock *ptr = a;

    while (1) {
        int cfd = accept4(ptr->serverFd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (cfd == -1) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            continue;
          } else {
            perror("Error accepting connection");
            return NULL;
          }
        }

        ptr->callback(cfd);
    }

    return NULL;
}
