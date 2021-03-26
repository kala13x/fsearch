/*
 *  src/search.h
 *
 *  This source is part of "fsearch" project
 *  2015-2020  Sun Dro (f4tb0y@protonmail.com)
 * 
 * Implementation of recursive file search and
 * path drawing with a nicely formatted tree
 */

#ifndef __FSEARCH_SEARCH_H__
#define __FSEARCH_SEARCH_H__

#include "config.h"

void fsearch_log_error(fsearch_cfg_t *pcfg, const char *path);
int fsearch_search_files(fsearch_cfg_t *pcfg, const char *pdirectory);

#endif /* __FSEARCH_SEARCH_H__ */