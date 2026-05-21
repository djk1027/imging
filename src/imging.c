#define STB_IMAGE_IMPLEMENTATION
#include "../include/stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "thread.h"
#include "block.h"

#define MAX_EVENTS 10
#define PORT 1027
#define BUFFER_SIZE 4096

int epollfd = -1;

void handleClientIo(void *arg) {
    ClientContext *ctx = (ClientContext *)arg;
    int fd = ctx->client_fd;

    char buf[BUFFER_SIZE];
    int done = 0;

    while (1) {
        ssize_t count = read(fd, buf, sizeof(buf));

        if (count == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            perror("read error");
            done = 1;
            break;
        } else if (count == 0) {
            done = 1;
            break;
        }

		printf("[Thread %ld] Recv from fd %d: %s\n", (long)pthread_self(), fd, buf);
		write(fd, buf, count);
    }

    if (done) {
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
	
	/* Read detection | Edge trigger processing */
	ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listen_sock;

	epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev);

	/* Main thread's event loop */
	while (1) {
		int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);

		for (int n = 0; n < nfds; ++n) {
			if (events[n].data.fd == listen_sock) {
			/* New Connections : The main thread directly performs accept. */
				while ((conn_sock = accept(listen_sock, NULL, NULL)) != -1) {
				/* ET mode: drain the accept queue until EAGAIN */
					setNonBlocking(conn_sock);
					
					/* ONESHOT: only one thread handles this fd at a time */
					ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
					ev.data.fd = conn_sock;
					epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev);
				}
			} else {
			/* Client I/O: hand over to worker thread */
				ClientContext *ctx = (ClientContext *)malloc(sizeof(ClientContext));
				ctx->client_fd = events[n].data.fd;
				
				iThreadPoolAdd(handleClientIo, ctx);
			}
		}
	}

	return 0;
}
