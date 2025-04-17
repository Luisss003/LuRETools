#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <unistd.h>

ssize_t safe_read(int fd, void *buf, size_t count);

void print_hex_line(const uint8_t *buf, size_t length, size_t offset);

int check_magic(const uint8_t *buf, const uint8_t *magic, size_t len);

#endif
