#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>

#define C_LINE_MAX 4096

typedef struct clientSocket {
    int sock;
    char hostip[20];
    int port;
    struct sockaddr_in server_addr;
} clientSocket;

static clientSocket cliSock;

/* Receives command-line input and returns the allocated buffer.
   called by startCommand() */
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

/* Splits the string based on the space symbol. */
char **splitArgs(const char *line, int *argc) {
    const char *p = line;
    
    char *arg = NULL;
    char **argv = NULL;
    *argc = 0;

    while (1) {
        while (*p && isspace((int)*p))
            p++;
        
        if (*p) {
            int done = 0;
            arg = calloc(256, sizeof(char));
            
            while (!done) {
                switch (*p) {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                case '\0':
                    done = 1;
                    break;
                default:
                    strncat(arg, p, 1);
                    p++;
                }
            }

            char **new_argv = realloc(argv, ((*argc) + 1) * sizeof(char *));

            if (new_argv == NULL) {
                while (*argc) {
                    free(argv[--(*argc)]);
                }
                return NULL;
            }

            argv = new_argv;
            argv[*argc] = arg;
            
            (*argc)++;
            arg = NULL;
        }
        else {
            return argv;
        }
    }
}

/* Initializes the client socket structure and establishes a connection to the server. */
void cliConnect() {
    cliSock.sock = 0;

    char *ip = "127.0.0.1";
    strcpy(cliSock.hostip, ip);

    if ((cliSock.sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n");
        exit(1);
    }

    struct sockaddr_in *serv_addr = &cliSock.server_addr;
    cliSock.port = 1027;
    
    memset(serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr->sin_family = AF_INET;
    serv_addr->sin_port = htons(cliSock.port);

    if (inet_pton(AF_INET, cliSock.hostip, &serv_addr->sin_addr) <= 0) {
        printf("Invalid address / Address not supported\n");
        exit(1);
    }

    if (connect(cliSock.sock, (struct sockaddr *)serv_addr, sizeof(*serv_addr)) < 0) {
        perror("Connection Failed");
        exit(1);
    }
}

/* Verify file path consistency and return file size. */
int fileCheck(char *path) {
    struct stat st;
    
    if(stat(path, &st) < 0) {
        if (errno == ENOENT) {
            fprintf(stderr, "Client Error: File not found. (Path: %s)\n", path);
        } else if (errno == EACCES) {
            fprintf(stderr, "Client Error: Permission denied (Path: %s)\n", path);
        } else {
            fprintf(stderr, "Client Error: stat() Failed (Reason: %s)\n", strerror(errno));
        }
        return -1;
    }

    if (!S_ISREG(st.st_mode)) {
        if (S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Client Error: '%s' is a directory, not a file.\n", path);
        } else {
            fprintf(stderr, "Client Error: '%s' is not a regular file.\n", path);
        }
        return -1;
    }

    return st.st_size;
}

/* The fset command is checked in advance and the command is sent to the server. 
   1. 
   2. */
int commandFileSet(int argc, char ***pArg) {
    char **argv = *pArg;

    /* check the consistency of the fset command
       before sending it to the server. */
    if (argc != 3) {
        fprintf(stderr, "Client Error: Wrong number of arguments (fset [KEY] [FILE])");
        return -1;     
    }

    /* Check the file */
    long flen = fileCheck(argv[2]);

    if (flen == -1) return -1;
    
    /* Assemble the header to be sent to the server. 
       : fset [key] [key_len]*/
    char header[1024];
    snprintf(header, sizeof(header), "%s %s %ld\n", argv[0], argv[1], flen);

    /* 1. Header transmission */
    send(cliSock.sock, header, strlen(header), 0);

    /* Opens the file at the input path and performs error checking. */
    int fd = open(argv[2], O_RDONLY);

    if (fd < 0) {
        perror("Client Error: Failed to open file");
        fprintf(stderr, "(file name : %s)\n", argv[2]);
        return -1;
    }

    /* 2. File transmission */
    off_t offset = 0;
    ssize_t send_bytes = sendfile(cliSock.sock, fd, &offset, flen);
    
    if (send_bytes < 0) {
        perror("Client Error: Failed to send file");
        fprintf(stderr, "(file name : %s)\n", argv[2]);
        return -1;
    }

    close(fd);
    return 0;
}

/* Process commands */
void startCommand() {
    char buffer[1024] = {0};
    char *line;

    int argc;
    char **argv;

    /* Create cli prompt */
    char prompt[128];
    strncpy(prompt, cliSock.hostip, sizeof(prompt) - 1);
    prompt[sizeof(prompt) - 1] = '\0';
    strcat(prompt, "> ");

    /* Currently, it is the main loop that handles send and read together. */
    while (1) {
        argc = 0;

        line = cliLine(prompt);
        argv = splitArgs(line, &argc);

        if (line == NULL) {
            continue;
        } else if (line[0] != '\0') {

            if (strcmp(argv[0], "fset") == 0) {
                if (commandFileSet(argc, &argv) == -1) continue;
            }

            send(cliSock.sock, line, strlen(line), 0);
            read(cliSock.sock, buffer, 1024);
            printf("Message from server: %s\n", buffer);

            memset(buffer, 0, sizeof(buffer));

            if (strcmp(line, "exit") == 0) {
                exit(0);
            }

            /* Free the result returned by splitArgs */
            if (argv) {
                while (argc) {
                    free(argv[--argc]);
                }
            }
        }
    }
}

/*---------------------
 * Main
 *---------------------*/
int main(int argc, char *argv[]) {
    /* Connect to the Server */
    cliConnect();

    /* start command loop */
    startCommand();

    close(cliSock.sock);
    exit(0);
}