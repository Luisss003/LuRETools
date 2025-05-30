#include "strings.h"
#include <stdio.h>
#include <ctype.h>

void extract_strings(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("fopen");
        return;
    }

    int ch, count = 0;
    char buf[256];

    while ((ch = fgetc(file)) != EOF) {
        if (isprint(ch)) {
            if (count < sizeof(buf) - 1)
                buf[count++] = ch;
        } else {
            if (count >= 4) {
                buf[count] = '\0';
                printf("%s\n", buf);
            }
            count = 0;
        }
    }

    if (count >= 4) {
        buf[count] = '\0';
        printf("%s\n", buf);
    }

    fclose(file);
}
