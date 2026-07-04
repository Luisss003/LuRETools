#include "lure_pcode.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_at(FILE *file, uint64_t offset, void *buf, size_t size) {
    if (fseek(file, (long)offset, SEEK_SET) != 0) {
        return 0;
    }

    return fread(buf, 1, size, file) == size;
}

static void init_varnode(LurePcodeVarnode *varnode,
                         LurePcodeSpace space,
                         uint64_t offset,
                         uint32_t size,
                         const char *name) {
    memset(varnode, 0, sizeof(*varnode));
    varnode->space = space;
    varnode->offset = offset;
    varnode->size = size;
    snprintf(varnode->name, sizeof(varnode->name), "%s", name ? name : "");
}

static LurePcodeStatus append_stub_instruction(LurePcodeProgram *program,
                                               const LureDisarmSection *section,
                                               uint64_t address,
                                               uint64_t file_offset,
                                               const uint8_t *bytes,
                                               size_t byte_count) {
    LurePcodeInstruction *new_instructions;
    LurePcodeOp *new_ops;
    LurePcodeInstruction *instruction;
    LurePcodeOp *op;
    size_t next_instruction_count = program->instruction_count + 1;
    size_t next_op_count = program->op_count + 1;

    new_instructions = realloc(program->instructions,
                               next_instruction_count * sizeof(*new_instructions));
    if (!new_instructions) {
        return LURE_PCODE_ERR_NOMEM;
    }

    program->instructions = new_instructions;
    new_ops = realloc(program->ops, next_op_count * sizeof(*new_ops));
    if (!new_ops) {
        return LURE_PCODE_ERR_NOMEM;
    }

    program->ops = new_ops;

    instruction = &program->instructions[program->instruction_count];
    memset(instruction, 0, sizeof(*instruction));
    snprintf(instruction->section, sizeof(instruction->section), "%s", section->name);
    instruction->address = address;
    instruction->file_offset = file_offset;
    instruction->byte_count = byte_count;
    memcpy(instruction->bytes, bytes, byte_count);
    instruction->first_op = program->op_count;
    instruction->op_count = 1;

    op = &program->ops[program->op_count];
    memset(op, 0, sizeof(*op));
    op->address = address;
    op->sequence = 0;
    op->opcode = LURE_PCODE_OP_CALLOTHER;
    snprintf(op->mnemonic, sizeof(op->mnemonic), "%s", "decode_pending");
    op->input_count = 3;
    init_varnode(&op->inputs[0],
                 LURE_PCODE_SPACE_CONSTANT,
                 file_offset,
                 sizeof(file_offset),
                 "file_offset");
    init_varnode(&op->inputs[1],
                 LURE_PCODE_SPACE_CONSTANT,
                 byte_count,
                 sizeof(byte_count),
                 "byte_count");
    init_varnode(&op->inputs[2],
                 LURE_PCODE_SPACE_RAM,
                 address,
                 (uint32_t)byte_count,
                 "raw_bytes");

    program->instruction_count = next_instruction_count;
    program->op_count = next_op_count;
    return LURE_PCODE_OK;
}

static void print_quoted(FILE *stream, const char *value) {
    fputc('"', stream);

    for (const char *cursor = value ? value : ""; *cursor; cursor++) {
        if (*cursor == '"' || *cursor == '\\') {
            fputc('\\', stream);
        }
        fputc(*cursor, stream);
    }

    fputc('"', stream);
}

static void print_bytes(FILE *stream, const uint8_t *bytes, size_t byte_count) {
    for (size_t i = 0; i < byte_count; i++) {
        fprintf(stream, "%02x%s", bytes[i], i + 1 == byte_count ? "" : " ");
    }
}

static void print_varnode_text(FILE *stream, const LurePcodeVarnode *varnode) {
    fprintf(stream,
            "%s:0x%lx:%u",
            lure_pcode_space_string(varnode->space),
            (unsigned long)varnode->offset,
            varnode->size);

    if (varnode->name[0]) {
        fprintf(stream, " <%s>", varnode->name);
    }
}

