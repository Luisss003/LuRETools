#include <stdio.h>
#include "elf_parser.h"
#include "hex_view.h"
#include "pe_parser.h"
#include "strings.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <binary>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];

    if (is_elf(filename)) {
        printf("[*] Detected ELF binary.\n");
        parse_elf(filename);
    } else if (is_pe(filename)) {
        printf("[*] Detected PE binary.\n");
        parse_pe(filename);
    } else {
        printf("[!] Unknown binary format.\n");
    }

    printf("\n[Hex Dump]\n");
    hex_view(filename, 64);

    printf("\n[Extracted Strings]\n");
    extract_strings(filename);

    return 0;
}
