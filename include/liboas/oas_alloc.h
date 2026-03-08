/**
 * @file oas_alloc.h
 * @brief Arena allocator for document model lifetime.
 *
 * All OAS model nodes are allocated from a single arena and freed together
 * in O(1) when the document is destroyed. Blocks grow as needed.
 */

#ifndef LIBOAS_OAS_ALLOC_H
#define LIBOAS_OAS_ALLOC_H

#include <stddef.h>

/** Default arena block size: 64 KiB */
constexpr size_t OAS_ARENA_DEFAULT_BLOCK = 64 * 1024;

typedef struct oas_arena oas_arena_t;

/**
 * @brief Create a new arena allocator.
 * @param block_size Size of each memory block (0 = default 64 KiB).
 * @return Arena pointer, or nullptr on allocation failure.
 */
[[nodiscard]] oas_arena_t *oas_arena_create(size_t block_size);

/**
 * @brief Destroy arena and free all blocks.
 * @param arena Arena to destroy (nullptr-safe).
 */
void oas_arena_destroy(oas_arena_t *arena);

/**
 * @brief Allocate memory from the arena.
 * @param arena Arena allocator.
 * @param size  Number of bytes to allocate.
 * @param align Alignment requirement (must be power of two).
 * @return Aligned pointer, or nullptr on failure or zero size.
 */
[[nodiscard]] void *oas_arena_alloc(oas_arena_t *arena, size_t size, size_t align);

/**
 * @brief Reset arena for reuse without freeing blocks.
 * @param arena Arena to reset.
 */
void oas_arena_reset(oas_arena_t *arena);

/**
 * @brief Get total bytes used across all blocks.
 * @param arena Arena to query.
 * @return Total bytes currently allocated.
 */
size_t oas_arena_used(const oas_arena_t *arena);

#endif /* LIBOAS_OAS_ALLOC_H */
