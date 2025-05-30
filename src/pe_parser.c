#include "pe_parser.h"
#include <stdio.h>

int is_pe(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) return 0;

    unsigned char mz[2];
    fread(mz, 1, 2, file);
    fclose(file);

    return mz[0] == 'M' && mz[1] == 'Z';
}

void parse_pe(const char *filename) {
    printf("PE parsing not implemented yet.\n");
}
