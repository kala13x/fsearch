/*
 *  src/fsearch.c
 *
 *  This source is part of "fsearch" project
 *  2015-2020  Sun Dro (f4tb0y@protonmail.com)
 * 
 * The main function of the fsearch software
 */

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include "config.h"
#include "search.h"

static int g_interrupted = 0;

void signal_callback(int sig)
{
    printf("\nInterrupted with signal: %d\n", sig);
    __sync_lock_test_and_set(&g_interrupted, 1);    
}

int main(int argc, char *argv[])
{
    fsearch_cfg_t config;
    config.interrupted = &g_interrupted;

    /* Register interrupt signal (Ctrl+C) */
    signal(SIGINT, signal_callback);

    /* Parse command line arguments */
    if (!fsearch_parse_args(&config, argc, argv))
    {
        fsearch_print_usage(argv[0]);
        return 1;
    }

    /* Start recursive search target files */
    int status = fsearch_search_files(&config, config.directory);
    if (status < 0) /* Can not open target directory */
    {
        fsearch_log_error(&config, config.directory);
        return 1;
    }

    /* Cant find any file */
    if (!config.is_found) 
    {
        printf("No file found\n");
        return 1;
    }

    /* Thats all */
    return 0;
}