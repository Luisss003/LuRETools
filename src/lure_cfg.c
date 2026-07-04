#include "lure_cfg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int add_seed_function(LureCfgGraph *graph,
                             const char *name,
                             uint64_t address,
                             uint64_t size) {
    LureCfgFunction *function;
    LureCfgBlock *block;
    size_t function_index = graph->function_count;
    size_t block_index = graph->block_count;

    function = &graph->functions[function_index];
    memset(function, 0, sizeof(*function));
    snprintf(function->name, sizeof(function->name), "%s", name && name[0] ? name : "<unnamed>");
    function->address = address;
    function->size = size;
    function->entry_block = block_index;
    function->first_block = block_index;
    function->block_count = 1;

    block = &graph->blocks[block_index];
    memset(block, 0, sizeof(*block));
    block->id = block_index;
    block->function_index = function_index;
    block->start = address;
    block->end = size > 0 ? address + size : address;
    block->first_edge = graph->edge_count;
    block->edge_count = 0;
    block->decoded = 0;
    block->synthetic = 1;

    graph->function_count++;
    graph->block_count++;
    graph->decoder_required = 1;
    return 1;
}

static size_t count_executable_sections(const LureDisarmImage *image) {
    size_t count = 0;

    for (size_t i = 0; i < image->section_count; i++) {
        const LureDisarmSection *section = &image->sections[i];

        if (section->executable && section->readable && section->size > 0) {
            count++;
        }
    }

    return count;
}

static void print_functions_text(FILE *stream, const LureCfgGraph *graph) {
    if (graph->function_count == 0) {
        return;
    }

    fprintf(stream, "\n[Functions]\n");
    fprintf(stream, "%-4s %-14s %-10s %-8s %s\n", "id", "address", "size", "blocks", "name");

    for (size_t i = 0; i < graph->function_count; i++) {
        const LureCfgFunction *function = &graph->functions[i];

        fprintf(stream,
                "%-4zu 0x%012lx 0x%08lx %-8zu %s\n",
                i,
                (unsigned long)function->address,
                (unsigned long)function->size,
                function->block_count,
                function->name);
    }
}

static void print_blocks_text(FILE *stream, const LureCfgGraph *graph) {
    if (graph->block_count == 0) {
        return;
    }

    fprintf(stream, "\n[Basic Blocks]\n");
    fprintf(stream, "%-4s %-4s %-14s %-14s %-8s %s\n",
            "id",
            "fn",
            "start",
            "end",
            "edges",
            "state");

    for (size_t i = 0; i < graph->block_count; i++) {
        const LureCfgBlock *block = &graph->blocks[i];

        fprintf(stream,
                "%-4zu %-4zu 0x%012lx 0x%012lx %-8zu %s%s\n",
                block->id,
                block->function_index,
                (unsigned long)block->start,
                (unsigned long)block->end,
                block->edge_count,
                block->decoded ? "decoded" : "decode_pending",
                block->synthetic ? ",synthetic" : "");
    }
}

static void print_edges_text(FILE *stream, const LureCfgGraph *graph) {
    if (graph->edge_count == 0) {
        fprintf(stream, "\n[Edges]\n");
        fprintf(stream, "; no edges recovered yet: instruction decoding is required\n");
        return;
    }

    fprintf(stream, "\n[Edges]\n");
    fprintf(stream, "%-6s %-6s %-12s %s\n", "src", "dst", "kind", "state");

    for (size_t i = 0; i < graph->edge_count; i++) {
        const LureCfgEdge *edge = &graph->edges[i];

        fprintf(stream,
                "%-6zu %-6zu %-12s %s\n",
                edge->source_block,
                edge->target_block,
                lure_cfg_edge_kind_string(edge->kind),
                edge->resolved ? "resolved" : "unresolved");
    }
}

static void print_cfg_text(FILE *stream,
                           const LureCfgGraph *graph,
                           const LureCfgOptions *options) {
    const LureDisarmImage *image = graph->image;

    fprintf(stream, "[lure-cfg]\n");
    fprintf(stream, "file: %s\n", image->filename ? image->filename : "<unknown>");
    fprintf(stream, "format: %s\n", lure_disarm_format_string(image->format));
    fprintf(stream, "arch: %s\n", lure_disarm_arch_string(image->arch));
    fprintf(stream, "entry: 0x%lx\n", (unsigned long)image->entry);
    fprintf(stream, "functions: %zu\n", graph->function_count);
    fprintf(stream, "blocks: %zu\n", graph->block_count);
    fprintf(stream, "edges: %zu\n", graph->edge_count);
    fprintf(stream, "state: %s\n", graph->decoder_required ? "seed_only_decoder_required" : "decoded");

    if (options->include_functions) {
        print_functions_text(stream, graph);
    }

    if (options->include_blocks) {
        print_blocks_text(stream, graph);
    }

    if (options->include_edges) {
        print_edges_text(stream, graph);
    }
}

