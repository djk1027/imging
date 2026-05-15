#define STB_IMAGE_IMPLEMENTATION
#include "../include/stb_image.h"
#include <stdio.h>

int width, height, channels;
unsigned char *img;

void print_img(unsigned char *img) {

	for (int y = 0; y < height;  y += 4) {
		for (int x = 0; x < width; x += 2) {
			int idx = (y * width + x) * 3;
			int avg = (img[idx] + img[idx+1] + img[idx+2]) / 3;
			printf("%c", avg > 128 ? '#' : ' ');
		}

		printf("\n");
	}

	printf("%d %d %d\n", width, height, channels); 
}

void print_img2(unsigned char *img) {
	
	for (int y = 0; y < height;  y += 2) {
		for (int x = 0; x < width; x ++) {
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

	img = stbi_load(argv[1] , &width, &height, &channels, 3);
	
	if (img == NULL) {
		printf("Image Load Fail: %s\n", argv[1]);
		exit(1);
	}

	print_img(img);

	stbi_image_free(img);
	return 0;
}
