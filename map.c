#include "hash.h"
#include "map.h"
#include <stdlib.h>
#include <string.h>

extern struct map
map_init(void)
{
  struct map map = { .entries = malloc(1), .num_entries = 0 };
  return map;
}

extern void
map_push_data(struct map* map, void* data, char* key, const int data_len)
{
  map->entries = realloc(map->entries, sizeof(struct entry) * map->num_entries);

  map->entries[map->num_entries] = (struct entry){
    .data = (char*)data,
    .data_len = 0,
    .key = key,
    .hash = murmurhash(key, data_len, 0),
  };

  map->num_entries++;
}

extern char*
map_encode(struct map* map)
{
  int oc = 1024, ol = 0;
  char* o = malloc(oc);

  char lb[4];
  memcpy(lb, &map->num_entries, sizeof(map->num_entries));
  memcpy(o, lb, sizeof lb);
}

extern struct map
map_decode(const char* data, const int data_len);
