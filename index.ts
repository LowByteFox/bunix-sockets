import { FFIType, dlopen, ptr, CString, JSCallback } from "bun:ffi";
import { constants } from "crypto";
import EventEmitter from "events";
import { closeSync, fdatasyncSync, fstatSync, readSync, statSync, unlinkSync, writeSync } from "fs";

const { symbols } = dlopen("libsockets.so", {
    initSocket: {
        args: [FFIType.cstring, FFIType.int],
        returns: FFIType.ptr
    },
    runServer: {
        args: [FFIType.ptr, "function"],
    },
    terminate: {
        args: [FFIType.ptr],
    },
    forceClose: {
        args: [FFIType.ptr, FFIType.int],
    },
    initClient: {
        args: [FFIType.cstring],
        returns: FFIType.ptr
    },
    runConnection: {
        args: [FFIType.ptr, "function"],
    },
    terminateClient: {
        args: [FFIType.ptr]
    },
    getConnectionFd: {
        args: [FFIType.ptr],
        returns: FFIType.int
    },
    writeToFd: {
        args: [FFIType.int, FFIType.cstring, FFIType.int],
    }
});

class UNIXfd {
    #fd;
    #sock;
    #client;
    #data;
    constructor(fd: number, sock: FFIType.ptr, data: FFIType.cstring, client = false) {
        this.#fd = fd;
        this.#sock = sock;
        this.#client = client;
        this.#data = data;
    }

    close() {
        if (!this.#client)
            symbols.forceClose(this.#sock, this.#fd);
        else
            symbols.terminateClient(this.#sock);
    }

    write(message: string) {
        // @ts-ignore not now
        symbols.writeToFd(this.#fd, Buffer.from(message + "\0"), message.length + 1);
    }

    read() {
        return new CString(this.#data);
    }

    getFd() {
        return this.#fd;
    }
}

class UNIXSockConn extends EventEmitter {
    #sock;
    #terminate = false;
    #id: any;
    #sockPath;
    #fd;

    constructor(path: string) {
        super();
        this.#sockPath = path;
        
        // @ts-ignore not now
        this.#sock = symbols.initClient(Buffer.from(path +"\0"));

        const callback = new JSCallback((fd: number, cstr: FFIType.cstring) => this.__handler(fd, cstr), {
            args: [FFIType.int, FFIType.cstring],
            threadsafe: true
        });

        this.#fd = symbols.getConnectionFd(this.#sock);

        symbols.runConnection(this.#sock, callback.ptr!);
        this.__keepMeAliveOrIllDie();
    }

    __handler(fd: number, cstr: FFIType.cstring) {
        this.emit("message", new UNIXfd(fd, this.#sock, cstr, true));
        if (this.#terminate) {
            if (this.#id != -1) {
                clearInterval(this.#id);
                symbols.terminateClient(this.#sock);
                unlinkSync(this.#sockPath);
            }
        }
    }

    send(message: string) {
        // @ts-ignore not now
        symbols.writeToFd(this.#fd, Buffer.from(message + "\0"), message.length + 1);
    }

    terminate() {
        this.#terminate = true;
    }

    __keepMeAliveOrIllDie() {
        this.#id = setInterval(() => {}, 10000);
    }
}

class UNIXSock extends EventEmitter {
    #sock;
    #terminate = false;
    #id: any;
    #sockPath;

    constructor(path: string, maxConnections: number, relink = false) {
        super();
        this.#sockPath = path;
        // @ts-ignore not now
        this.#sock = symbols.initSocket(Buffer.from(path + "\0"), maxConnections);
        if (this.#sock == null && relink) {
            unlinkSync(path);
            // @ts-ignore not now
            this.#sock = symbols.initSocket(Buffer.from(path + "\0"), maxConnections);
        } else if (this.#sock == null && !relink) {
            throw new Error("Bailing out, socket exists! Set 3rd param to true to automatically remove previous socket.");
        }

        const callback = new JSCallback((fd: number, cstr: FFIType.cstring) => this.__handler(fd, cstr), {
            args: [FFIType.int, FFIType.cstring],
            threadsafe: true
        });

        symbols.runServer(this.#sock, callback.ptr!);
        this.__keepMeAliveOrIllDie();
    }

    __handler(fd: number, cstr: FFIType.cstring) {
        this.emit("message", new UNIXfd(fd, this.#sock, cstr));
        if (this.#terminate) {
            if (this.#id != -1) {
                clearInterval(this.#id);
                symbols.terminate(this.#sock);
                unlinkSync(this.#sockPath);
            }
        }
    }

    terminate() {
        this.#terminate = true;
    }

    __keepMeAliveOrIllDie() {
        this.#id = setInterval(() => {}, 10000);
    }
}

const sock = new UNIXSock("/tmp/lol", 4024, true);

sock.on("message", (sock: UNIXfd) => {
    console.log(sock.read());

});


const client = new UNIXSockConn("/tmp/lol");

client.on("message", (sock: UNIXfd) => {
    console.log("rec");
    console.log(sock.read());
    // console.log(sock.read());
});

client.send("ping");

