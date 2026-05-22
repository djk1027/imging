#include <stdlib.h>
#include <pthread.h>

/* fixed-size block */
#define BLOCK_TYPE_1 (1 * 1024)
#define BLOCK_TYPE_8 (8 * 1024)
#define BLOCK_TYPE_16 (16 * 1024)
#define BLOCK_TYPE_32 (32 * 1024)
#define BLOCK_TYPE_64 (64 * 1024)

 static size_t type_to_size(uint8_t type) {
    switch (type) {
        case 1:  return BLOCK_TYPE_1;
        case 8:  return BLOCK_TYPE_8;
        case 16: return BLOCK_TYPE_16;
        case 32: return BLOCK_TYPE_32;
        case 64: return BLOCK_TYPE_64;
        default: return 0;
    }
 }

typedef struct block block;
typedef struct bucket bucket;
typedef struct hashtable hashtable;

/*
    fset key0 [ 200K Data ]

    key_id + part_no --(hash)--> [Hash key]
    ------------------------------------------------
    Key0 + 0 --(hash)--> 0X12345678 (random hash)
    Key0 + 1 --(hash)--> 0X23456789 (random hash)
    Key0 + 2 --(hash)--> 0X34567890 (random hash)
    Key0 + 3 --(hash)--> 0X45678901 (random hash)
    
    B1 -> B2 -> B3 -> B4
    64 -> 64 -> 64 -> 8 

	[ Hashtable ]
	+--------------------------------+
	|  Bucket [0] - ?? - B0 - ?? ... |
	|  Bucket [1] - B1 - ??          |
	|  Bucket [2] - ??               |
	|  Bucket [3] - B3               |
	|  Bucket [4] -                  |
	|  Bucket [5] - B2 - ??          |
	|  ...                           |
	+--------------------------------+
 */

struct bucket {
    pthread_mutex_t latch;
	block *b;
};

struct hashtable {
    bucket *bk;
    size_t size;
};

struct block {
    /* key's hashing value */
	uint64_t key_id;
    /* block part serno */
    uint16_t part_no;
    /* 1 or 8 or 16 or 32 or 64 */
    uint8_t type;
    /* bit 0 - end Yn */
    uint8_t flags;
    /* next block addr */
    block *next;

    /* Flexible Array Member*/
    char data[];
};

 block *block_alloc(uint8_t type) {
    size_t payload = type_to_size(type);

    if (payload == 0) return NULL;
    
    block *b = malloc(sizeof(block) + payload);
    if (!b) return NULL;

    b->type = type;
    b->part_no = 0;
    b->flags = 0;
    b->next = NULL;

    return b;
 }