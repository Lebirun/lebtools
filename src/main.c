#include <stdio.h>
#include <string.h>

#include "version.h"
#include "lpkg.h"

static void print_help(FILE *stream)
{
    fprintf(stream, "Usage: lebtools <command> [options]\n");
    fprintf(stream, "       lebtools --version\n");
    fprintf(stream, "\n");
    fprintf(stream, "Commands:\n");
    fprintf(stream, "  lpkg       Create an LPKG package\n");
    fprintf(stream, "  help       Show this help\n");
    fprintf(stream, "\n");
    fprintf(stream, "Run 'lebtools <command> --help' for command help.\n");
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        print_help(stdout);
        return 0;
    }

    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0) {
        printf("lebtools %s\n", LEBTOOLS_VERSION);
        return 0;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0 ||
        strcmp(argv[1], "help") == 0) {
        print_help(stdout);
        return 0;
    }

    if (strcmp(argv[1], "lpkg") == 0)
        return lpkg_main(argc - 1, argv + 1);

    fprintf(stderr, "Error: unknown command '%s'\n", argv[1]);
    print_help(stderr);
    return 1;
}