static void print_sections_facts(FILE *stream, const LureDisarmImage *image) {
    for (size_t i = 0; i < image->section_count; i++) {
        const LureDisarmSection *section = &image->sections[i];
        char flags[4];

        flags[0] = section->readable ? 'r' : '-';
        flags[1] = section->writable ? 'w' : '-';
        flags[2] = section->executable ? 'x' : '-';
        flags[3] = '\0';

        fprintf(stream, "section(");
        print_quoted(stream, section->name);
        fprintf(stream,
                ",%lu,%lu,%lu,",
                (unsigned long)section->vaddr,
                (unsigned long)section->file_offset,
                (unsigned long)section->size);
        print_quoted(stream, flags);
        fprintf(stream, ").\n");
    }
}

static void print_functions_facts(FILE *stream, const LureDisarmImage *image) {
    for (size_t i = 0; i < image->function_count; i++) {
        const LureDisarmFunction *function = &image->functions[i];

        fprintf(stream, "function(");
        print_quoted(stream, function->name);
        fprintf(stream,
                ",%lu,%lu).\n",
                (unsigned long)function->address,
                (unsigned long)function->size);
    }
}

static void print_pcode_facts(FILE *stream,
                              const LurePcodeProgram *program,
                              const LurePcodeOptions *options) {
    const LureDisarmImage *image = program->image;

    fprintf(stream, "binary(");
    print_quoted(stream, image->filename);
    fprintf(stream, ",");
    print_quoted(stream, lure_disarm_format_string(image->format));
    fprintf(stream, ",");
    print_quoted(stream, lure_disarm_arch_string(image->arch));
    fprintf(stream, ",%lu).\n", (unsigned long)image->entry);

    fprintf(stream, "pcode_backend(");
    print_quoted(stream, lure_pcode_backend_string(program->backend));
    fprintf(stream, ").\n");

    if (options->include_sections) {
        print_sections_facts(stream, image);
    }

    if (options->include_functions) {
        print_functions_facts(stream, image);
    }

    for (size_t i = 0; i < program->instruction_count; i++) {
        const LurePcodeInstruction *instruction = &program->instructions[i];

        fprintf(stream,
                "pcode_record(%lu,%lu,",
                (unsigned long)instruction->address,
                (unsigned long)instruction->file_offset);
        print_quoted(stream, instruction->section);
        fprintf(stream, ",%zu).\n", instruction->byte_count);

        if (options->include_raw_bytes) {
            for (size_t j = 0; j < instruction->byte_count; j++) {
                fprintf(stream,
                        "raw_byte(%lu,%lu,%u).\n",
                        (unsigned long)(instruction->address + j),
                        (unsigned long)(instruction->file_offset + j),
                        instruction->bytes[j]);
            }
        }

        for (size_t j = 0; j < instruction->op_count; j++) {
            const LurePcodeOp *op = &program->ops[instruction->first_op + j];

            fprintf(stream,
                    "pcode_op(%lu,%zu,",
                    (unsigned long)op->address,
                    op->sequence);
            print_quoted(stream, lure_pcode_opcode_string(op->opcode));
            fprintf(stream, ",");
            print_quoted(stream, op->mnemonic);
            fprintf(stream, ").\n");

            for (size_t k = 0; k < op->input_count; k++) {
                const LurePcodeVarnode *input = &op->inputs[k];

                fprintf(stream,
                        "pcode_input(%lu,%zu,%zu,",
                        (unsigned long)op->address,
                        op->sequence,
                        k);
                print_quoted(stream, lure_pcode_space_string(input->space));
                fprintf(stream,
                        ",%lu,%u,",
                        (unsigned long)input->offset,
                        input->size);
                print_quoted(stream, input->name);
                fprintf(stream, ").\n");
            }
        }
    }
}

