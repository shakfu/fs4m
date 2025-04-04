#include <stdio.h>
#include <stdlib.h>

int main()
{
    char *buf;
    size_t size;
    FILE* mem = open_memstream(&buf, &size);

    fprintf(mem, "HELLO WORLD\n");

    // off_t current = ftell(mem);

    fclose(mem);

    printf("buf: %s", buf);

    free(buf);
}

