CC = g++
CFLAGS = -Wall -Werror -fpic
LDFLAGS = -shared
LIBRARY_NAME = libsockets.so

.PHONY: all clean

all: $(LIBRARY_NAME)

$(LIBRARY_NAME): sockets.cpp
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -f $(LIBRARY_NAME)
