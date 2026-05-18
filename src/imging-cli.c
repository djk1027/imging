#define STB_IMAGE_IMPLEMENTATION
#include "../include/stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define C_LINE_MAX 4096

char *cliLine(const char *prompt) {
    char buf[C_LINE_MAX];
    size_t len;

    printf("%s", prompt);
    fflush(stdout);
    
    if (fgets(buf, C_LINE_MAX, stdin) == NULL) return NULL;

    len = strlen(buf);

    while (len && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        len--;
        buf[len] = '\0';
    }

    return strdup(buf);
}

/*---------------------
 * Main
 *---------------------*/
int main(int argc, char *argv[]) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    char *hostip = "127.0.0.1";

    char *line;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n");
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(1027);

    if (inet_pton(AF_INET, hostip, &serv_addr.sin_addr) <= 0) {
        printf("Invalid address / Address not supported\n");
        exit(1);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed\n");
        exit(1);
    }

    /* Create cli prompt */
    char prompt[128];
    strncpy(prompt, hostip, sizeof(prompt) - 1);
    prompt[sizeof(prompt) - 1] = '\0';
    strcat(prompt, "> ");

    /* Currently, it is the main loop that handles send and read together. */
    while (1) {
        line = cliLine(prompt);

        if (strcmp(line, "exit") == 0) {
            exit(0);
        }
        
        send(sock, line, strlen(line), 0);
        read(sock, buffer, 1024);
        printf("Message from server: %s\n", buffer);
    }

    close(sock);
    return 0;
}