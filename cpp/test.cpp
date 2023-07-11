#include "sockets/usockets.hpp"

int main() {
    auto sock = new usockets::UNIXServer("/tmp/xd", 1024);
    sock->start();

    while (true);
    return 0;
}
