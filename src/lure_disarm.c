#include "lure_disarm.h"

#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LureArch arch_from_elf_machine(uint16_t machine) {
    switch (machine) {
        case EM_386:
            return LURE_ARCH_X86;
        case EM_X86_64:
            return LURE_ARCH_X86_64;
        case EM_AARCH64:
            return LURE_ARCH_ARM64;
        default:
            return LURE_ARCH_UNKNOWN;
    }
}

static int read_at(FILE *file, uint64_t offset, void *buf, size_t size) {
    if (fseek(file, (long)offset, SEEK_SET) != 0) {
        return 0;
    }

    return fread(buf, 1, size, file) == size;
}

static const char *string_at(const char *table, size_t table_size, uint32_t offset) {
    if (!table || offset >= table_size) {
        return "";
    }

    return table + offset;
}

static LureDisarmStatus add_function(LureDisarmImage *image,
                                     const char *name,
                                     uint64_t address,
                                     uint64_t size) {
    LureDisarmFunction *new_functions;
    size_t next_count = image->function_count + 1;

    new_functions = realloc(image->functions, next_count * sizeof(*new_functions));
    if (!new_functions) {
        return LURE_DISARM_ERR_NOMEM;
    }

    image->functions = new_functions;
    LureDisarmFunction *function = &image->functions[image->function_count];
    memset(function, 0, sizeof(*function));

    snprintf(function->name, sizeof(function->name), "%s", name && name[0] ? name : "<unnamed>");
    function->address = address;
    function->size = size;
    image->function_count = next_count;

    return LURE_DISARM_OK;
}

static LureDisarmStatus load_elf_functions(FILE *file,
                                           const Elf64_Shdr *shdrs,
                                           size_t shnum,
                                           LureDisarmImage *image) {
    for (size_t i = 0; i < shnum; i++) {
        if (shdrs[i].sh_type != SHT_SYMTAB && shdrs[i].sh_type != SHT_DYNSYM) {
            continue;
        }

        if (shdrs[i].sh_entsize == 0 || shdrs[i].sh_link >= shnum) {
            continue;
        }

        const Elf64_Shdr *strhdr = &shdrs[shdrs[i].sh_link];
        char *strtab = calloc(1, strhdr->sh_size + 1);
        if (!strtab) {
            return LURE_DISARM_ERR_NOMEM;
        }

        if (!read_at(file, strhdr->sh_offset, strtab, strhdr->sh_size)) {
            free(strtab);
            return LURE_DISARM_ERR_IO;
        }

        size_t symbol_count = shdrs[i].sh_size / shdrs[i].sh_entsize;
        for (size_t j = 0; j < symbol_count; j++) {
            Elf64_Sym sym;
            uint64_t symbol_offset = shdrs[i].sh_offset + (j * shdrs[i].sh_entsize);

            if (!read_at(file, symbol_offset, &sym, sizeof(sym))) {
                free(strtab);
                return LURE_DISARM_ERR_IO;
            }

            if (ELF64_ST_TYPE(sym.st_info) != STT_FUNC || sym.st_value == 0) {
                continue;
            }

            LureDisarmStatus status = add_function(image,
                                                   string_at(strtab, strhdr->sh_size, sym.st_name),
                                                   sym.st_value,
                                                   sym.st_size);
            if (status != LURE_DISARM_OK) {
                free(strtab);
                return status;
            }
        }

        free(strtab);
    }

    return LURE_DISARM_OK;
}

