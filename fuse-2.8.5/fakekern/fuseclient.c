#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <wchar.h>
#include "fuse_kernel.h"

int main(void) {
    char* path = "/dev/fuse";
    int fd, res;
    struct sockaddr_un addr;
    size_t size;
    int status = 0;

    FUSE_REQUEST request;
    wcscpy(request.MountPoint, L"hw");
    request.MountPointLength = wcslen(request.MountPoint);

    fd = socket(PF_UNIX, SOCK_DGRAM, 0);
    printf("Response to opening socket is %d\n", fd);

    if(fd < 0) {
        status = fd;
        goto SHUTDOWN_CLIENT;
    }
    
    strcpy(addr.sun_path, path);
    addr.sun_family = AF_UNIX;

    size = offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path) + 1;
    res = connect(fd, (struct sockaddr*)&addr, size);
    printf("Response to connecting to %s is %d\n", path, res);
    printf("errno is %d\n", errno);

    if(res != 0) {
        status = res;
        goto SHUTDOWN_CLIENT;
    }

    printf("Sending request\n");

    // send request to "kernel"
    send(fd, &request, sizeof(FUSE_REQUEST), 0);

    printf("Reading back response\n");

    // read response from "kernel"
    read(fd, &request, sizeof(FUSE_REQUEST));

SHUTDOWN_CLIENT:
    shutdown(fd, SHUT_RDWR);
    close(fd);

    return fd;
}


