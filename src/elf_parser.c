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

    unsigned char arch_word_size[16];
    if (read(fd, arch_word_size, 16) != 16) {
        dprintf(STDERR_FILENO, "Failed to read ELF arch_word_size\n");
        close(fd);
        return;
    }

    if (memcmp(arch_word_size, "\x7f""ELF", 4) != 0) {
        dprintf(STDOUT_FILENO, "Not an ELF file.\n");
    } 
    else {
        dprintf(STDOUT_FILENO, "Valid ELF file.\n");

        if(arch_word_size[4] == 1) {
            dprintf(STDOUT_FILENO, "Class: 32-bit\n");
        } 
        else if (arch_word_size[4] == 2) {
            dprintf(STDOUT_FILENO, "Class: 64-bit\n");
        } 
        else {
            dprintf(STDOUT_FILENO, "Class: Unknown\n");
        }
    }

    close(fd);
}
