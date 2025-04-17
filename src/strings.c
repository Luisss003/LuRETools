#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "../include/strings.h"

void extract_strings(char *filename, int min_length) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }

    unsigned char buf[1024];
    unsigned char temp[256];
    int count = 0;
    ssize_t bytes, i;

    while ((bytes = read(fd, buf, sizeof(buf))) > 0) {
        for (i = 0; i < bytes; i++) {
            if (isprint(buf[i])) {
                if (count < (int)(sizeof(temp) - 1)) {
                    temp[count++] = buf[i];
                } else {
                    count = 0;
                }
            } else {
                if (count >= min_length) {
                    temp[count] = '\0';
                    write(STDOUT_FILENO, temp, count);
                    write(STDOUT_FILENO, "\n", 1);
                }
                count = 0;
            }
        }
    }

    if (count >= min_length) {
        temp[count] = '\0';
        write(STDOUT_FILENO, temp, count);
        write(STDOUT_FILENO, "\n", 1);
    }

    close(fd);
}

