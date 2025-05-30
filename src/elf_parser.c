#include "elf_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>
/**
 * Checks if the given file is an ELF binary.
 * This function reads the first few bytes of the file to check
 * for the ELF magic number (0x7f followed by 'E', 'L', 'F').
 */
int is_elf(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) return 0;

    //Read the ELF header to check the magic number
    unsigned char e_ident[EI_NIDENT];
    fread(e_ident, 1, EI_NIDENT, file);
    fclose(file);

    //Determine whether the file is an ELF file by checking the magic number
    return memcmp(e_ident, ELFMAG, SELFMAG) == 0;
}

/**
 * Parses the ELF file and prints some basic information.
 * This function reads the ELF header and prints the entry point,
 * program header offset, and section header offset.
 */
void parse_elf(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("fopen");
        return;
    }

    //Read the ELF header
    Elf64_Ehdr *ehdr;
    fread(&ehdr, 1, sizeof(ehdr), file);

    printf("ELF Entry Point: 0x%lx\n", ehdr->e_entry);
    printf("Program Header Offset: 0x%lx\n", ehdr->e_phoff);
    printf("Section Header Offset: 0x%lx\n", ehdr->e_shoff);

    fclose(file);
}
