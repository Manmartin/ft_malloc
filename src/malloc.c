#include <stdbool.h>  /* for bool */
#include <stddef.h>   /* for size_t and NULL */
#include <sys/mman.h> /* for mmap and munmap */
#include <unistd.h>   /* for write */

#include <stdio.h>

#define ALIGNMENT 16
#define ALIGN(x, alignment) ((x) + (alignment)-1) / (alignment) * (alignment)

#define TINY_ALLOC_NUMBER 100
#define TINY_ALLOC_SIZE 64

typedef enum
{
    TINY,
    SMALL,
    LARGE,
} AllocType;

typedef struct alloc_s
{
    bool      free : 1;
    bool      end : 1;
    AllocType type : 2;

    size_t          size;
    struct alloc_s *past;
    struct alloc_s *next;
    char            data[];
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
            ALIGN(TINY_ALLOC_NUMBER * (TINY_ALLOC_SIZE + sizeof(alloc_t)) + sizeof(alloc_t), page_size);

        header.tiny_allocs = mmap(NULL, tiny_zone_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (header.tiny_allocs == NULL)
            return NULL;

        header.tiny_allocs->free = true;
        header.tiny_allocs->end  = true;
        header.tiny_allocs->type = TINY;
        header.tiny_allocs->past = NULL;
        header.tiny_allocs->next = NULL;
        header.tiny_allocs->size = tiny_zone_size - sizeof(alloc_t);
    }

    if (size <= TINY_ALLOC_SIZE)
    {
        alloc_t *allocs = header.tiny_allocs;

        size_t aligned_size = ALIGN(size, ALIGNMENT);
        while ((!allocs->free || (allocs->size != aligned_size && allocs->size < aligned_size + sizeof(alloc_t))) &&
               !allocs->end)
            allocs = allocs->next;

        alloc_t *next_alloc;
        if (allocs->end)
        {
            // End of allocated memory. TODO: Request more memory for the new allocation;
            if (allocs->size < aligned_size + sizeof(alloc_t))
                return NULL;

            next_alloc       = (alloc_t *)((char *)allocs + sizeof(alloc_t) + aligned_size);
            next_alloc->past = allocs;
            next_alloc->next = NULL;

            next_alloc->size = allocs->size - aligned_size - sizeof(alloc_t);
            next_alloc->free = true;
            next_alloc->end  = true;
            next_alloc->type = TINY;

            allocs->next = next_alloc;
        }
        else if (allocs->size >= aligned_size + sizeof(alloc_t))
        {
            next_alloc       = (alloc_t *)((char *)allocs + sizeof(alloc_t) + aligned_size);
            next_alloc->past = allocs;
            next_alloc->next = allocs->next;

            next_alloc->size = allocs->size - aligned_size - sizeof(alloc_t);
            next_alloc->free = true;
            next_alloc->end  = false;
            next_alloc->type = TINY;

            allocs->next = next_alloc;
        }
        allocs->size = size;
        allocs->free = false;
        allocs->end  = false;

        return allocs->data;
    }
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
    if ((char *)alloc + sizeof(alloc_t) == ptr)
        alloc->free = true;

    // Get real size of free block
    alloc->size = ALIGN(alloc->size, ALIGNMENT);

    if (alloc->past != NULL && alloc->past->free)
    {
        alloc->past->size += alloc->size + sizeof(alloc_t);
        alloc->past->next = alloc->next;
        alloc             = alloc->past;
    }
    if (alloc->next != NULL && alloc->next->free)
    {
        alloc->size += alloc->next->size + sizeof(alloc_t);
        alloc->next = alloc->next->next;
    }
}

// TODO : Remove printf
void show_alloc_mem(void)
{
    size_t total_size = 0;
    printf("TINY:\n");
    alloc_t *tiny_allocs = header.tiny_allocs;
    size_t   base_dir    = (size_t)tiny_allocs;
    while (!tiny_allocs->end)
    {
        char  *position     = (char *)tiny_allocs;
        size_t aligned_size = ALIGN(tiny_allocs->size, ALIGNMENT);
        if (!tiny_allocs->free)
        {
            printf("%-5p -  %-5p : %li bytes\n", position - base_dir + sizeof(alloc_t),
                   position - base_dir + sizeof(alloc_t) + tiny_allocs->size, tiny_allocs->size);
            total_size += tiny_allocs->size;
        }
        tiny_allocs = (alloc_t *)(position + sizeof(alloc_t) + aligned_size);
    }
    printf("Total : %li bytes\n", total_size);
}
