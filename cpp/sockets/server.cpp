#include "usockets.hpp"
#include <algorithm>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace usockets {

    void findAndRemove(std::vector<int>& v, int fd) {
        auto pos = std::find(v.begin(), v.end(), fd);
        v.erase(pos);
    }


    UNIXServer::UNIXServer(std::string path, int maxConnections) {
        this->path = path;
        this->maxConnections = maxConnections;

        this->serverFd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
        this->sock.sun_family = AF_UNIX;
        std::memcpy(this->sock.sun_path, path.c_str(), path.length());

        if (bind(this->serverFd, (struct sockaddr*) &this->sock, sizeof(this->sock)) == -1) {
            perror("Failed to bind socket");
        }

        this->endThread = false;
    }

    void UNIXServer::threadedServe() {
        int epollFd = epoll_create(this->maxConnections);
        struct epoll_event events[this->maxConnections];

        while (!this->endThread) {
            int clientFd = accept4(this->serverFd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
            bool handleReq = false;
            if (clientFd == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK);
                else {
                    perror("Error accepting connection");
                    return;
                }
            } else {
                this->currentlyConnected++;
                handleReq = true;
            }

            if (handleReq) {
                if (this->currentlyConnected >= this->maxConnections) close(clientFd);
                else {
                    struct epoll_event* ev = new struct epoll_event;
                    ev->events = EPOLLIN | EPOLLOUT | EPOLLRDHUP;
                    ev->data.fd = clientFd;
                    epoll_ctl(epollFd, EPOLL_CTL_ADD, clientFd, ev);
                }
            }

            int evCount = epoll_wait(epollFd, events, this->maxConnections, this->maxConnections / 2);

            for (int i = 0; i < evCount; i++) {
                const auto& e = events[i];
                this->handleEvent(e);
            }
        }
    }

    void UNIXServer::handleEvent(const epoll_event& ev) {
        int fd = ev.data.fd;
        if (ev.events & EPOLLRDHUP) {
            close(fd);
            this->inputCache[fd] = "";
            findAndRemove(this->executedVec, fd);
            this->currentlyConnected--;
            return;
        }

        if (ev.events & EPOLLIN) {
            char buff[1024];
            int bytesRead = read(fd, buff, 1024);

            if (bytesRead) this->inputCache[fd].append(buff, bytesRead);
            return;
        }

        if (ev.events & EPOLLOUT) {
            auto pos = std::find(this->executedVec.begin(), this->executedVec.end(), fd);
            if (pos == this->executedVec.end()) {
                this->executedVec.push_back(fd);
                this->callback(fd, strdup(this->inputCache[fd].c_str()));
                this->inputCache[fd] = "";
            }
        }
    }

    void UNIXServer::start() {
        if (listen(this->serverFd, this->maxConnections) == -1) {
            perror("Failed to listen");
            return;
        }

        std::thread thr([this]() {
                this->threadedServe();
        });

        thr.detach();
        this->t = std::move(thr);
    }
}
