#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

#include "lpkg.h"

#define LPKG_MAGIC "LPKG"
#define LPKG_VERSION 1
#define LPKG_MAX_FILES 65535UL
#define LPKG_MAX_PATH_LENGTH 65535UL
#define LPKG_MAX_FILE_SIZE 4294967295UL

static void print_usage(FILE *stream)
{
    fprintf(stream, "Usage: lebtools lpkg <output.lpkg> <install_path> <local_file> [<install_path> <local_file> ...]\n");
    fprintf(stream, "Example: lebtools lpkg hello.lpkg /bin/hello ./hello\n");
}

static int write_u8(FILE *file, uint8_t value)
{
    return fwrite(&value, 1, 1, file) == 1 ? 0 : -1;
}

static int write_u16le(FILE *file, uint16_t value)
{
    uint8_t buffer[2];

    buffer[0] = (uint8_t)(value & 0xffU);
    buffer[1] = (uint8_t)((value >> 8) & 0xffU);
    return fwrite(buffer, 1, 2, file) == 2 ? 0 : -1;
}

static int write_u32le(FILE *file, uint32_t value)
{
    uint8_t buffer[4];

    buffer[0] = (uint8_t)(value & 0xffUL);
    buffer[1] = (uint8_t)((value >> 8) & 0xffUL);
    buffer[2] = (uint8_t)((value >> 16) & 0xffUL);
    buffer[3] = (uint8_t)((value >> 24) & 0xffUL);
    return fwrite(buffer, 1, 4, file) == 4 ? 0 : -1;
}

static long get_file_size(FILE *file)
{
    long size;

    if (fseek(file, 0, SEEK_END) != 0)
        return -1;
    size = ftell(file);
    if (size < 0)
        return -1;
    if (fseek(file, 0, SEEK_SET) != 0)
        return -1;
    return size;
}

static int copy_file(FILE *output, FILE *input, long file_size)
{
    unsigned char buffer[16384];
    long remaining;
    size_t chunk_size;

    remaining = file_size;
    while (remaining > 0) {
        chunk_size = remaining > (long)sizeof(buffer)
            ? sizeof(buffer) : (size_t)remaining;
        if (fread(buffer, 1, chunk_size, input) != chunk_size)
            return -1;
        if (fwrite(buffer, 1, chunk_size, output) != chunk_size)
            return -1;
        remaining -= (long)chunk_size;
    }
    return 0;
}

static int discard_package(FILE *output, const char *output_path)
{
    fclose(output);
    remove(output_path);
    return 1;
}

static int validate_install_paths(int count, char **install_paths)
{
    int i;
    size_t path_length;

    for (i = 0; i < count; i++) {
        path_length = strlen(install_paths[i]);
        if (path_length == 0 || install_paths[i][0] != '/') {
            fprintf(stderr, "Error: install path must be absolute: '%s'\n",
                    install_paths[i]);
            return 1;
        }
        if (path_length > LPKG_MAX_PATH_LENGTH) {
            fprintf(stderr, "Error: install path is too long: '%s'\n",
                    install_paths[i]);
            return 1;
        }
    }
    return 0;
}

static int pack_files(const char *output_path, int count, char **install_paths,
                      char **local_paths)
{
    FILE *output;
    FILE *input;
    int i;
    long file_size;
    size_t path_length;

    if (validate_install_paths(count, install_paths) != 0)
        return 1;

    output = fopen(output_path, "wb");
    if (output == NULL) {
        fprintf(stderr, "Error: cannot open '%s' for writing\n", output_path);
        return 1;
    }

    if (fwrite(LPKG_MAGIC, 1, 4, output) != 4 ||
        write_u8(output, LPKG_VERSION) != 0 ||
        write_u16le(output, (uint16_t)count) != 0) {
        fprintf(stderr, "Error: write failed\n");
        return discard_package(output, output_path);
    }

    for (i = 0; i < count; i++) {
        path_length = strlen(install_paths[i]);
        input = fopen(local_paths[i], "rb");
        if (input == NULL) {
            fprintf(stderr, "Error: cannot open '%s'\n", local_paths[i]);
            return discard_package(output, output_path);
        }

        file_size = get_file_size(input);
        if (file_size < 0 || (unsigned long)file_size > LPKG_MAX_FILE_SIZE) {
            fprintf(stderr, "Error: invalid file size for '%s'\n",
                    local_paths[i]);
            fclose(input);
            return discard_package(output, output_path);
        }

        if (write_u16le(output, (uint16_t)path_length) != 0 ||
            fwrite(install_paths[i], 1, path_length, output) != path_length ||
            write_u32le(output, (uint32_t)file_size) != 0 ||
            copy_file(output, input, file_size) != 0) {
            fprintf(stderr, "Error: failed to package '%s'\n", local_paths[i]);
            fclose(input);
            return discard_package(output, output_path);
        }

        if (fclose(input) != 0) {
            fprintf(stderr, "Error: failed to close '%s'\n", local_paths[i]);
            return discard_package(output, output_path);
        }
        printf("  %s -> %s (%ld bytes)\n", local_paths[i], install_paths[i],
               file_size);
    }

    if (fclose(output) != 0) {
        fprintf(stderr, "Error: failed to close '%s'\n", output_path);
        remove(output_path);
        return 1;
    }
    printf("Created %s with %d file(s)\n", output_path, count);
    return 0;
}

int lpkg_main(int argc, char **argv)
{
    int file_count;
    int pair_arguments;
    char **install_paths;
    char **local_paths;
    int i;
    int result;

    if (argc == 2 && (strcmp(argv[1], "--help") == 0 ||
                      strcmp(argv[1], "-h") == 0)) {
        print_usage(stdout);
        return 0;
    }

    if (argc < 4) {
        print_usage(stderr);
        return 1;
    }

    pair_arguments = argc - 2;
    if (pair_arguments % 2 != 0) {
        fprintf(stderr, "Error: each file needs an <install_path> and a <local_file>\n");
        print_usage(stderr);
        return 1;
    }

    file_count = pair_arguments / 2;
    if ((unsigned long)file_count > LPKG_MAX_FILES) {
        fprintf(stderr, "Error: too many files\n");
        return 1;
    }

    install_paths = malloc(sizeof(*install_paths) * (size_t)file_count);
    local_paths = malloc(sizeof(*local_paths) * (size_t)file_count);
    if (install_paths == NULL || local_paths == NULL) {
        fprintf(stderr, "Error: out of memory\n");
        free(install_paths);
        free(local_paths);
        return 1;
    }

    for (i = 0; i < file_count; i++) {
        install_paths[i] = argv[2 + i * 2];
        local_paths[i] = argv[3 + i * 2];
    }

    result = pack_files(argv[1], file_count, install_paths, local_paths);
    free(install_paths);
    free(local_paths);
    return result;
}
