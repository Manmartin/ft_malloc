#include <stdbool.h>  /* for bool */
#include <stddef.h>   /* for size_t and NULL */
#include <sys/mman.h> /* for mmap and munmap */
#include <unistd.h>   /* for write */

#define ALIGNMENT 16
#define ALIGN(x, alignment) ((x) + (alignment)-1) / (alignment) * (alignment)

#define TINY_ALLOC_NUMBER 100
#define TINY_ALLOC_SIZE 64

#define SMALL_ALLOC_NUMBER 100
#define SMALL_ALLOC_SIZE 256

#define LARGE_ALLOC_NUMBER 100

#ifdef MALLOC_DEBUG
# include <string.h>
# define log(s) write(2, s, strlen(s));
#else
# define log(s)
#endif


typedef enum
{
    TINY,
    SMALL,
    LARGE,
} AllocType;

typedef struct alloc_s
{
    AllocType type : 2;
    bool      free : 1;

    size_t          size;

    // ptr and header are used for large allocations
    // ptr points to the header in the large allocation requested memory
    // header points to the header in large_zone
    union {
        struct alloc_s *past;
        struct alloc_s *ptr;
        struct alloc_s *header;
    };
    struct alloc_s *next;
    char            data[];
} alloc_t;

typedef struct
{
    alloc_t *tiny_allocs;
    alloc_t *small_allocs;
    alloc_t *large_allocs;
} malloc_header_t;

static malloc_header_t header;

