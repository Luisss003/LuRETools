#include "lure_module.h"

#include "lure_cfg.h"
#include "lure_disarm.h"

#include <string.h>

static const LureModule modules[] = {
    {
        "disarm",
        "Load binary metadata and preview executable bytes",
        "disarm <binary>",
        lure_disarm_main,
    },
    {
        "cfg",
        "Seed function CFG records from binary metadata",
        "cfg [--text|--facts|--dot] <binary>",
        lure_cfg_main,
    },
};

const LureModule *lure_modules(size_t *count) {
    if (count) {
        *count = sizeof(modules) / sizeof(modules[0]);
    }

    return modules;
}

const LureModule *lure_find_module(const char *name) {
    size_t count;
    const LureModule *available = lure_modules(&count);

    if (!name) {
        return NULL;
    }

    for (size_t i = 0; i < count; i++) {
        if (strcmp(available[i].name, name) == 0) {
            return &available[i];
        }
    }

    return NULL;
}

void lure_print_usage(FILE *stream, const char *program) {
    size_t count;
    const LureModule *available = lure_modules(&count);

    fprintf(stream, "Usage:\n");
    fprintf(stream, "  %s <module> [args]\n\n", program);
    fprintf(stream, "Modules:\n");

    for (size_t i = 0; i < count; i++) {
        fprintf(stream,
                "  %-12s %s\n",
                available[i].name,
                available[i].summary);
        fprintf(stream,
                "  %-12s usage: %s\n",
                "",
                available[i].usage);
    }
}
