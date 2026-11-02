#include <stdbool.h>  /* for bool */
#include <stddef.h>   /* for size_t and NULL */
#include <sys/mman.h> /* for mmap and munmap */
#include <unistd.h>   /* for write */

#define ALIGNMENT 16
#define ALIGN(x, alignment) ((x) + (alignment) - 1) / (alignment) * (alignment)

#define TINY_ALLOC_NUMBER 100
#define TINY_ALLOC_SIZE 64

#define SMALL_ALLOC_NUMBER 100
#define SMALL_ALLOC_SIZE 256

#define LARGE_ALLOC_NUMBER 100

#ifdef MALLOC_DEBUG
#include <string.h>
#define log(s) write(2, s, strlen(s));
#else
#define log(s)
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
    unsigned  extra_space : 5;

    size_t size;
    // ptr and header are used for large allocations
    // ptr points to the header in the large allocation requested memory
    // header points to the header in large_zone
    union {
        struct alloc_s *past;
        struct alloc_s *ptr;    // In large allocations, ptr->size hold requested size
        struct alloc_s *header; // In large allocations, header->size hold true size
    };

    union {
        struct alloc_s *next;
        size_t          reserved_size; // In large allocations, indicate size used in
    };
    char data[];
} alloc_t;

typedef struct
{
    alloc_t *tiny_allocs;
    alloc_t *small_allocs;
    alloc_t *large_allocs;
} malloc_header_t;

static malloc_header_t headers;

