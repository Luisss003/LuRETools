#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "../include/strings.h"
#include "../include/hex_view.h"
#include "../include/elf_parser.h"

void print_usage(const char *prog) {
    printf(
        "Usage: %s <mode> <file>\n"
        "Modes:\n"
        "  -strings    Extract printable strings\n"
        "  -hex        Print file in hex + ASCII\n"
        "  -elf        Check ELF header\n"
        "  -net        Parses PCAP file\n",
        prog);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *mode = argv[1];
    const char *filename = argv[2];

    if (strcmp(mode, "-strings") == 0) {
        extract_strings(filename, 4); // default min length = 4
    }
    else if (strcmp(mode, "-hex") == 0) {
        print_hex(filename);
    }
    else if (strcmp(mode, "-elf") == 0) {
        check_elf_header(filename);
    }
    else {
        printf("Unknown mode: %s\n", mode);
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
