import { FFIType, dlopen, ptr, CString, JSCallback } from "bun:ffi";
import { closeSync, fstatSync, readSync, statSync } from "fs";

const { symbols } = dlopen("libsockets.so", {
    initSocket: {
        args: [FFIType.cstring],
        returns: FFIType.ptr
    },
    runServer: {
        args: [FFIType.ptr, "function"],
        returns: FFIType.void
    },
    terminate: {
        args: [FFIType.ptr],
        returns: FFIType.void
    }
});

const path = Buffer.from("/tmp/xd\0");

// @ts-ignore stfu
const data = symbols.initSocket(path);

const callback = new JSCallback((fd: number) => {
    const buff = Buffer.alloc(1024);

    const _ = readSync(fd, buff);
    console.log(buff.toString("utf8"));

    closeSync(fd);
}, {
    args: [FFIType.int],
    threadsafe: true
});

symbols.runServer(data, callback.ptr!);

setInterval(() => {
    symbols.terminate(data);
}, 1000000);