// Initialize internal memory mappings. Return false if allocations fails.
static bool init_headers()
{
    size_t page_size = sysconf(_SC_PAGESIZE);

    // Calculate minimum page number for every zone
    size_t tiny_zone_size = ALIGN(TINY_ALLOC_NUMBER * (TINY_ALLOC_SIZE + sizeof(alloc_t)) + sizeof(alloc_t), page_size);
    size_t small_zone_size =
        ALIGN(SMALL_ALLOC_NUMBER * (SMALL_ALLOC_SIZE + sizeof(alloc_t)) + sizeof(alloc_t), page_size);
    size_t large_zone_size = ALIGN(LARGE_ALLOC_NUMBER * sizeof(alloc_t), page_size);

    headers.tiny_allocs  = mmap(NULL, tiny_zone_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    headers.small_allocs = mmap(NULL, small_zone_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    headers.large_allocs = mmap(NULL, small_zone_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (headers.tiny_allocs == NULL || headers.small_allocs == NULL || headers.large_allocs == NULL)
    {
        if (headers.tiny_allocs != NULL)
            munmap(headers.tiny_allocs, tiny_zone_size);
        if (headers.small_allocs != NULL)
            munmap(headers.small_allocs, small_zone_size);
        if (headers.large_allocs != NULL)
            munmap(headers.large_allocs, large_zone_size);
        return false;
    }

    headers.tiny_allocs->type = TINY;
    headers.tiny_allocs->free = true;
    headers.tiny_allocs->size = tiny_zone_size - sizeof(alloc_t);

    headers.small_allocs->type = SMALL;
    headers.small_allocs->free = true;
    headers.small_allocs->size = small_zone_size - sizeof(alloc_t);

    headers.large_allocs->type = LARGE;
    headers.large_allocs->free = true;
    headers.large_allocs->size = large_zone_size - sizeof(alloc_t);
    return true;
}

static void *alloc_large(size_t size)
{
    alloc_t *header = headers.large_allocs;

    while (header->next != NULL && !header->free)
        header = header->next;

    // End of allocated memory. TODO: Request more memory for the new
    // allocation;
    if (header->next == NULL && header->size < sizeof(alloc_t))
        return NULL;

    size_t   alloc_size = ALIGN(size + sizeof(alloc_t), sysconf(_SC_PAGESIZE));
    alloc_t *alloc      = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (alloc == NULL)
        return NULL;

    header->free = false;
    header->ptr  = alloc;

    alloc->type          = LARGE;
    alloc->free          = false;
    alloc->size          = size;
    alloc->reserved_size = alloc_size;
    alloc->header        = header;
    if (header->next == NULL)
    {
        header->next = header + 1;
        header->size = 0;

        header->next->type = LARGE;
        header->next->free = true;
        header->next->size = header->size - sizeof(alloc_t);
        header->next->next = NULL;
        header->next->ptr  = NULL;
    }
    return alloc->data;
}

void *malloc(size_t size)
{
    log("malloc\n");
    // Initialize memory mappings in the first call
    if (headers.tiny_allocs == NULL)
    {
        if (!init_headers())
            return NULL;
    }

    alloc_t *alloc;
    if (size <= TINY_ALLOC_SIZE)
        alloc = headers.tiny_allocs;
    else if (size <= SMALL_ALLOC_SIZE)
        alloc = headers.small_allocs;
    else
        return alloc_large(size);

    size_t aligned_size = ALIGN(size, ALIGNMENT);
    while (alloc->next != NULL && (!alloc->free || alloc->size < aligned_size))
        alloc = alloc->next;

    // End of allocated memory. TODO: Request more memory for the new
    // allocation;
    if (alloc->next == NULL && alloc->size < aligned_size + sizeof(alloc_t))
        return NULL;
    alloc_t *next_alloc;
    next_alloc = (alloc_t *)((char *)alloc + sizeof(alloc_t) + aligned_size);

    next_alloc->type = alloc->type;
    next_alloc->free = true;
    next_alloc->size = alloc->size - aligned_size - sizeof(alloc_t);

    next_alloc->past = alloc;
    next_alloc->next = alloc->next;

    alloc->next = next_alloc;
    alloc->size = size;
    alloc->free = false;

    return alloc->data;
}

void free(void *ptr)
{
    log("free\n");
    if (ptr == NULL)
        return;
    alloc_t *alloc = ((alloc_t *)ptr - 1);

    if (alloc->type == LARGE)
    {
        alloc->header->free = true;
        alloc->header->ptr  = NULL;
        munmap(alloc, alloc->reserved_size);
        return;
    }
    alloc->free = true;

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

static void ft_memcpy(void *dst, const void *src, size_t size)
{
    while (size-- != 0)
    {
        *(unsigned char *)dst = *(unsigned char *)src;

        dst = (unsigned char *)dst + 1;
        src = (unsigned char *)src + 1;
    }
}

// TODO: remove memmove
void *realloc(void *ptr, size_t size)
{
    log("realloc\n");
    if (ptr == NULL)
        return malloc(size);
    if (size == 0)
    {
        free(ptr);
        return NULL;
    }
    alloc_t *alloc = (alloc_t *)ptr - 1;
    if (size == alloc->size)
        return alloc->ptr;

    if (alloc->type == LARGE)
    {
        if (size > alloc->reserved_size)
        {
            void *new_alloc = malloc(size);
            if (new_alloc == NULL)
                return NULL;
            ft_memcpy(new_alloc, alloc->data, alloc->size);
            free(alloc->data);
            return new_alloc;
        }
        else
        {
            alloc->size = size;
            return alloc->data;
        }
    }
    return NULL;
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

    char   buffer[20];
    size_t digits = count_digits(n, 10);

    buffer[0] = '0';
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

void show_alloc_mem(void)
{
    if (headers.tiny_allocs == NULL)
        return;

    size_t total_size = 0;

    alloc_t *tiny_allocs = headers.tiny_allocs;
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

    alloc_t *small_allocs = headers.small_allocs;
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

    alloc_t *large_allocs = headers.large_allocs;

    bool all_free = true;
    while (large_allocs->next != NULL)
    {
        if (!large_allocs->free)
        {
            all_free = false;
            break;
        }
        large_allocs = large_allocs->next;
    }

    large_allocs = headers.large_allocs;
    if (!all_free && large_allocs->next != NULL)
        write(1, "LARGE:\n", 7);
    while (large_allocs->next != NULL)
    {
        if (!large_allocs->free)
        {
            print_alloc(large_allocs->ptr->data, large_allocs->ptr->size);
            total_size += large_allocs->ptr->size;
        }
        large_allocs = large_allocs->next;
    }
    write(1, "Total : ", 8);
    put_nbr(total_size);
    write(1, " bytes\n", 7);
}
