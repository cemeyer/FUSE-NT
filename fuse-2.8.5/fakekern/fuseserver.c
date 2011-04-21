#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include "fuse_kernel.h"

int socketHandle;

void handleBreak(int sig) {
    shutdown(socketHandle, SHUT_RDWR);
    close(socketHandle);
}

int main(void) {
    char* path = "/dev/fuse";
    int res;
    struct sockaddr_un addr;
    size_t size;
    int status = 0;

    FUSE_REQUEST request;

    signal(SIGINT, handleBreak);

    socketHandle = socket(PF_UNIX, SOCK_DGRAM, 0);
    printf("Response to opening socket is %d\n", socketHandle);

    if(socketHandle < 0) {
        status = socketHandle;
        goto SHUTDOWN_SERVER;
    }
    
    strcpy(addr.sun_path, path);
    addr.sun_family = AF_UNIX;

    size = offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path) + 1;
    res = bind(socketHandle, (struct sockaddr*)&addr, size);
    printf("Response to binding socket to address is %d\n", res);
    printf("errno is %d\n", errno);

    if(res != 0) {
        status = res;
        goto SHUTDOWN_SERVER;
    }

    for(;;) {
        res = read(socketHandle, &request, sizeof(FUSE_REQUEST));

        printf("Response to reading is %d\n", res);

        if(res != sizeof(FUSE_REQUEST)) {
            printf("Invalid number of bytes read\n");
            status = res;
            goto SHUTDOWN_SERVER;
        }

        printf("Requested mount point is %S\n", request.MountPoint);

        // send same request back at sender
        res = send(socketHandle, &request, sizeof(FUSE_REQUEST), 0);

        printf("Sent reply with status %d\n", res);
        printf("errno is %d\n", errno);
    }

SHUTDOWN_SERVER:
    shutdown(socketHandle, SHUT_RDWR);
    close(socketHandle);
    unlink(path);

    return status;
}