static void print_cfg_facts(FILE *stream,
                            const LureCfgGraph *graph,
                            const LureCfgOptions *options) {
    const LureDisarmImage *image = graph->image;

    fprintf(stream, "binary(");
    print_quoted(stream, image->filename);
    fprintf(stream, ",");
    print_quoted(stream, lure_disarm_format_string(image->format));
    fprintf(stream, ",");
    print_quoted(stream, lure_disarm_arch_string(image->arch));
    fprintf(stream, ",%lu).\n", (unsigned long)image->entry);

    fprintf(stream, "cfg_state(");
    print_quoted(stream, graph->decoder_required ? "seed_only_decoder_required" : "decoded");
    fprintf(stream, ").\n");

    if (options->include_functions) {
        for (size_t i = 0; i < graph->function_count; i++) {
            const LureCfgFunction *function = &graph->functions[i];

            fprintf(stream, "cfg_function(%zu,", i);
            print_quoted(stream, function->name);
            fprintf(stream,
                    ",%lu,%lu,%zu).\n",
                    (unsigned long)function->address,
                    (unsigned long)function->size,
                    function->entry_block);
        }
    }

    if (options->include_blocks) {
        for (size_t i = 0; i < graph->block_count; i++) {
            const LureCfgBlock *block = &graph->blocks[i];

            fprintf(stream,
                    "cfg_block(%zu,%zu,%lu,%lu,",
                    block->id,
                    block->function_index,
                    (unsigned long)block->start,
                    (unsigned long)block->end);
            print_quoted(stream, block->decoded ? "decoded" : "decode_pending");
            fprintf(stream,
                    ",%d).\n",
                    block->synthetic);
        }
    }

    if (options->include_edges) {
        for (size_t i = 0; i < graph->edge_count; i++) {
            const LureCfgEdge *edge = &graph->edges[i];

            fprintf(stream,
                    "cfg_edge(%zu,%zu,",
                    edge->source_block,
                    edge->target_block);
            print_quoted(stream, lure_cfg_edge_kind_string(edge->kind));
            fprintf(stream, ",%d).\n", edge->resolved);
        }
    }
}

static void print_cfg_dot(FILE *stream, const LureCfgGraph *graph) {
    fprintf(stream, "digraph lure_cfg {\n");
    fprintf(stream, "  graph [label=\"lure-cfg seed graph\", labelloc=t];\n");
    fprintf(stream, "  node [shape=box, fontname=\"monospace\"];\n");

    for (size_t i = 0; i < graph->function_count; i++) {
        const LureCfgFunction *function = &graph->functions[i];

        fprintf(stream, "  subgraph cluster_%zu {\n", i);
        fprintf(stream, "    label=");
        print_quoted(stream, function->name);
        fprintf(stream, ";\n");

        for (size_t j = 0; j < function->block_count; j++) {
            const LureCfgBlock *block = &graph->blocks[function->first_block + j];

            fprintf(stream,
                    "    b%zu [label=\"b%zu\\n0x%lx..0x%lx\\ndecode_pending\"];\n",
                    block->id,
                    block->id,
                    (unsigned long)block->start,
                    (unsigned long)block->end);
        }

        fprintf(stream, "  }\n");
    }

    for (size_t i = 0; i < graph->edge_count; i++) {
        const LureCfgEdge *edge = &graph->edges[i];

        fprintf(stream,
                "  b%zu -> b%zu [label=",
                edge->source_block,
                edge->target_block);
        print_quoted(stream, lure_cfg_edge_kind_string(edge->kind));
        fprintf(stream, "];\n");
    }

    fprintf(stream, "}\n");
}

static void print_lure_cfg_usage(FILE *stream) {
    fprintf(stream, "Usage: luretools cfg [options] <binary>\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  --text          print readable CFG seed records (default)\n");
    fprintf(stream, "  --facts         print Datalog-style CFG facts\n");
    fprintf(stream, "  --dot           print Graphviz DOT\n");
    fprintf(stream, "  --no-functions  omit function list from text/facts output\n");
    fprintf(stream, "  --no-blocks     omit block list from text/facts output\n");
    fprintf(stream, "  --no-edges      omit edge list from text/facts output\n");
}

const char *lure_cfg_status_string(LureCfgStatus status) {
    switch (status) {
        case LURE_CFG_OK:
            return "ok";
        case LURE_CFG_ERR_IO:
            return "I/O error";
        case LURE_CFG_ERR_FORMAT:
            return "invalid input";
        case LURE_CFG_ERR_NOMEM:
            return "out of memory";
        default:
            return "unknown error";
    }
}

const char *lure_cfg_output_format_string(LureCfgOutputFormat format) {
    switch (format) {
        case LURE_CFG_OUTPUT_TEXT:
            return "text";
        case LURE_CFG_OUTPUT_FACTS:
            return "facts";
        case LURE_CFG_OUTPUT_DOT:
            return "dot";
        default:
            return "unknown";
    }
}

const char *lure_cfg_edge_kind_string(LureCfgEdgeKind kind) {
    switch (kind) {
        case LURE_CFG_EDGE_FALLTHROUGH:
            return "fallthrough";
        case LURE_CFG_EDGE_BRANCH:
            return "branch";
        case LURE_CFG_EDGE_CALL:
            return "call";
        case LURE_CFG_EDGE_RETURN:
            return "return";
        case LURE_CFG_EDGE_UNKNOWN:
        default:
            return "unknown";
    }
}