static LureDisarmStatus load_elf64(const char *filename, LureDisarmImage *image) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        return LURE_DISARM_ERR_IO;
    }

    Elf64_Ehdr ehdr;
    if (!read_at(file, 0, &ehdr, sizeof(ehdr))) {
        fclose(file);
        return LURE_DISARM_ERR_IO;
    }

    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0 ||
        ehdr.e_ident[EI_CLASS] != ELFCLASS64) {
        fclose(file);
        return LURE_DISARM_ERR_FORMAT;
    }

    image->filename = filename;
    image->format = LURE_BINARY_ELF64;
    image->arch = arch_from_elf_machine(ehdr.e_machine);
    image->entry = ehdr.e_entry;

    if (ehdr.e_shnum == 0) {
        fclose(file);
        return LURE_DISARM_OK;
    }

    Elf64_Shdr *shdrs = calloc(ehdr.e_shnum, sizeof(*shdrs));
    if (!shdrs) {
        fclose(file);
        return LURE_DISARM_ERR_NOMEM;
    }

    if (!read_at(file, ehdr.e_shoff, shdrs, ehdr.e_shnum * sizeof(*shdrs))) {
        free(shdrs);
        fclose(file);
        return LURE_DISARM_ERR_IO;
    }

    char *section_names = NULL;
    size_t section_names_size = 0;

    if (ehdr.e_shstrndx != SHN_UNDEF && ehdr.e_shstrndx < ehdr.e_shnum) {
        Elf64_Shdr *name_hdr = &shdrs[ehdr.e_shstrndx];
        section_names = calloc(1, name_hdr->sh_size + 1);
        if (!section_names) {
            free(shdrs);
            fclose(file);
            return LURE_DISARM_ERR_NOMEM;
        }

        if (!read_at(file, name_hdr->sh_offset, section_names, name_hdr->sh_size)) {
            free(section_names);
            free(shdrs);
            fclose(file);
            return LURE_DISARM_ERR_IO;
        }

        section_names_size = name_hdr->sh_size;
    }

    image->sections = calloc(ehdr.e_shnum, sizeof(*image->sections));
    if (!image->sections) {
        free(section_names);
        free(shdrs);
        fclose(file);
        return LURE_DISARM_ERR_NOMEM;
    }

    image->section_count = ehdr.e_shnum;
    for (size_t i = 0; i < image->section_count; i++) {
        LureDisarmSection *section = &image->sections[i];
        const char *name = string_at(section_names, section_names_size, shdrs[i].sh_name);

        snprintf(section->name, sizeof(section->name), "%s", name[0] ? name : "<unnamed>");
        section->vaddr = shdrs[i].sh_addr;
        section->file_offset = shdrs[i].sh_offset;
        section->size = shdrs[i].sh_size;
        section->flags = shdrs[i].sh_flags;
        section->executable = (shdrs[i].sh_flags & SHF_EXECINSTR) != 0;
        section->writable = (shdrs[i].sh_flags & SHF_WRITE) != 0;
        section->readable = shdrs[i].sh_type != SHT_NOBITS;
    }

    LureDisarmStatus status = load_elf_functions(file, shdrs, ehdr.e_shnum, image);

    free(section_names);
    free(shdrs);
    fclose(file);
    return status;
}

const char *lure_disarm_status_string(LureDisarmStatus status) {
    switch (status) {
        case LURE_DISARM_OK:
            return "ok";
        case LURE_DISARM_ERR_IO:
            return "I/O error";
        case LURE_DISARM_ERR_FORMAT:
            return "invalid or unknown binary format";
        case LURE_DISARM_ERR_UNSUPPORTED:
            return "unsupported binary feature";
        case LURE_DISARM_ERR_NOMEM:
            return "out of memory";
        default:
            return "unknown error";
    }
}

const char *lure_disarm_format_string(LureBinaryFormat format) {
    switch (format) {
        case LURE_BINARY_ELF64:
            return "ELF64";
        case LURE_BINARY_PE:
            return "PE";
        case LURE_BINARY_UNKNOWN:
        default:
            return "unknown";
    }
}

const char *lure_disarm_arch_string(LureArch arch) {
    switch (arch) {
        case LURE_ARCH_X86:
            return "x86";
        case LURE_ARCH_X86_64:
            return "x86-64";
        case LURE_ARCH_ARM64:
            return "arm64";
        case LURE_ARCH_UNKNOWN:
        default:
            return "unknown";
    }
}