static void print_pcode_text(FILE *stream,
                             const LurePcodeProgram *program,
                             const LurePcodeOptions *options) {
    const LureDisarmImage *image = program->image;

    fprintf(stream, "[lure-pcode]\n");
    fprintf(stream, "file: %s\n", image->filename ? image->filename : "<unknown>");
    fprintf(stream, "format: %s\n", lure_disarm_format_string(image->format));
    fprintf(stream, "arch: %s\n", lure_disarm_arch_string(image->arch));
    fprintf(stream, "entry: 0x%lx\n", (unsigned long)image->entry);
    fprintf(stream, "backend: %s\n", lure_pcode_backend_string(program->backend));
    fprintf(stream, "records: %zu\n", program->instruction_count);
    fprintf(stream, "ops: %zu\n", program->op_count);
    fprintf(stream, "; stub backend emits CALLOTHER decode_pending records until a decoder is added\n");

    if (options->include_functions && image->function_count > 0) {
        fprintf(stream, "\n[Functions]\n");
        for (size_t i = 0; i < image->function_count; i++) {
            const LureDisarmFunction *function = &image->functions[i];
            fprintf(stream,
                    "0x%012lx 0x%08lx %s\n",
                    (unsigned long)function->address,
                    (unsigned long)function->size,
                    function->name);
        }
    }

    fprintf(stream, "\n[P-code Records]\n");
    for (size_t i = 0; i < program->instruction_count; i++) {
        const LurePcodeInstruction *instruction = &program->instructions[i];

        fprintf(stream,
                "%s:0x%012lx file+0x%08lx bytes[%zu]=",
                instruction->section,
                (unsigned long)instruction->address,
                (unsigned long)instruction->file_offset,
                instruction->byte_count);
        print_bytes(stream, instruction->bytes, instruction->byte_count);
        fprintf(stream, "\n");

        for (size_t j = 0; j < instruction->op_count; j++) {
            const LurePcodeOp *op = &program->ops[instruction->first_op + j];

            fprintf(stream,
                    "  %zu: %s %s(",
                    op->sequence,
                    lure_pcode_opcode_string(op->opcode),
                    op->mnemonic);
            for (size_t k = 0; k < op->input_count; k++) {
                print_varnode_text(stream, &op->inputs[k]);
                fprintf(stream, "%s", k + 1 == op->input_count ? "" : ", ");
            }
            fprintf(stream, ")\n");
        }
    }
}

static int parse_size(const char *value, size_t *out) {
    char *end = NULL;
    unsigned long parsed;

    errno = 0;
    parsed = strtoul(value, &end, 0);
    if (errno != 0 || !end || *end != '\0') {
        return 0;
    }

    *out = (size_t)parsed;
    return 1;
}

static void print_lure_pcode_usage(FILE *stream) {
    fprintf(stream, "Usage: luretools pcode [options] <binary>\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  --text              print readable records (default)\n");
    fprintf(stream, "  --facts             print Datalog-style facts\n");
    fprintf(stream, "  --max-bytes <n>     limit bytes per executable section (default: 128)\n");
    fprintf(stream, "  --all-bytes         process every byte in executable sections\n");
    fprintf(stream, "  --no-functions      omit function metadata from output\n");
    fprintf(stream, "  --no-sections       omit section metadata from facts output\n");
    fprintf(stream, "  --no-raw-bytes      omit raw_byte facts\n");
}

const char *lure_pcode_status_string(LurePcodeStatus status) {
    switch (status) {
        case LURE_PCODE_OK:
            return "ok";
        case LURE_PCODE_ERR_IO:
            return "I/O error";
        case LURE_PCODE_ERR_FORMAT:
            return "invalid input";
        case LURE_PCODE_ERR_NOMEM:
            return "out of memory";
        default:
            return "unknown error";
    }
}

const char *lure_pcode_backend_string(LurePcodeBackend backend) {
    switch (backend) {
        case LURE_PCODE_BACKEND_STUB:
            return "stub";
        default:
            return "unknown";
    }
}

