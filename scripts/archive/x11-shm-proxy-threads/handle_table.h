#ifndef HANDLE_TABLE_H
#define HANDLE_TABLE_H

#include <stdint.h>
#include <stddef.h>

/* Server-owned handle table API.
   thread-safe. */
void ht_init(void);
uint64_t ht_store_ptr(void *ptr);    /* return non-zero handle */
void *ht_lookup_ptr(uint64_t handle);
void *ht_remove_ptr(uint64_t handle);

#endif /* HANDLE_TABLE_H */
