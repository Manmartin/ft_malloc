#include <stdbool.h>  /* for bool */
#include <stddef.h>   /* for size_t and NULL */
#include <sys/mman.h> /* for mmap and munmap */
#include <unistd.h>   /* for write */

#include <stdio.h>

#define ALIGNMENT 16
#define ALIGN(x, alignment) ((x) + (alignment)-1) / (alignment) * (alignment)

#define TINY_ALLOC_NUMBER 100
#define TINY_ALLOC_SIZE 64

#define SMALL_ALLOC_NUMBER 100
#define SMALL_ALLOC_SIZE 256

typedef enum
{
    TINY,
    SMALL,
    LARGE,
} AllocType;

typedef struct alloc_s
{
    bool      free : 1;
    AllocType type : 2;

    size_t          size;
    struct alloc_s *past;
    struct alloc_s *next;
    char            data[];
} alloc_t;

typedef struct
{
    alloc_t *tiny_allocs;
    alloc_t *small_allocs;
} malloc_header_t;

static malloc_header_t header;

void *malloc(size_t size)
{
    // If is the first call Initialize memory mappings
    if (header.tiny_allocs == NULL)
    {

        size_t page_size = sysconf(_SC_PAGESIZE);

        // Calculate minimum page number for every zone
        size_t tiny_zone_size =
            ALIGN(TINY_ALLOC_NUMBER * (TINY_ALLOC_SIZE + sizeof(alloc_t)) + sizeof(alloc_t), page_size);
        size_t small_zone_size =
            ALIGN(SMALL_ALLOC_NUMBER * (SMALL_ALLOC_SIZE + sizeof(alloc_t)) + sizeof(alloc_t), page_size);

        header.tiny_allocs  = mmap(NULL, tiny_zone_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        header.small_allocs = mmap(NULL, small_zone_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (header.tiny_allocs == NULL || header.small_allocs == NULL)
        {
            if (header.tiny_allocs != NULL)
                munmap(header.tiny_allocs, tiny_zone_size);
            if (header.small_allocs != NULL)
                munmap(header.small_allocs, small_zone_size);
            return NULL;
        }

        header.tiny_allocs->free = true;
        header.tiny_allocs->type = TINY;
        header.tiny_allocs->past = NULL;
        header.tiny_allocs->next = NULL;
        header.tiny_allocs->size = tiny_zone_size - sizeof(alloc_t);

        header.small_allocs->free = true;
        header.small_allocs->type = SMALL;
        header.small_allocs->past = NULL;
        header.small_allocs->next = NULL;
        header.small_allocs->size = small_zone_size - sizeof(alloc_t);
    }

    alloc_t *alloc;
    if (size <= TINY_ALLOC_SIZE)
        alloc = header.tiny_allocs;
    else if (size <= SMALL_ALLOC_SIZE)
        alloc = header.small_allocs;
    else
        return NULL;

    size_t aligned_size = ALIGN(size, ALIGNMENT);

    while ((!alloc->free || (alloc->size != aligned_size && alloc->size < aligned_size + sizeof(alloc_t))) &&
           alloc->next != NULL)
        alloc = alloc->next;

    alloc_t *next_alloc;
    if (alloc->next == NULL)
    {
        // End of allocated memory. TODO: Request more memory for the new allocation;
        if (alloc->size < aligned_size + sizeof(alloc_t))
            return NULL;

        next_alloc       = (alloc_t *)((char *)alloc + sizeof(alloc_t) + aligned_size);
        next_alloc->past = alloc;
        next_alloc->next = NULL;

        next_alloc->size = alloc->size - aligned_size - sizeof(alloc_t);
        next_alloc->free = true;
        next_alloc->type = alloc->type;

        alloc->next = next_alloc;
    }
    else if (alloc->size >= aligned_size + sizeof(alloc_t))
    {
        next_alloc       = (alloc_t *)((char *)alloc + sizeof(alloc_t) + aligned_size);
        next_alloc->past = alloc;
        next_alloc->next = alloc->next;

        next_alloc->size = alloc->size - aligned_size - sizeof(alloc_t);
        next_alloc->free = true;
        next_alloc->type = alloc->type;

        alloc->next = next_alloc;
    }
    alloc->size = size;
    alloc->free = false;

    return alloc->data;
    return NULL;
}

void *realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
        return malloc(size);
    return NULL;
}

void free(void *ptr)
{
    if (ptr == NULL)
        return;

    alloc_t *alloc = ((alloc_t *)ptr - 1);
    alloc->free    = true;

    // Get real size of free block
    alloc->size = ALIGN(alloc->size, ALIGNMENT);

    if (alloc->past != NULL && alloc->past->free)
    {
        alloc->past->next = alloc->next;
        alloc->next->past = alloc->past;

        alloc->past->size += alloc->size + sizeof(alloc_t);

        alloc = alloc->past;
    }
    if (alloc->next != NULL && alloc->next->free)
    {
        alloc->size += alloc->next->size + sizeof(alloc_t);
        alloc->next = alloc->next->next;
        if (alloc->next != NULL)
            alloc->next->past = alloc;
    }
}

// TODO : Remove printf
void show_alloc_mem(void)
{
    size_t total_size = 0;

    alloc_t *tiny_allocs = header.tiny_allocs;
    size_t   base_dir    = (size_t)tiny_allocs;

    if (tiny_allocs->next != NULL)
        printf("TINY:\n");
    while (tiny_allocs->next != NULL)
    {
        char *position = (char *)tiny_allocs;
        if (!tiny_allocs->free)
        {
            printf("%-5p -  %-5p : %li bytes\n", position - base_dir + sizeof(alloc_t),
                   position - base_dir + sizeof(alloc_t) + tiny_allocs->size, tiny_allocs->size);
            total_size += tiny_allocs->size;
        }
        tiny_allocs = tiny_allocs->next;
    }

    alloc_t *small_allocs = header.small_allocs;
    base_dir              = (size_t)small_allocs;
    if (tiny_allocs->next != NULL)
        printf("SMALL:\n");
    while (small_allocs->next != NULL)
    {
        char *position = (char *)small_allocs;
        if (!small_allocs->free)
        {
            printf("%-5p -  %-5p : %li bytes\n", position - base_dir + sizeof(alloc_t),
                   position - base_dir + sizeof(alloc_t) + small_allocs->size, small_allocs->size);
            total_size += small_allocs->size;
        }
        small_allocs = small_allocs->next;
    }

    printf("Total : %li bytes\n", total_size);
}
