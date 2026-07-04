#ifndef LURE_CFG_H
#define LURE_CFG_H

#include "lure_disarm.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    LURE_CFG_OK = 0,
    LURE_CFG_ERR_IO,
    LURE_CFG_ERR_FORMAT,
    LURE_CFG_ERR_NOMEM
} LureCfgStatus;

typedef enum {
    LURE_CFG_OUTPUT_TEXT = 0,
    LURE_CFG_OUTPUT_FACTS,
    LURE_CFG_OUTPUT_DOT
} LureCfgOutputFormat;

typedef enum {
    LURE_CFG_EDGE_UNKNOWN = 0,
    LURE_CFG_EDGE_FALLTHROUGH,
    LURE_CFG_EDGE_BRANCH,
    LURE_CFG_EDGE_CALL,
    LURE_CFG_EDGE_RETURN
} LureCfgEdgeKind;

typedef struct {
    char name[128];
    uint64_t address;
    uint64_t size;
    size_t entry_block;
    size_t first_block;
    size_t block_count;
} LureCfgFunction;

typedef struct {
    size_t id;
    size_t function_index;
    uint64_t start;
    uint64_t end;
    size_t first_edge;
    size_t edge_count;
    int decoded;
    int synthetic;
} LureCfgBlock;

typedef struct {
    size_t source_block;
    size_t target_block;
    LureCfgEdgeKind kind;
    int resolved;
} LureCfgEdge;

typedef struct {
    LureCfgOutputFormat output_format;
    int include_functions;
    int include_blocks;
    int include_edges;
} LureCfgOptions;

typedef struct {
    const LureDisarmImage *image;
    LureCfgFunction *functions;
    size_t function_count;
    LureCfgBlock *blocks;
    size_t block_count;
    LureCfgEdge *edges;
    size_t edge_count;
    int decoder_required;
} LureCfgGraph;

const char *lure_cfg_status_string(LureCfgStatus status);
const char *lure_cfg_output_format_string(LureCfgOutputFormat format);
const char *lure_cfg_edge_kind_string(LureCfgEdgeKind kind);

LureCfgOptions lure_cfg_default_options(void);
LureCfgStatus lure_cfg_build(const LureDisarmImage *image,
                             const LureCfgOptions *options,
                             LureCfgGraph *graph);
void lure_cfg_free(LureCfgGraph *graph);
void lure_cfg_print(FILE *stream,
                    const LureCfgGraph *graph,
                    const LureCfgOptions *options);

int lure_cfg_run_cli(const char *filename, const LureCfgOptions *options);
int lure_cfg_main(int argc, char **argv);

#endif
