#define STB_IMAGE_IMPLEMENTATION
#include "../include/stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

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

int main(int argc, char *argv[]) {

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

	return 0;
}
