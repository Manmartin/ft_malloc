#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void show_alloc_mem(void);

void write_str(char const *str)
{
    size_t len = strlen(str);
    write(1, str, len);
}


int main(void)
{
    
    // Tiny Size Test
    size_t test_size = 65;
    void *ptrs[test_size];
   
    write_str("--- Memory Before Allocations ---\n\n");
    show_alloc_mem();
    
    for (size_t i = 0; i < test_size; ++i) {
        if ((ptrs[i] = malloc(i + 1)) == NULL)
            exit(1);
    }
    write_str("\n--- Memory After Allocations --- \n\n");
    show_alloc_mem();

    for (size_t i = 0; i < test_size; ++i) {
        free(ptrs[i]);
    }
    write_str("\n--- Memory After Free ---\n");
    show_alloc_mem();
    return 0;
}
