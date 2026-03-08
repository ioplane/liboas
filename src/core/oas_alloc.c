#include <liboas/oas_alloc.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct oas_arena_block {
    struct oas_arena_block *next;
    size_t capacity;
    size_t used;
    alignas(max_align_t) uint8_t data[];
} oas_arena_block_t;

struct oas_arena {
    oas_arena_block_t *current;
    oas_arena_block_t *head;
    size_t block_size;
    size_t total_allocated;
};

static oas_arena_block_t *oas_arena_block_create(size_t capacity)
{
    oas_arena_block_t *block = malloc(sizeof(*block) + capacity);
    if (!block) {
        return nullptr;
    }
    block->next = nullptr;
    block->capacity = capacity;
    block->used = 0;
    return block;
}

oas_arena_t *oas_arena_create(size_t block_size)
{
    if (block_size == 0) {
        block_size = OAS_ARENA_DEFAULT_BLOCK;
    }

    oas_arena_t *arena = malloc(sizeof(*arena));
    if (!arena) {
        return nullptr;
    }

    oas_arena_block_t *block = oas_arena_block_create(block_size);
    if (!block) {
        free(arena);
        return nullptr;
    }

    arena->current = block;
    arena->head = block;
    arena->block_size = block_size;
    arena->total_allocated = 0;
    return arena;
}

void oas_arena_destroy(oas_arena_t *arena)
{
    if (!arena) {
        return;
    }

    oas_arena_block_t *block = arena->head;
    while (block) {
        oas_arena_block_t *next = block->next;
        free(block);
        block = next;
    }
    free(arena);
}

static size_t align_up(size_t offset, size_t align)
{
    return (offset + align - 1) & ~(align - 1);
}

void *oas_arena_alloc(oas_arena_t *arena, size_t size, size_t align)
{
    if (!arena || size == 0 || align == 0) {
        return nullptr;
    }

    /* Ensure alignment is power of two */
    if ((align & (align - 1)) != 0) {
        return nullptr;
    }

    oas_arena_block_t *block = arena->current;
    size_t aligned_offset = align_up(block->used, align);

    if (aligned_offset + size <= block->capacity) {
        void *ptr = &block->data[aligned_offset];
        block->used = aligned_offset + size;
        arena->total_allocated += size;
        return ptr;
    }

    /* Need a new block — size may exceed default block_size for large objects */
    size_t new_capacity = arena->block_size;
    if (size + align > new_capacity) {
        new_capacity = size + align;
    }

    oas_arena_block_t *new_block = oas_arena_block_create(new_capacity);
    if (!new_block) {
        return nullptr;
    }

    /* Link new block after current */
    new_block->next = block->next;
    block->next = new_block;
    arena->current = new_block;

    aligned_offset = align_up(new_block->used, align);
    void *ptr = &new_block->data[aligned_offset];
    new_block->used = aligned_offset + size;
    arena->total_allocated += size;
    return ptr;
}

void oas_arena_reset(oas_arena_t *arena)
{
    if (!arena) {
        return;
    }

    oas_arena_block_t *block = arena->head;
    while (block) {
        block->used = 0;
        block = block->next;
    }
    arena->current = arena->head;
    arena->total_allocated = 0;
}

size_t oas_arena_used(const oas_arena_t *arena)
{
    if (!arena) {
        return 0;
    }
    return arena->total_allocated;
}