const char *lure_pcode_output_format_string(LurePcodeOutputFormat format) {
    switch (format) {
        case LURE_PCODE_OUTPUT_TEXT:
            return "text";
        case LURE_PCODE_OUTPUT_FACTS:
            return "facts";
        default:
            return "unknown";
    }
}

const char *lure_pcode_space_string(LurePcodeSpace space) {
    switch (space) {
        case LURE_PCODE_SPACE_CONSTANT:
            return "const";
        case LURE_PCODE_SPACE_REGISTER:
            return "register";
        case LURE_PCODE_SPACE_UNIQUE:
            return "unique";
        case LURE_PCODE_SPACE_RAM:
            return "ram";
        case LURE_PCODE_SPACE_UNRESOLVED:
            return "unresolved";
        default:
            return "unknown";
    }
}

const char *lure_pcode_opcode_string(LurePcodeOpCode opcode) {
    switch (opcode) {
        case LURE_PCODE_OP_COPY:
            return "COPY";
        case LURE_PCODE_OP_LOAD:
            return "LOAD";
        case LURE_PCODE_OP_STORE:
            return "STORE";
        case LURE_PCODE_OP_BRANCH:
            return "BRANCH";
        case LURE_PCODE_OP_CBRANCH:
            return "CBRANCH";
        case LURE_PCODE_OP_CALL:
            return "CALL";
        case LURE_PCODE_OP_RETURN:
            return "RETURN";
        case LURE_PCODE_OP_INT_ADD:
            return "INT_ADD";
        case LURE_PCODE_OP_INT_SUB:
            return "INT_SUB";
        case LURE_PCODE_OP_INT_MULT:
            return "INT_MULT";
        case LURE_PCODE_OP_INT_AND:
            return "INT_AND";
        case LURE_PCODE_OP_INT_OR:
            return "INT_OR";
        case LURE_PCODE_OP_INT_XOR:
            return "INT_XOR";
        case LURE_PCODE_OP_CALLOTHER:
            return "CALLOTHER";
        case LURE_PCODE_OP_UNIMPLEMENTED:
            return "UNIMPLEMENTED";
        default:
            return "UNKNOWN";
    }
}

LurePcodeOptions lure_pcode_default_options(void) {
    LurePcodeOptions options;

    options.backend = LURE_PCODE_BACKEND_STUB;
    options.output_format = LURE_PCODE_OUTPUT_TEXT;
    options.max_bytes_per_section = 128;
    options.include_sections = 1;
    options.include_functions = 1;
    options.include_raw_bytes = 1;

    return options;
}

LurePcodeStatus lure_pcode_build(const LureDisarmImage *image,
                                 const LurePcodeOptions *options,
                                 LurePcodeProgram *program) {
    LurePcodeOptions effective_options = options ? *options : lure_pcode_default_options();
    FILE *file;

    if (!image || !image->filename || !program) {
        return LURE_PCODE_ERR_FORMAT;
    }

    memset(program, 0, sizeof(*program));
    program->image = image;
    program->backend = effective_options.backend;

    file = fopen(image->filename, "rb");
    if (!file) {
        return LURE_PCODE_ERR_IO;
    }

    for (size_t i = 0; i < image->section_count; i++) {
        const LureDisarmSection *section = &image->sections[i];
        uint64_t limit;
        uint64_t cursor = 0;

        if (!section->executable || !section->readable || section->size == 0) {
            continue;
        }

        limit = section->size;
        if (effective_options.max_bytes_per_section > 0 &&
            limit > effective_options.max_bytes_per_section) {
            limit = effective_options.max_bytes_per_section;
        }

        while (cursor < limit) {
            uint8_t bytes[8];
            size_t chunk = (limit - cursor) < sizeof(bytes) ?
                           (size_t)(limit - cursor) :
                           sizeof(bytes);
            LurePcodeStatus status;

            if (!read_at(file, section->file_offset + cursor, bytes, chunk)) {
                fclose(file);
                lure_pcode_free(program);
                return LURE_PCODE_ERR_IO;
            }

            status = append_stub_instruction(program,
                                             section,
                                             section->vaddr + cursor,
                                             section->file_offset + cursor,
                                             bytes,
                                             chunk);
            if (status != LURE_PCODE_OK) {
                fclose(file);
                lure_pcode_free(program);
                return status;
            }

            cursor += chunk;
        }
    }

    fclose(file);
    return LURE_PCODE_OK;
}

