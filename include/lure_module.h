#ifndef LURE_MODULE_H
#define LURE_MODULE_H

#include <stddef.h>
#include <stdio.h>

typedef int (*LureModuleMain)(int argc, char **argv);

typedef struct {
    const char *name;
    const char *summary;
    const char *usage;
    LureModuleMain run;
} LureModule;

const LureModule *lure_modules(size_t *count);
const LureModule *lure_find_module(const char *name);
void lure_print_usage(FILE *stream, const char *program);

#endif
