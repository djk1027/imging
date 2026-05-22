#define STB_IMAGE_IMPLEMENTATION
#include "../include/stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "imging.h"
#include "thread.h"
#include "block.h"
#include "siphash.h"

#define MAX_EVENTS 10
#define PORT 1027
#define BUFFER_SIZE 4096

int epollfd = -1;

/* Socket non-blocking setup function */
int setNonBlocking(int fd) {
    int flags = fcntl(fd ,F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void handleClientIo(void *arg) {
    ClientContext *ctx = (ClientContext *)arg;
    int fd = ctx->client_fd;

    char buf[BUFFER_SIZE];
    int done = 0;

    while (1) {
		/* Since it is Edge-Triggered, the entire socket buffer must be
		   retrieved until EAGAIN appears. */
        ssize_t count = read(fd, buf, sizeof(buf));

		
        if (count == -1) {
			/* No more data available in the kernel buffer;
			   drop out of the loop and wait for the next epoll event */
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            perror("read error");
            done = 1;
            break;
        } else if (count == 0) {
		/* Client terminates connection */
            done = 1;
            break;
        }

		// printf("[Thread %ld] Recv from fd %d, count %ld: %s\n", (long)pthread_self(), fd, count, buf);
		// write(fd, buf, count);
    }

	char *msg = "OK";
	write(fd, msg, strlen(msg));

    if (done) {
		epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
        close(fd);
        free(ctx);
    } else {
		/* Since the task is finished, reactivate the epoll event
		   for this socket (effectively release EPOLLONESHOT) */
        struct epoll_event ev;
		ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
		ev.data.ptr = ctx;

		epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
    }
}

/*---------------------
 * Main
 *---------------------*/
int main(int argc, char *argv[]) {
	int listen_sock, conn_sock;
	struct sockaddr_in addr;
	struct epoll_event ev, events[MAX_EVENTS];

	/* Initialize the thread pool, worker threads */
	iThreadPoolInit(4);
	
	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    
	addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    
	bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr));
    listen(listen_sock, 5);

	setNonBlocking(listen_sock);

	/* Create epoll instance */
	epollfd = epoll_create1(0);

	ServerContext *stx = (ServerContext *)malloc(sizeof(ServerContext));
	stx->server_fd = listen_sock;
	
	/* Read detection | Edge trigger processing */
	ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = stx;

	epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev);

	/* Main thread's event loop */
	while (1) {
		int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);

		for (int n = 0; n < nfds; ++n) {
			if (events[n].data.ptr == stx) {
			/* New Connections : The main thread directly performs accept. */
				while (1) {
					conn_sock = accept(listen_sock, NULL, NULL);
					
					if (conn_sock == -1) {
						/* ET mode: drain the accept queue until EAGAIN or EWOULDBLOCK */
						if (errno == EINTR || errno == EWOULDBLOCK) break;

						perror("accept() error");
						break;
					}

					setNonBlocking(conn_sock);
					
					/* Client I/O: hand over to worker thread */
					ClientContext *ctx = (ClientContext *)malloc(sizeof(ClientContext));
					ctx->client_fd = conn_sock;
					
					/* ONESHOT: only one thread handles this fd at a time */
					ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
					ev.data.ptr = ctx;
					
					epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev);
				}
			} else {
				ClientContext *ctx = (ClientContext *)events[n].data.ptr;

				iThreadPoolAdd(handleClientIo, ctx);
			}
		}
	}

	return 0;
}