void lure_pcode_free(LurePcodeProgram *program) {
    if (!program) {
        return;
    }

    free(program->instructions);
    free(program->ops);
    memset(program, 0, sizeof(*program));
}

void lure_pcode_print(FILE *stream,
                      const LurePcodeProgram *program,
                      const LurePcodeOptions *options) {
    LurePcodeOptions effective_options = options ? *options : lure_pcode_default_options();

    if (!stream || !program || !program->image) {
        return;
    }

    if (effective_options.output_format == LURE_PCODE_OUTPUT_FACTS) {
        print_pcode_facts(stream, program, &effective_options);
    } else {
        print_pcode_text(stream, program, &effective_options);
    }
}

int lure_pcode_run_cli(const char *filename, const LurePcodeOptions *options) {
    LureDisarmImage image;
    LurePcodeProgram program;
    LurePcodeOptions effective_options = options ? *options : lure_pcode_default_options();
    LureDisarmStatus disarm_status = lure_disarm_load(filename, &image);
    LurePcodeStatus pcode_status;

    if (disarm_status != LURE_DISARM_OK) {
        fprintf(stderr, "lure-pcode: %s: %s\n", filename, lure_disarm_status_string(disarm_status));
        return 1;
    }

    pcode_status = lure_pcode_build(&image, &effective_options, &program);
    if (pcode_status != LURE_PCODE_OK) {
        fprintf(stderr, "lure-pcode: %s\n", lure_pcode_status_string(pcode_status));
        lure_disarm_free(&image);
        return 1;
    }

    lure_pcode_print(stdout, &program, &effective_options);

    lure_pcode_free(&program);
    lure_disarm_free(&image);
    return 0;
}

int lure_pcode_main(int argc, char **argv) {
    LurePcodeOptions options = lure_pcode_default_options();
    const char *filename = NULL;

    if (argc < 2) {
        print_lure_pcode_usage(stderr);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_lure_pcode_usage(stdout);
            return 0;
        } else if (strcmp(argv[i], "--text") == 0) {
            options.output_format = LURE_PCODE_OUTPUT_TEXT;
        } else if (strcmp(argv[i], "--facts") == 0) {
            options.output_format = LURE_PCODE_OUTPUT_FACTS;
        } else if (strcmp(argv[i], "--max-bytes") == 0) {
            if (i + 1 >= argc || !parse_size(argv[i + 1], &options.max_bytes_per_section)) {
                fprintf(stderr, "lure-pcode: --max-bytes requires a non-negative integer\n");
                return 1;
            }
            i++;
        } else if (strcmp(argv[i], "--all-bytes") == 0) {
            options.max_bytes_per_section = 0;
        } else if (strcmp(argv[i], "--no-functions") == 0) {
            options.include_functions = 0;
        } else if (strcmp(argv[i], "--no-sections") == 0) {
            options.include_sections = 0;
        } else if (strcmp(argv[i], "--no-raw-bytes") == 0) {
            options.include_raw_bytes = 0;
        } else if (!filename) {
            filename = argv[i];
        } else {
            fprintf(stderr, "lure-pcode: unexpected argument: %s\n", argv[i]);
            print_lure_pcode_usage(stderr);
            return 1;
        }
    }

    if (!filename) {
        print_lure_pcode_usage(stderr);
        return 1;
    }

    return lure_pcode_run_cli(filename, &options);
}
