#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void show_alloc_mem(void);

void write_str(char const *str)
{
    size_t len = strlen(str);
    write(1, str, len);
}

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

static void put_nbr(size_t n)
{

    char   buffer[20];
    size_t digits = count_digits(n, 10);

    for (size_t i = digits - 1; n >= 10; n /= 10)
        buffer[i--] = '0' + (n % 10);
    buffer[0] = '0' + (n % 10);

    write(1, buffer, digits);
}

int main(void)
{

    // Tiny Size Test
    size_t test_size = 5000;
    void  *ptrs[test_size];

    write_str("--- Memory Before Allocations ---\n\n");
    show_alloc_mem();

    for (size_t i = 0; i < test_size; ++i)
    {
        put_nbr(i);
        write(1, "\n", 1);
        if ((ptrs[i] = malloc(1000)) == NULL)
            exit(1);
    }
    write_str("\n--- Memory After Allocations --- \n\n");
    show_alloc_mem();

    for (size_t i = 0; i < test_size; ++i)
    {
        free(ptrs[i]);
    }
    write_str("\n--- Memory After Free ---\n");
    show_alloc_mem();
    return 0;
}