LureCfgOptions lure_cfg_default_options(void) {
    LureCfgOptions options;

    options.output_format = LURE_CFG_OUTPUT_TEXT;
    options.include_functions = 1;
    options.include_blocks = 1;
    options.include_edges = 1;

    return options;
}

LureCfgStatus lure_cfg_build(const LureDisarmImage *image,
                             const LureCfgOptions *options,
                             LureCfgGraph *graph) {
    size_t seed_count;
    (void)options;

    if (!image || !graph) {
        return LURE_CFG_ERR_FORMAT;
    }

    memset(graph, 0, sizeof(*graph));
    graph->image = image;

    seed_count = image->function_count > 0 ? image->function_count : count_executable_sections(image);
    if (seed_count == 0) {
        graph->decoder_required = 1;
        return LURE_CFG_OK;
    }

    graph->functions = calloc(seed_count, sizeof(*graph->functions));
    graph->blocks = calloc(seed_count, sizeof(*graph->blocks));
    if (!graph->functions || !graph->blocks) {
        lure_cfg_free(graph);
        return LURE_CFG_ERR_NOMEM;
    }

    if (image->function_count > 0) {
        for (size_t i = 0; i < image->function_count; i++) {
            const LureDisarmFunction *function = &image->functions[i];

            add_seed_function(graph, function->name, function->address, function->size);
        }
    } else {
        for (size_t i = 0; i < image->section_count; i++) {
            const LureDisarmSection *section = &image->sections[i];

            if (!section->executable || !section->readable || section->size == 0) {
                continue;
            }

            add_seed_function(graph, section->name, section->vaddr, section->size);
        }
    }

    return LURE_CFG_OK;
}

void lure_cfg_free(LureCfgGraph *graph) {
    if (!graph) {
        return;
    }

    free(graph->functions);
    free(graph->blocks);
    free(graph->edges);
    memset(graph, 0, sizeof(*graph));
}

void lure_cfg_print(FILE *stream,
                    const LureCfgGraph *graph,
                    const LureCfgOptions *options) {
    LureCfgOptions effective_options = options ? *options : lure_cfg_default_options();

    if (!stream || !graph || !graph->image) {
        return;
    }

    if (effective_options.output_format == LURE_CFG_OUTPUT_FACTS) {
        print_cfg_facts(stream, graph, &effective_options);
    } else if (effective_options.output_format == LURE_CFG_OUTPUT_DOT) {
        print_cfg_dot(stream, graph);
    } else {
        print_cfg_text(stream, graph, &effective_options);
    }
}

int lure_cfg_run_cli(const char *filename, const LureCfgOptions *options) {
    LureDisarmImage image;
    LureCfgGraph graph;
    LureCfgOptions effective_options = options ? *options : lure_cfg_default_options();
    LureDisarmStatus disarm_status = lure_disarm_load(filename, &image);
    LureCfgStatus cfg_status;

    if (disarm_status != LURE_DISARM_OK) {
        fprintf(stderr, "lure-cfg: %s: %s\n", filename, lure_disarm_status_string(disarm_status));
        return 1;
    }

    cfg_status = lure_cfg_build(&image, &effective_options, &graph);
    if (cfg_status != LURE_CFG_OK) {
        fprintf(stderr, "lure-cfg: %s\n", lure_cfg_status_string(cfg_status));
        lure_disarm_free(&image);
        return 1;
    }

    lure_cfg_print(stdout, &graph, &effective_options);

    lure_cfg_free(&graph);
    lure_disarm_free(&image);
    return 0;
}

int lure_cfg_main(int argc, char **argv) {
    LureCfgOptions options = lure_cfg_default_options();
    const char *filename = NULL;

    if (argc < 2) {
        print_lure_cfg_usage(stderr);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_lure_cfg_usage(stdout);
            return 0;
        } else if (strcmp(argv[i], "--text") == 0) {
            options.output_format = LURE_CFG_OUTPUT_TEXT;
        } else if (strcmp(argv[i], "--facts") == 0) {
            options.output_format = LURE_CFG_OUTPUT_FACTS;
        } else if (strcmp(argv[i], "--dot") == 0) {
            options.output_format = LURE_CFG_OUTPUT_DOT;
        } else if (strcmp(argv[i], "--no-functions") == 0) {
            options.include_functions = 0;
        } else if (strcmp(argv[i], "--no-blocks") == 0) {
            options.include_blocks = 0;
        } else if (strcmp(argv[i], "--no-edges") == 0) {
            options.include_edges = 0;
        } else if (!filename) {
            filename = argv[i];
        } else {
            fprintf(stderr, "lure-cfg: unexpected argument: %s\n", argv[i]);
            print_lure_cfg_usage(stderr);
            return 1;
        }
    }

    if (!filename) {
        print_lure_cfg_usage(stderr);
        return 1;
    }

    return lure_cfg_run_cli(filename, &options);
}