LureDisarmOptions lure_disarm_default_options(void) {
    LureDisarmOptions options;

    options.max_preview_bytes = 96;
    options.show_sections = 1;
    options.show_functions = 1;
    options.show_preview = 1;

    return options;
}

LureDisarmStatus lure_disarm_load(const char *filename, LureDisarmImage *image) {
    unsigned char ident[EI_NIDENT];
    FILE *file;

    if (!filename || !image) {
        return LURE_DISARM_ERR_FORMAT;
    }

    memset(image, 0, sizeof(*image));

    file = fopen(filename, "rb");
    if (!file) {
        return LURE_DISARM_ERR_IO;
    }

    if (fread(ident, 1, sizeof(ident), file) != sizeof(ident)) {
        fclose(file);
        return LURE_DISARM_ERR_IO;
    }

    fclose(file);

    if (memcmp(ident, ELFMAG, SELFMAG) == 0) {
        if (ident[EI_CLASS] != ELFCLASS64) {
            return LURE_DISARM_ERR_UNSUPPORTED;
        }

        return load_elf64(filename, image);
    }

    if (ident[0] == 'M' && ident[1] == 'Z') {
        image->filename = filename;
        image->format = LURE_BINARY_PE;
        return LURE_DISARM_ERR_UNSUPPORTED;
    }

    return LURE_DISARM_ERR_FORMAT;
}

void lure_disarm_free(LureDisarmImage *image) {
    if (!image) {
        return;
    }

    free(image->sections);
    free(image->functions);
    memset(image, 0, sizeof(*image));
}

LureDisarmStatus lure_disarm_read_bytes(const LureDisarmImage *image,
                                        uint64_t file_offset,
                                        void *buf,
                                        size_t size) {
    FILE *file;
    int ok;

    if (!image || !image->filename || !buf) {
        return LURE_DISARM_ERR_FORMAT;
    }

    file = fopen(image->filename, "rb");
    if (!file) {
        return LURE_DISARM_ERR_IO;
    }

    ok = read_at(file, file_offset, buf, size);
    fclose(file);

    return ok ? LURE_DISARM_OK : LURE_DISARM_ERR_IO;
}

void lure_disarm_print_summary(const LureDisarmImage *image) {
    if (!image) {
        return;
    }

    printf("[lure-disarm]\n");
    printf("file: %s\n", image->filename ? image->filename : "<unknown>");
    printf("format: %s\n", lure_disarm_format_string(image->format));
    printf("arch: %s\n", lure_disarm_arch_string(image->arch));
    printf("entry: 0x%lx\n", (unsigned long)image->entry);
    printf("sections: %zu\n", image->section_count);
    printf("functions: %zu\n", image->function_count);
}

void lure_disarm_print_sections(const LureDisarmImage *image) {
    if (!image || image->section_count == 0) {
        return;
    }

    printf("\n[Sections]\n");
    printf("%-20s %-14s %-10s %-10s %s\n", "name", "vaddr", "offset", "size", "flags");

    for (size_t i = 0; i < image->section_count; i++) {
        const LureDisarmSection *section = &image->sections[i];
        printf("%-20s 0x%012lx 0x%08lx 0x%08lx %c%c%c\n",
               section->name,
               (unsigned long)section->vaddr,
               (unsigned long)section->file_offset,
               (unsigned long)section->size,
               section->readable ? 'r' : '-',
               section->writable ? 'w' : '-',
               section->executable ? 'x' : '-');
    }
}

void lure_disarm_print_functions(const LureDisarmImage *image) {
    if (!image || image->function_count == 0) {
        return;
    }

    printf("\n[Functions]\n");
    printf("%-14s %-10s %s\n", "address", "size", "name");

    for (size_t i = 0; i < image->function_count; i++) {
        const LureDisarmFunction *function = &image->functions[i];
        printf("0x%012lx 0x%08lx %s\n",
               (unsigned long)function->address,
               (unsigned long)function->size,
               function->name);
    }
}

