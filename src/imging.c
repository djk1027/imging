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

#define MAX_EVENTS 10
#define PORT 1027
#define BUFFER_SIZE 4096

int epollfd = -1;

int width, height, channels;
unsigned char *img;

long cols = 100;

void print_img(unsigned char *img, int **board) {
	int dy = height / (cols / 2);
	int dx = width / cols;

	for (int y = 0; y < dy * (cols / 2); y++) {
		for (int x = 0; x < dx * cols; x++) {
			int idx = (y * width + x) * 3;
			int avg = (img[idx] + img[idx+1] + img[idx+2]) / 3;

			if (avg > 128) {
				board[y/dy][x/dx]++;
			}
		}
	}

	int full = dy * dx;
	int sat;
	char ch;

	for (int i = 0; i < cols / 2; i++) {
		for (int j = 0; j < cols; j++) {
			sat = (board[i][j] * 100) / full;

			if (sat >= 80)      ch = '@'; // @
			else if (sat >= 60) ch = '#'; // #
			else if (sat >= 40) ch = '^'; // ^
			else if (sat >= 20) ch = '*'; // *
			else if (sat >= 10) ch = '.'; // .
			else ch = ' ';
			
			printf("%c", ch);
		}

		printf("\n");
	}

	printf("%d %d %d\n", width, height, channels); 
}

void print_img2(unsigned char *img) {
	
	for (int y = 0; y < height;  y += 2) {
		for (int x = 0; x < width; x++) {
			int idx = (y * width + x) * 3;
			printf("\x1b[48;2;%d;%d;%dm  ", img[idx], img[idx + 1], img[idx + 2]);
		}

		printf("\x1b[0m\n");
	}
}

void handleClientIo(void *arg) {
    ClientContext *ctx = (ClientContext *)arg;
    int fd = ctx->client_fd;
    char buf[BUFFER_SIZE];
    int done = 0;

    while (1) {
        ssize_t count = read(fd, buf, sizeof(buf) - 1);

        if (count == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            perror("read error");
            done = 1;
            break;
        } else if (count == 0) {
            done = 1;
            break;
        }

        buf[count] = '\0';
		printf("[Thread %ld] Recv from fd %d: %s", (long)pthread_self(), fd, buf);
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
		ev.data.fd = fd;
		epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
		free(ctx);
    }
}

/*---------------------
 * Main
 *---------------------*/
int main(int argc, char *argv[]) {
	int listen_sock, conn_sock;
	struct sockaddr_in addr;
	struct epoll_event ev, events[MAX_EVENTS];

	/* Worker thread initialization */
	iThreadPoolInit(4);
	
	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    
	addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    
	bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr));
    listen(listen_sock, 5);

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

	/*
	if (argc < 2) {
		printf("Usage: %s <your_img_path>\n", argv[0]);
		exit(1);
	}

	if (argc >= 3) {
		char *endptr;
		errno = 0;

		cols = strtol(argv[2], &endptr, 10);

		if (endptr == argv[2] || *endptr != '\0') {
			printf("Error : Not a correct number %s\n", argv[2]);
			exit(1);
		}

		if (errno == ERANGE || cols > INT_MAX || cols < INT_MIN) {
			printf("Error : Not an integer %ld\n", cols);
			exit(1);
		}
	}

	img = stbi_load(argv[1] , &width, &height, &channels, 3);
	
	if (img == NULL) {
		printf("Image Load Fail: %s\n", argv[1]);
		exit(1);
	}

	int **board = (int **)malloc(sizeof(int *) * cols / 2);

	for (int i = 0; i < cols / 2; i++) {
		board[i] = (int *)calloc(cols, sizeof(int));
	}

	print_img(img, board);

	stbi_image_free(img);
	*/

	return 0;
}
