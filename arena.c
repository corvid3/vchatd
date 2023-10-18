#include "arena.h"
#include <stdlib.h>

#define DEFAULT_CAP_SIZE 64
#define GROWTH_RATE 2

inline static void arena_ensure_cap(struct vcd_arena *arena, size_t newsize) {
  if (newsize > arena->cap) {
    arena->cap *= GROWTH_RATE;
    arena->allocs = realloc(arena->allocs, GROWTH_RATE * sizeof(void *));
  }
}

extern struct vcd_arena arena_init(void) {
  struct vcd_arena arena;
  arena.cap = DEFAULT_CAP_SIZE;
  arena.num = 0;
  arena.allocs = malloc(sizeof(void *) * DEFAULT_CAP_SIZE);
  return arena;
}

extern void *arena_alloc(struct vcd_arena *arena, size_t size) {
  arena_ensure_cap(arena, arena->num += 1);
  void *data = malloc(size);
  arena->allocs[arena->num - 1] = data;
  return data;
}

extern void arena_free(struct vcd_arena *arena) {
  for (size_t i = 0; i < arena->num; i++)
    free(arena->allocs[i]);
  free(arena->allocs);
}
