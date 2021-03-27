/*
 *  src/config.c
 *
 *  This source is part of "fsearch" project
 *  2015-2020  Sun Dro (f4tb0y@protonmail.com)
 * 
 * Program configuration and search criteria
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include "config.h"

extern char *optarg;

#define FSEARCH_VERSION_MAX     0
#define FSEARCH_VERSION_MIN     1
#define FSEARCH_BUILD_NUMBER    6

static void fsearch_config_init(fsearch_cfg_t *pcfg, const char *pname)
{
    pcfg->exec_name = pname;
    pcfg->last_directory[0] = '\0';
    pcfg->file_name[0] = '\0';
    pcfg->output[0] = '\0';

    pcfg->directory[0] = '.';
    pcfg->directory[1] = '/';
    pcfg->directory[2] = '\0';

    pcfg->permissions = 0;
    pcfg->file_types = 0;
    pcfg->link_count = -1;
    pcfg->file_size = -1;
    pcfg->indentation = 0;

    pcfg->recursive = 0;
    pcfg->use_regex = 0;
    pcfg->is_found = 0;
    pcfg->criteria = 0;
    pcfg->verbose = 0;
}

static int fsearch_get_ftypes(const char *pname, const char *ctypes)
{
    size_t i, len = strlen(ctypes);
    int ntypes = 0;

    for (i = 0; i < len; i++)
    {
        switch (ctypes[i])
        {
            case 'b': ntypes |= fsearch_block_device; break;
            case 'c': ntypes |= fsearch_char_device; break;
            case 'd': ntypes |= fsearch_directory; break;
            case 'f': ntypes |= fsearch_regular_file; break;
            case 'l': ntypes |= fsearch_symlink; break;
            case 'p': ntypes |= fsearch_pipe; break;
            case 's': ntypes |= fsearch_socket; break;
            default:
            {
                fprintf(stderr, "%s: '%c': Invalid file type\n", pname, ctypes[i]);
                return -1;
            }
        }
    }

    return ntypes;
}

static int fsearch_get_part_perm(const char *part)
{
    int perm = 0;
    if (part[0] == 'r') perm += 4;
    else if (part[0] != '-') return -1;

    if (part[1] == 'w') perm += 2;
    else if (part[1] != '-') return -1;

    if (part[2] == 'x') perm += 1;
    else if (part[2] != '-') return -1;

    return perm;
}

static int fsearch_get_permissions(const char *pname, const char *chmod)
{
    size_t len = strlen(chmod);
    if (len != FSEARCH_PERM_LEN)
    {
        fprintf(stderr, "%s: '%s': Invalid permission\n", pname, chmod);
        return -1;   
    }

    int owner = fsearch_get_part_perm(chmod);
    int group = fsearch_get_part_perm(&chmod[3]);
    int others = fsearch_get_part_perm(&chmod[6]);

    if (owner < 0 || group < 0 || others < 0)
    {
        fprintf(stderr, "%s: '%s': Invalid permission\n", pname, chmod);
        return -1;
    }

    char buff[FSEARCH_PERM_LEN + 1]; // +1 for null terminator
    snprintf(buff, sizeof(buff), "%d%d%d", owner, group, others);

    return atoi(buff);
}

static int fsearch_get_filename(fsearch_cfg_t *pcfg, const char *optarg)
{
    size_t i, length = strnlen(optarg, sizeof(pcfg->file_name));

    /* Change file name to lowercase to support case sensitivity */
    for (i = 0; i < length; i++) pcfg->file_name[i] = tolower(optarg[i]);
    pcfg->file_name[length] = '\0';

    /* Get regex usage flag only once */
    return (strstr(pcfg->file_name, "+") != NULL) ? 1 : 0;
}

void fsearch_print_usage(const char *name)
{
    printf("==========================================================\n");
    printf("Advanced File Search - Version: %d.%d build %d (%s)\n", 
        FSEARCH_VERSION_MAX, FSEARCH_VERSION_MIN, FSEARCH_BUILD_NUMBER, __DATE__);
    printf("==========================================================\n");

    int i, len = strlen(name) + 6;
    char whitespace[len + 1];

    for (i = 0; i < len; i++) whitespace[i] = ' ';
    whitespace[len] = 0;

    printf("Usage: %s [-i <indentation>] [-f <file_name>] [-b <file_size>]\n", name);
    printf(" %s [-p <permissions>] [-t <file_type>] [-o <file_path>]\n", whitespace);
    printf(" %s [-d <target_path>] [-l <link_count>] [-r] [-v] [-h]\n\n", whitespace);

    printf("Options are:\n");
    printf("  -d <target_path>    # Target directory path\n");
    printf("  -i <indentation>    # Ident using tabs with specified size\n");
    printf("  -o <file_path>      # Write output in a specified file\n");
    printf("  -f <file_name>      # Target file name (case insensitive)\n");
    printf("  -b <file_size>      # Target file size in bytes\n");
    printf("  -t <file_type>      # Target file type (*)\n");
    printf("  -l <link_count>     # Target file link count\n");
    printf("  -p <permissions>    # Target file permissions (e.g. 'rwxr-xr--')\n");
    printf("  -r                  # Recursive search target directory\n");
    printf("  -v                  # Display additional information (verbose) \n");
    printf("  -h                  # Displays version and usage information\n\n");

    printf("File types (*):\n");
    printf("   b: block device\n");
    printf("   c: character device\n");
    printf("   d: directory\n");
    printf("   f: regular file\n");
    printf("   l: symbolic link\n");
    printf("   p: pipe\n");
    printf("   s: socket\n\n");

    printf("Notes:\n");
    printf("   1) <filename> option is supporting the following regular expression: +\n");
    printf("   2) <file_type> option is supporting one and more file types like: -t ldb\n\n");
    printf("Example: %s -d targetDirectoryPath -f lost+file -b 100 -t b\n\n", name);
}

int fsearch_parse_args(fsearch_cfg_t *pcfg, int argc, char *argv[])
{
    fsearch_config_init(pcfg, argv[0]);
    int opt = 0;

    while ((opt = getopt(argc, argv, "d:i:o:b:l:t:p:f:r1:v1:h1")) != -1) 
    {
        switch (opt)
        {
            case 'i':
                pcfg->indentation = atol(optarg);
                break;
            case 'b':
                pcfg->file_size = atol(optarg);
                pcfg->criteria++;
                break;
            case 'l':
                pcfg->link_count = atol(optarg);
                pcfg->criteria++;
                break;
            case 'd':
                snprintf(pcfg->directory, sizeof(pcfg->directory), "%s", optarg);
                pcfg->criteria++;
                break;
            case 'o':
                snprintf(pcfg->output, sizeof(pcfg->output), "%s", optarg);
                break;
            case 't':
                pcfg->file_types = fsearch_get_ftypes(argv[0], optarg);
                pcfg->criteria++;
                break;
            case 'p':
                pcfg->permissions = fsearch_get_permissions(argv[0], optarg);
                pcfg->criteria++;
                break;
            case 'f':
                pcfg->use_regex = fsearch_get_filename(pcfg, optarg);
                pcfg->criteria++;
                break;
            case 'r':
                pcfg->recursive = 1;
                break;
            case 'v':
                pcfg->verbose = 1;
                break;
            case 'h':
            default:
                return 0;
        }
    }

    /* Validate opts */
    if (pcfg->permissions < 0 || 
        pcfg->file_types < 0) 
            return 0;

    return 1;
}
