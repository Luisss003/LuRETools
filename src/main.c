#include <stdio.h>
#include <string.h>

#include "lure_module.h"

int main(int argc, char *argv[]) {
    const LureModule *module;

    if (argc < 2) {
        lure_print_usage(stderr, argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        lure_print_usage(stdout, argv[0]);
        return 0;
    }

    module = lure_find_module(argv[1]);
    if (!module) {
        fprintf(stderr, "Unknown module: %s\n\n", argv[1]);
        lure_print_usage(stderr, argv[0]);
        return 1;
    }

    return module->run(argc - 1, argv + 1);
}
