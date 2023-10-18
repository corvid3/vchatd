#pragma once

#include <stddef.h>

struct vcd_arena {
  size_t cap;
  size_t num;
  void **allocs;
};

extern struct vcd_arena arena_init(void);
extern void* arena_alloc(struct vcd_arena*, size_t);
extern void arena_free(struct vcd_arena*);

