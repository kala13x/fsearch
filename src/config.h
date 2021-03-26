/*
 *  src/config.h
 *
 *  This source is part of "fsearch" project
 *  2015-2020  Sun Dro (f4tb0y@protonmail.com)
 * 
 * Program configuration and search criteria
 */

#ifndef __FSEARCH_CONFIG_H__
#define __FSEARCH_CONFIG_H__

#include <sys/types.h>

#ifdef __linux__ 
#include <linux/limits.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef NAME_MAX
#define NAME_MAX 256
#endif

#ifndef LINE_MAX
#define LINE_MAX 8192
#endif

#define FSEARCH_INFO_LEN 128
#define FSEARCH_TIME_LEN 12
#define FSEARCH_SIZE_LEN 10
#define FSEARCH_PERM_LEN 9

typedef enum {
    fsearch_regular_file = (1 << 0),
    fsearch_block_device = (1 << 1),
    fsearch_char_device = (1 << 2),
    fsearch_directory = (1 << 3),
    fsearch_symlink = (1 << 4),
    fsearch_socket = (1 << 5),
    fsearch_pipe = (1 << 6)
} fsearch_type_e;

typedef struct fsearch_cfg_ 
{
    /* FSearch context */
    char last_directory[PATH_MAX];  // Last printed directory path
    char directory[PATH_MAX];       // Target directory path
    char file_name[NAME_MAX];       // Needed file name (First regex token if using regex)
    char output[PATH_MAX];          // Output file path
    const char *exec_name;          // Name of executable file (same as argv[0])

    /* Search criteria */
    int permissions;                // Needed file permissions
    int link_count;                 // Needed file link count
    int file_types;                 // Needed file types
    int file_size;                  // Needed file size
    int criteria;                   // Count of search criteria

    /* Flags */
    int *interrupted;               // Interrupt flag
    int indentation;                // Ident using tabs
    int recursive:1;                // Recursive search
    int use_regex:1;                // Regex flag
    int is_found:1;                 // Status flag
    int verbose:1;                  // Verbose flag
} fsearch_cfg_t;

int fsearch_parse_args(fsearch_cfg_t *pcfg, int argc, char *argv[]);
void fsearch_print_usage(const char *name);

#endif /* __FSEARCH_CONFIG_H__ */