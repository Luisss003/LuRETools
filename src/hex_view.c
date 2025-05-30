#include "hex_view.h"
#include <stdio.h>
#include <ctype.h>

/**
 * Displays a hexadecimal view of the specified file.
 * This function reads the file in chunks and prints each byte in hexadecimal format,
 * along with its ASCII representation if printable.
 */
void hex_view(const char *filename, size_t length) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("fopen");
        return;
    }


    unsigned char buffer[16];
    size_t bytesRead;
    size_t total_read = 0;

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0 && total_read < length) {
        printf("%08zx  ", total_read);
        for (int i = 0; i < 16; i++) {
            if (i < bytesRead){
                printf("%02x ", buffer[i]);
            }
            else {
                printf("   ");
            }
        }

        printf(" ");
        for (size_t i = 0; i < bytesRead; i++)
            printf("%c", isprint(buffer[i]) ? buffer[i] : '.');

        printf("\n");
        total_read += bytesRead;
    }

    fclose(file);
}