void *malloc(size_t size)
{
    log("malloc\n");
    // If is the first call Initialize memory mappings
    if (header.tiny_allocs == NULL)
    {

        size_t page_size = sysconf(_SC_PAGESIZE);

        // Calculate minimum page number for every zone
        size_t tiny_zone_size =
            ALIGN(TINY_ALLOC_NUMBER * (TINY_ALLOC_SIZE + sizeof(alloc_t)) + sizeof(alloc_t), page_size);
        size_t small_zone_size =
            ALIGN(SMALL_ALLOC_NUMBER * (SMALL_ALLOC_SIZE + sizeof(alloc_t)) + sizeof(alloc_t), page_size);
        size_t large_zone_size =
            ALIGN(LARGE_ALLOC_NUMBER * sizeof(alloc_t), page_size);

        header.tiny_allocs  = mmap(NULL, tiny_zone_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        header.small_allocs = mmap(NULL, small_zone_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        header.large_allocs = mmap(NULL, small_zone_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (header.tiny_allocs == NULL || header.small_allocs == NULL || header.large_allocs == NULL)
        {
            if (header.tiny_allocs != NULL)
                munmap(header.tiny_allocs, tiny_zone_size);
            if (header.small_allocs != NULL)
                munmap(header.small_allocs, small_zone_size);
            if (header.large_allocs != NULL)
                munmap(header.large_allocs, large_zone_size);
            return NULL;
        }

        header.tiny_allocs->type = TINY;
        header.tiny_allocs->free = true;
        header.tiny_allocs->size = tiny_zone_size - sizeof(alloc_t);
        header.tiny_allocs->past = NULL;
        header.tiny_allocs->next = NULL;

        header.small_allocs->type = SMALL;
        header.small_allocs->free = true;
        header.small_allocs->size = small_zone_size - sizeof(alloc_t);
        header.small_allocs->past = NULL;
        header.small_allocs->next = NULL;

        header.large_allocs->type = LARGE;
        header.large_allocs->free = true;
        header.large_allocs->size = large_zone_size - sizeof(alloc_t);
        header.large_allocs->ptr  = NULL;
        header.large_allocs->next = NULL;
    }

    alloc_t *alloc;
    if (size <= TINY_ALLOC_SIZE)
        alloc = header.tiny_allocs;
    else if (size <= SMALL_ALLOC_SIZE)
        alloc = header.small_allocs;
    else
    {
        alloc = header.large_allocs;
        while (alloc->next != NULL && !alloc->free)
            alloc = alloc->next;

        // End of allocated memory. TODO: Request more memory for the new allocation;
        if (alloc->next == NULL && alloc->size < sizeof(alloc_t))
            return NULL;

        size_t   ptr_size = ALIGN(size + sizeof(alloc_t), sysconf(_SC_PAGESIZE));
        alloc_t *ptr      = mmap(NULL, ptr_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == NULL)
            return NULL;
        ptr->type = LARGE;
        ptr->free = false;
        ptr->size = size;
        alloc->free = false;
        if (alloc->next == NULL)
        {
            alloc->next = alloc + 1;
            alloc->next->free = true;
            alloc->next->size = alloc->size - sizeof(alloc_t);
            alloc->size = 0;
            alloc->next->type = LARGE;
            alloc->next->next = NULL;
            alloc->next->ptr  = NULL;
        }

        alloc->ptr = ptr;
        ptr->header = alloc;
        return ptr->data;
    }

    size_t aligned_size = ALIGN(size, ALIGNMENT);
    while ((!alloc->free || (alloc->size != aligned_size && alloc->size < aligned_size + sizeof(alloc_t))) &&
           alloc->next != NULL)
        alloc = alloc->next;

    alloc_t *next_alloc;
    // End of allocated memory. TODO: Request more memory for the new allocation;
    if (alloc->next == NULL && alloc->size < aligned_size + sizeof(alloc_t))
        return NULL;
    else if (alloc->size >= aligned_size + sizeof(alloc_t))
    {
        next_alloc = (alloc_t *)((char *)alloc + sizeof(alloc_t) + aligned_size);

        next_alloc->type = alloc->type;
        next_alloc->free = true;
        next_alloc->size = alloc->size - aligned_size - sizeof(alloc_t);

        next_alloc->past = alloc;
        next_alloc->next = alloc->next;

        alloc->next = next_alloc;
    }
    alloc->size = size;
    alloc->free = false;

    return alloc->data;
    return NULL;
}

void *realloc(void *ptr, size_t size)
{
    log("realloc");
    if (ptr == NULL)
        return malloc(size);
    return NULL;
}

void free(void *ptr)
{
    log("free");
    if (ptr == NULL)
        return;

    alloc_t *alloc = ((alloc_t *)ptr - 1);
    alloc->free    = true;
    if (alloc->type == LARGE)
    {
        size_t ptr_size = ALIGN(alloc->size + sizeof(alloc_t), sysconf(_SC_PAGESIZE));
        alloc->header->free = true;
        munmap(alloc, ptr_size);
        return;
    }

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

// Aux print functions (used to avoid malloc calls from printf)
static size_t count_digits(size_t n, size_t base)
{
    size_t digits = 0;

    if (n == 0)
        return 1;
    while (n > 0)
    {
        ++digits;
        n /= base;
    }
    return digits;
}

static void put_nbr16(size_t n)
{
    char buffer[16];
    char base[] = "0123456789ABCDEF";

    size_t digits = count_digits(n, 16);

    for (size_t i = digits - 1; n != 0; n /= 16)
        buffer[i--] = base[n % 16];

    write(1, buffer, digits);
}

static void put_nbr(size_t n)
{

    char buffer[20];
    size_t digits = count_digits(n, 10);

    for (size_t i = digits - 1; n != 0; n /= 10)
        buffer[i--] = '0' + (n % 10);

    write(1, buffer, digits);
}


static void print_alloc(void *origin, size_t size)
{
    write(1, "  0x", 4);
    put_nbr16((size_t)origin);
    write(1, " - 0x", 5);
    put_nbr16((size_t)((char *)origin + size));
    write(1, " : ", 3);
    put_nbr(size);
    write(1, " bytes\n", 7);
}

#include <stdio.h>

void show_alloc_mem(void)
{
    if (header.tiny_allocs == NULL)
        return;

    size_t total_size = 0;

    alloc_t *tiny_allocs = header.tiny_allocs;
    if (tiny_allocs->next != NULL)
        write(1, "TINY:\n", 6);
    while (tiny_allocs->next != NULL)
    {
        if (!tiny_allocs->free)
        {
            print_alloc(tiny_allocs, tiny_allocs->size);
            total_size += tiny_allocs->size;
        }
        tiny_allocs = tiny_allocs->next;
    }

    alloc_t *small_allocs = header.small_allocs;
    if (small_allocs->next != NULL)
        write(1, "SMALL:\n", 7);
    while (small_allocs->next != NULL)
    {
        if (!small_allocs->free)
        {
            print_alloc(small_allocs, small_allocs->size);
            total_size += small_allocs->size;
        }
        small_allocs = small_allocs->next;
    }

    alloc_t *large_allocs = header.large_allocs;
    if (large_allocs->next != NULL)
        write(1, "LARGE:\n", 7);
    while (large_allocs->next != NULL)
    {
        if (!large_allocs->free)
        {
            print_alloc(large_allocs, large_allocs->ptr->size);
            total_size += large_allocs->ptr->size;
        }
        large_allocs = large_allocs->next;
    }
    write(1, "Total : ", 8);
    put_nbr(total_size);
    write(1, " bytes\n", 7);
}
