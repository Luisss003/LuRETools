#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include "../include/hex_view.h"

void print_hex(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }

    unsigned char buf[16];
    ssize_t n, i;
    off_t offset = 0;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        printf("%08lx  ", offset);

        for (i = 0; i < 16; i++) {
            if (i < n) {
                printf("%02x ", buf[i]);
            } else {
                printf("   ");
            }
        }

        printf(" ");

        for (i = 0; i < n; i++) {
            if (isprint(buf[i])) {
                printf("%c", buf[i]);
            } 
            else {
                printf(".");
            }
        }

        printf("\n");
        offset += n;
    }

    close(fd);
}
