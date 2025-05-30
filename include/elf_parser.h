#ifndef ELF_PARSER_H
#define ELF_PARSER_H

int is_elf(const char *filename);
void parse_elf(const char *filename);

#endif
