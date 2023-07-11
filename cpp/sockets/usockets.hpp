#ifndef USOCKETS_HPP
#define USOCKETS_HPP

#include <atomic>
#include <map>
#include <sys/epoll.h>
#include <sys/un.h>
#include <vector>
#include <string>
#include <thread>

namespace usockets {
    class UNIXServer {
        private:
            int maxConnections;
            std::atomic_int currentlyConnected;
            std::atomic_bool endThread;
            std::vector<int> executedVec;
            std::map<int, std::string> inputCache;
            std::string path;
            void (*callback)(int, char*);
            std::thread t;

            int serverFd;
            struct sockaddr_un sock;

            void threadedServe();
            void handleEvent(const epoll_event& ev);

        public:
            UNIXServer(std::string path, int maxConnections);
            void start();
    };
}

#endif
