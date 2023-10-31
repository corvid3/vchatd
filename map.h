#pragma once

/*

map.h
simple hash map, utility for communication
		between clients and the server

serialization protocol:
	u4: number of correlations

*/

struct entry {
	char* key;
	int hash;

	char* data;
	int data_len;
};

struct map {
	struct entry* entries;
	int num_entries;
};

extern struct map map_init(void);

// moves data & key
#define MAP_SET(map, key, data) map_push_bytes(map, &data, key, sizeof data)
#define MAP_GET(map, key, type) ((type*)map_get_bytes(map, key))

// moves data & key
extern void map_push_data(struct map* map, void* data, char* key, const int data_len);
extern void* map_get_bytes(struct map* map, const char* key);

extern void map_free(struct map* map);


// converts a map into a stream of bytes
extern char* map_encode(struct map* map);

extern struct map map_decode(const char* data, const int data_len);
