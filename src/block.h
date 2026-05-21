#include <stdlib.h>

/* 64K fixed-size block */
#define BLOCK_SIZE (64 * 1024)

typedef struct {
	unsigned long block_id;
	size_t data_size;
	char data[BLOCK_SIZE];
} Block;