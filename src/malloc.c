#include <stdbool.h>  /* for bool */
#include <stddef.h>   /* for size_t and NULL */
#include <sys/mman.h> /* for mmap and munmap */
#include <unistd.h>   /* for write */

#define TINY_ALLOC_NUMBER 100
#define TINY_ALLOC_SIZE 64

typedef enum
{
    TINY,
    SMALL,
    LARGE,
} AllocTypes;

typedef struct
{
    bool       free : 1;
    bool       end : 1;
    AllocTypes type : 2;

    size_t size;
    char   data[];
} alloc_t;

typedef struct
{
    alloc_t *tiny_allocs;
} malloc_header_t;

static malloc_header_t header;

void *malloc(size_t size)
{
    // If is the first call Initialize memory mappings
    if (header.tiny_allocs == NULL)
    {

        size_t page_size = sysconf(_SC_PAGESIZE);

        // Calculate minimum page number for tiny zone
        size_t tiny_zone_size =
            (TINY_ALLOC_NUMBER * (TINY_ALLOC_SIZE + sizeof(alloc_t)) + sizeof(alloc_t) + page_size - 1) / page_size *
            page_size;

        header.tiny_allocs = mmap(NULL, tiny_zone_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (header.tiny_allocs == NULL)
            return NULL;

        header.tiny_allocs->free = true;
        header.tiny_allocs->end  = true;
        header.tiny_allocs->type = TINY;
        header.tiny_allocs->size = tiny_zone_size - sizeof(alloc_t);
    }

    if (size <= TINY_ALLOC_SIZE)
    {
        char *position = (char *)header.tiny_allocs;
        while ((!((alloc_t *)position)->free || ((alloc_t *)position)->size < TINY_ALLOC_SIZE) &&
               !((alloc_t *)position)->end)
            position += sizeof(alloc_t) + TINY_ALLOC_SIZE;

        alloc_t *current_alloc = (alloc_t *)position;
        // End of allocated memory. TODO: Request more memory for the new allocation;
        if (current_alloc->end && current_alloc->size < TINY_ALLOC_SIZE + sizeof(alloc_t))
            return NULL;

        alloc_t *next_alloc = (alloc_t *)(position + sizeof(alloc_t) + TINY_ALLOC_SIZE);
        next_alloc->size    = current_alloc->size - TINY_ALLOC_SIZE - sizeof(alloc_t);
        next_alloc->free    = true;
        next_alloc->end     = current_alloc->end ? true : false;

        current_alloc->size = size;
        current_alloc->free = false;
        current_alloc->end  = false;
        return current_alloc->data;
    }
    return NULL;
}

void *realloc(void *ptr, size_t size)
{
    // TODO: act like malloc when ptr == NULL
    if (ptr == NULL)
        return NULL;
    alloc_t *alloc = (alloc_t *)ptr - 1;
    if (alloc->type == TINY)
    {
        if (size > TINY_ALLOC_SIZE)
            return NULL;
        alloc->size = size;
    }
    else
        return NULL;
    return alloc->data;
}

void free(void *ptr)
{
    alloc_t *block = ((alloc_t *)ptr - 1);
    block->free    = true;
}

// TODO : Remove printf
#include <stdio.h>
void show_alloc_mem(void)
{
    size_t total_size = 0;
    printf("TINY:\n");
    alloc_t *tiny_allocs = header.tiny_allocs;
    size_t   base_dir    = (size_t)tiny_allocs;
    while (!tiny_allocs->end)
    {
        char *position = (char *)tiny_allocs;
        if (!tiny_allocs->free)
        {
            printf("%-5p -  %-5p : %li bytes\n", position - base_dir + sizeof(alloc_t),
                   position - base_dir + sizeof(alloc_t) + tiny_allocs->size, tiny_allocs->size);
            total_size += tiny_allocs->size;
        }
        tiny_allocs = (alloc_t *)(position + sizeof(alloc_t) + TINY_ALLOC_SIZE);
    }
    printf("Total : %li bytes\n", total_size);
}
