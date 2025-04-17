#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "../include/elf_parser.h"

void check_elf_header(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }

    unsigned char file_info[16];
    if (read(fd, file_info, 16) != 16) {
        dprintf(STDERR_FILENO, "Failed to read ELF file_info\n");
        close(fd);
        return;
    }

    if (memcmp(file_info, "\x7f""ELF", 4) != 0) {
        dprintf(STDOUT_FILENO, "Not an ELF file.\n");
    } 
    else {
        dprintf(STDOUT_FILENO, "Valid ELF file.\n");

        if(file_info[4] == 1) {
            dprintf(STDOUT_FILENO, "Class: 32-bit\n");
        } 
        else if (file_info[4] == 2) {
            dprintf(STDOUT_FILENO, "Class: 64-bit\n");
        } 
        else {
            dprintf(STDOUT_FILENO, "Class: Unknown\n");
        }
    }

    close(fd);
}