static void print_preview_line(uint64_t address,
                               const unsigned char *bytes,
                               size_t byte_count) {
    printf("0x%012lx  ", (unsigned long)address);

    for (size_t i = 0; i < 8; i++) {
        if (i < byte_count) {
            printf("%02x ", bytes[i]);
        } else {
            printf("   ");
        }
    }

    printf(" db ");
    for (size_t i = 0; i < byte_count; i++) {
        printf("0x%02x%s", bytes[i], i + 1 == byte_count ? "" : ", ");
    }
    printf("\n");
}

LureDisarmStatus lure_disarm_print_preview(const LureDisarmImage *image,
                                           const LureDisarmOptions *options) {
    size_t max_preview_bytes = options ? options->max_preview_bytes : 96;
    FILE *file;

    if (!image || !image->filename) {
        return LURE_DISARM_ERR_FORMAT;
    }

    file = fopen(image->filename, "rb");
    if (!file) {
        return LURE_DISARM_ERR_IO;
    }

    printf("\n[Executable Section Preview]\n");
    printf("; decoder backend is intentionally stubbed: bytes are emitted as db records\n");

    for (size_t i = 0; i < image->section_count; i++) {
        const LureDisarmSection *section = &image->sections[i];
        uint64_t remaining;
        uint64_t cursor = 0;

        if (!section->executable || section->size == 0 || !section->readable) {
            continue;
        }

        remaining = section->size < max_preview_bytes ? section->size : max_preview_bytes;
        printf("\n%s:\n", section->name);

        while (remaining > 0) {
            unsigned char bytes[8];
            size_t chunk = remaining < sizeof(bytes) ? (size_t)remaining : sizeof(bytes);

            if (!read_at(file, section->file_offset + cursor, bytes, chunk)) {
                fclose(file);
                return LURE_DISARM_ERR_IO;
            }

            print_preview_line(section->vaddr + cursor, bytes, chunk);
            cursor += chunk;
            remaining -= chunk;
        }
    }

    fclose(file);
    return LURE_DISARM_OK;
}

LureDisarmStatus lure_disarm_run_passes(const LureDisarmImage *image,
                                        const LureDisarmPass *passes,
                                        size_t pass_count) {
    if (!image || (!passes && pass_count > 0)) {
        return LURE_DISARM_ERR_FORMAT;
    }

    for (size_t i = 0; i < pass_count; i++) {
        LureDisarmStatus status;

        if (!passes[i].run) {
            continue;
        }

        status = passes[i].run(image, passes[i].user_data);
        if (status != LURE_DISARM_OK) {
            fprintf(stderr,
                    "lure-disarm: pass '%s' failed: %s\n",
                    passes[i].name ? passes[i].name : "<unnamed>",
                    lure_disarm_status_string(status));
            return status;
        }
    }

    return LURE_DISARM_OK;
}

int lure_disarm_run_cli(const char *filename, const LureDisarmOptions *options) {
    LureDisarmImage image;
    LureDisarmOptions effective_options = options ? *options : lure_disarm_default_options();
    LureDisarmStatus status = lure_disarm_load(filename, &image);

    if (status != LURE_DISARM_OK) {
        fprintf(stderr, "lure-disarm: %s: %s\n", filename, lure_disarm_status_string(status));
        return 1;
    }

    lure_disarm_print_summary(&image);

    if (effective_options.show_sections) {
        lure_disarm_print_sections(&image);
    }

    if (effective_options.show_functions) {
        lure_disarm_print_functions(&image);
    }

    if (effective_options.show_preview) {
        status = lure_disarm_print_preview(&image, &effective_options);
        if (status != LURE_DISARM_OK) {
            fprintf(stderr, "lure-disarm: preview failed: %s\n", lure_disarm_status_string(status));
            lure_disarm_free(&image);
            return 1;
        }
    }

    lure_disarm_free(&image);
    return 0;
}

int lure_disarm_main(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        fprintf(stderr, "Usage: luretools disarm <binary>\n");
        return argc < 2 ? 1 : 0;
    }

    return lure_disarm_run_cli(argv[1], NULL);
}
