#ifndef LURE_PCODE_H
#define LURE_PCODE_H

#include "lure_disarm.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    LURE_PCODE_OK = 0,
    LURE_PCODE_ERR_IO,
    LURE_PCODE_ERR_FORMAT,
    LURE_PCODE_ERR_NOMEM
} LurePcodeStatus;

typedef enum {
    LURE_PCODE_BACKEND_STUB = 0
} LurePcodeBackend;

typedef enum {
    LURE_PCODE_OUTPUT_TEXT = 0,
    LURE_PCODE_OUTPUT_FACTS
} LurePcodeOutputFormat;

typedef enum {
    LURE_PCODE_SPACE_CONSTANT = 0,
    LURE_PCODE_SPACE_REGISTER,
    LURE_PCODE_SPACE_UNIQUE,
    LURE_PCODE_SPACE_RAM,
    LURE_PCODE_SPACE_UNRESOLVED
} LurePcodeSpace;

typedef enum {
    LURE_PCODE_OP_COPY = 0,
    LURE_PCODE_OP_LOAD,
    LURE_PCODE_OP_STORE,
    LURE_PCODE_OP_BRANCH,
    LURE_PCODE_OP_CBRANCH,
    LURE_PCODE_OP_CALL,
    LURE_PCODE_OP_RETURN,
    LURE_PCODE_OP_INT_ADD,
    LURE_PCODE_OP_INT_SUB,
    LURE_PCODE_OP_INT_MULT,
    LURE_PCODE_OP_INT_AND,
    LURE_PCODE_OP_INT_OR,
    LURE_PCODE_OP_INT_XOR,
    LURE_PCODE_OP_CALLOTHER,
    LURE_PCODE_OP_UNIMPLEMENTED
} LurePcodeOpCode;

typedef struct {
    LurePcodeSpace space;
    uint64_t offset;
    uint32_t size;
    char name[64];
} LurePcodeVarnode;

typedef struct {
    uint64_t address;
    size_t sequence;
    LurePcodeOpCode opcode;
    char mnemonic[32];
    LurePcodeVarnode output;
    LurePcodeVarnode inputs[4];
    size_t input_count;
} LurePcodeOp;

typedef struct {
    char section[64];
    uint64_t address;
    uint64_t file_offset;
    uint8_t bytes[16];
    size_t byte_count;
    size_t first_op;
    size_t op_count;
} LurePcodeInstruction;

typedef struct {
    LurePcodeBackend backend;
    LurePcodeOutputFormat output_format;
    size_t max_bytes_per_section;
    int include_sections;
    int include_functions;
    int include_raw_bytes;
} LurePcodeOptions;

typedef struct {
    const LureDisarmImage *image;
    LurePcodeBackend backend;
    LurePcodeInstruction *instructions;
    size_t instruction_count;
    LurePcodeOp *ops;
    size_t op_count;
} LurePcodeProgram;

const char *lure_pcode_status_string(LurePcodeStatus status);
const char *lure_pcode_backend_string(LurePcodeBackend backend);
const char *lure_pcode_output_format_string(LurePcodeOutputFormat format);
const char *lure_pcode_space_string(LurePcodeSpace space);
const char *lure_pcode_opcode_string(LurePcodeOpCode opcode);

LurePcodeOptions lure_pcode_default_options(void);
LurePcodeStatus lure_pcode_build(const LureDisarmImage *image,
                                 const LurePcodeOptions *options,
                                 LurePcodeProgram *program);
void lure_pcode_free(LurePcodeProgram *program);
void lure_pcode_print(FILE *stream,
                      const LurePcodeProgram *program,
                      const LurePcodeOptions *options);

int lure_pcode_run_cli(const char *filename, const LurePcodeOptions *options);
int lure_pcode_main(int argc, char **argv);

#endif
