#include <algorithm>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <vector>
#ifdef __linux__
#include <sys/epoll.h>
#else
#error "epoll: I don't understand the language of your tribe"
#endif
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

extern "C" {

typedef struct {
    int serverFd;
    int count;
    struct sockaddr_un serverAddr;
    char *path;
    pthread_t threadId;
    void (*callback)(int, char*);
    std::vector<int>* cache;
} UNIXSock;

typedef struct {
    int clientFd;
    char *path;
    void (*callback)(int, char*);
    struct sockaddr_un serverAddr;
    pthread_t threadId;
    bool alreadyRun;
} UNIXSockConn;

UNIXSock *initSocket(char *path, int count) {
    UNIXSock* ptr = (UNIXSock*) malloc(sizeof(UNIXSock));
    ptr->serverFd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    ptr->serverAddr.sun_family = AF_UNIX;
    ptr->count = count;
    strcpy(ptr->serverAddr.sun_path, path);

    if (bind(ptr->serverFd, (struct sockaddr*) &ptr->serverAddr,
             sizeof(ptr->serverAddr)) == -1) {
        perror("Failed to bind socket");
        return NULL;
    }

    return ptr;
}

UNIXSockConn *initClient(char *path) {
    UNIXSockConn* ptr = (UNIXSockConn*) malloc(sizeof(UNIXSockConn));
    ptr->clientFd = socket(AF_UNIX, SOCK_STREAM, 0);
    ptr->serverAddr.sun_family = AF_UNIX;
    strcpy(ptr->serverAddr.sun_path, path);
    ptr->alreadyRun = false;

    return ptr;
}

void *threadServe(void *a);
void *threadConnect(void *a);

void runServer(UNIXSock *ptr, void (*callback)(int, char*)) {
    if (listen(ptr->serverFd, ptr->count) == -1) {
        perror("Failed to listen");
        return;
    }

    pthread_t id;
    ptr->callback = callback;
    pthread_create(&id, NULL, threadServe, ptr);
    ptr->threadId = id;
}

int getConnectionFd(UNIXSockConn *ptr) {
    return ptr->clientFd;
}

void runConnection(UNIXSockConn *ptr, void (*callback)(int, char*)) {
    if (connect(ptr->clientFd, (struct sockaddr*) &ptr->serverAddr, sizeof(ptr->serverAddr)) == -1) {
        perror("Failed to connect to server");
        return;
    }

    pthread_t id;
    ptr->callback = callback;
    pthread_create(&id, NULL, threadConnect, ptr);
    ptr->threadId = id;
}

void *threadConnect(void *a) {
    UNIXSockConn* ptr = (UNIXSockConn*) a;

    int epollFd = epoll_create(1);
    struct epoll_event events[1];

    struct epoll_event *event = new struct epoll_event;
    event->events = EPOLLIN | EPOLLOUT | EPOLLRDHUP;
    event->data.fd = ptr->clientFd;

    std::string cache = "";

    while (true) {
        epoll_wait(epollFd, events, 1, -1);
        const struct epoll_event& e = events[0];
        if (e.events & EPOLLIN) {
            char buff[1024];
            int bytesRead = read(ptr->clientFd, buff, 1024);

            printf("%s\n", buff);

            if (bytesRead) cache.append(buff, bytesRead);
            continue;
        }
        if (e.events & EPOLLOUT) {
            if (!ptr->alreadyRun) {
                ptr->callback(ptr->clientFd, strdup(cache.c_str()));
                cache = "";
                ptr->alreadyRun = true;
            }
        }
        if (e.events & EPOLLRDHUP) {
            close(e.data.fd);
            break;
        }
    }

    return NULL;
}

void terminateClient(UNIXSockConn *ptr) {
    pthread_cancel(ptr->threadId);

    close(ptr->clientFd);
}

void writeToFd(int clientFd, char *data, int len) {
    write(clientFd, data, len);
    fsync(clientFd);
}

void terminate(UNIXSock *ptr) {
    pthread_cancel(ptr->threadId);

    close(ptr->serverFd);
}

void forceClose(UNIXSock *p, int fd) {
    auto index = std::find(p->cache->begin(), p->cache->end(), fd);
    if (index != p->cache->end()) p->cache->erase(index);
}

void *threadServe(void *a) {
    UNIXSock *ptr = (UNIXSock *) a;

    int fdCount = 0;
    int epollFd = epoll_create(ptr->count);

    struct epoll_event events[ptr->count];

    std::vector<int> cachedFds;
    std::map<int, std::string> inCache;

    ptr->cache = &cachedFds;
    
    while (1) {
        int cfd = accept4(ptr->serverFd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
        int handleReq = 0;
        if (cfd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
            } else {
                perror("Error accepting connection");
                return NULL;
            }
        } else {
            fdCount++;
            handleReq = 1;
        }

        if (handleReq) {
            if (fdCount >= ptr->count) {
                close(cfd);
            } else {
                struct epoll_event *event = new struct epoll_event;
                event->events = EPOLLIN | EPOLLOUT;
                event->data.fd = cfd;
                epoll_ctl(epollFd, EPOLL_CTL_ADD, cfd, event);
            }
        }

        int numberOfEvents = epoll_wait(epollFd, events, ptr->count, 10);

        for (int i = 0; i < numberOfEvents; i++) {
            int fd = events[i].data.fd;
            if (events[i].events & EPOLLIN) {
                char buff[1024];
                int bytesRead = read(fd, buff, 1024);

                if (bytesRead) inCache[fd].append(buff, bytesRead);
                continue;
            }

            if (events[i].events & EPOLLOUT) {
                auto alreadyRun = std::find(cachedFds.begin(), cachedFds.end(), fd);
                if (alreadyRun == cachedFds.end()) {
                    cachedFds.push_back(fd);
                    ptr->callback(fd, strdup(inCache[fd].c_str()));
                    inCache[fd] = "";
                }
            }
        }
    }

    return NULL;
}

}
