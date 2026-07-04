#ifndef LURE_DISARM_H
#define LURE_DISARM_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    LURE_DISARM_OK = 0,
    LURE_DISARM_ERR_IO,
    LURE_DISARM_ERR_FORMAT,
    LURE_DISARM_ERR_UNSUPPORTED,
    LURE_DISARM_ERR_NOMEM
} LureDisarmStatus;

typedef enum {
    LURE_BINARY_UNKNOWN = 0,
    LURE_BINARY_ELF64,
    LURE_BINARY_PE
} LureBinaryFormat;

typedef enum {
    LURE_ARCH_UNKNOWN = 0,
    LURE_ARCH_X86,
    LURE_ARCH_X86_64,
    LURE_ARCH_ARM64
} LureArch;

typedef struct {
    char name[64];
    uint64_t vaddr;
    uint64_t file_offset;
    uint64_t size;
    uint64_t flags;
    int executable;
    int readable;
    int writable;
} LureDisarmSection;

typedef struct {
    char name[128];
    uint64_t address;
    uint64_t size;
} LureDisarmFunction;

typedef struct {
    uint64_t address;
    uint64_t file_offset;
    uint8_t bytes[16];
    size_t byte_count;
    char mnemonic[32];
    char operands[96];
} LureDisarmInstruction;

typedef struct {
    const char *filename;
    LureBinaryFormat format;
    LureArch arch;
    uint64_t entry;

    LureDisarmSection *sections;
    size_t section_count;

    LureDisarmFunction *functions;
    size_t function_count;
} LureDisarmImage;

typedef struct {
    size_t max_preview_bytes;
    int show_sections;
    int show_functions;
    int show_preview;
} LureDisarmOptions;

typedef struct {
    const char *name;
    LureDisarmStatus (*run)(const LureDisarmImage *image, void *user_data);
    void *user_data;
} LureDisarmPass;

const char *lure_disarm_status_string(LureDisarmStatus status);
const char *lure_disarm_format_string(LureBinaryFormat format);
const char *lure_disarm_arch_string(LureArch arch);

LureDisarmOptions lure_disarm_default_options(void);
LureDisarmStatus lure_disarm_load(const char *filename, LureDisarmImage *image);
void lure_disarm_free(LureDisarmImage *image);
LureDisarmStatus lure_disarm_read_bytes(const LureDisarmImage *image,
                                        uint64_t file_offset,
                                        void *buf,
                                        size_t size);

void lure_disarm_print_summary(const LureDisarmImage *image);
void lure_disarm_print_sections(const LureDisarmImage *image);
void lure_disarm_print_functions(const LureDisarmImage *image);
LureDisarmStatus lure_disarm_print_preview(const LureDisarmImage *image,
                                           const LureDisarmOptions *options);

LureDisarmStatus lure_disarm_run_passes(const LureDisarmImage *image,
                                        const LureDisarmPass *passes,
                                        size_t pass_count);

int lure_disarm_run_cli(const char *filename, const LureDisarmOptions *options);
int lure_disarm_main(int argc, char **argv);

#endif
