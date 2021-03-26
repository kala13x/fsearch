/*
 *  src/search.c
 *
 *  This source is part of "fsearch" project
 *  2015-2020  Sun Dro (f4tb0y@protonmail.com)
 * 
 * Implementation of recursive file search and
 * path drawing with a nicely formatted tree
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "search.h"

#define FSEARCH_CHECK_FL(types, flag) (((types) & (flag)) == (flag))
#define FSEARCH_FULL_PATH_LEN   (PATH_MAX + NAME_MAX + 1) // +1 for slash

#define FSEARCH_STR_BOLD        "\033[1m"
#define FSEARCH_STR_RESET       "\033[0m"

void fsearch_log_error(fsearch_cfg_t *pcfg, const char *path)
{
    fprintf(stderr, "%s: '%s': %s\n", 
        pcfg->exec_name, path, strerror(errno));
}

static int fsearch_check_name(fsearch_cfg_t *pcfg, const char *entry)
{
    size_t name_len = strlen(pcfg->file_name);
    if (!name_len) return 1;

    /* Copy file name otherwise strtok will damage variable */
    char file_name[name_len + 1];
    strcpy(file_name, pcfg->file_name);

    size_t i, entry_len = strlen(entry);
    char entry_name[entry_len + 1];

    /* Make file name lowercase to support case sensitivity */
    for (i = 0; i < entry_len; i++) entry_name[i] = tolower(entry[i]);

    entry_name[entry_len] = '\0';
    char *saveptr = NULL;

    /* There is no regex operator in search criteria */
    if (!pcfg->use_regex) return !strcmp(pcfg->file_name, entry_name);

    char *token = strtok_r(file_name, "+", &saveptr);
    if (token == NULL) return 0;

    char *offset = strstr(entry_name, token);
    if (offset == NULL) return 0; /* Token not found */

    while (token != NULL)
    {
        /* Compare first token and file name offset */
        size_t token_len = strlen(token);
        if (strncmp(offset, token, token_len)) return 0;

        /* Move offset after first token */
        offset += token_len - 1;
        char skip_char = *offset;
        i = 0;

        /* Skip last characer of first token */
        while (offset[i] == skip_char) i++;
        offset += i;
        
        /* Get next token */
        token = strtok_r(NULL, "+", &saveptr);
    }

    /* All possible occurrences passed */
    return 1;
}

static int fsearch_check_size(fsearch_cfg_t *pcfg, size_t size)
{
    if (pcfg->file_size < 0) return 1;
    return pcfg->file_size == size ? 1 : 0;
}

static int fsearch_check_links(fsearch_cfg_t *pcfg, nlink_t links)
{
    if (pcfg->link_count < 0) return 1;
    return pcfg->link_count == links ? 1 : 0;
}

static char fsearch_get_type(fsearch_cfg_t *pcfg, mode_t mode)
{
    switch (mode & S_IFMT)
    {
        case S_IFBLK: return 'b';
        case S_IFCHR: return 'c';
        case S_IFDIR: return 'd';
        case S_IFIFO: return 'p';
        case S_IFLNK: return 'l';
        case S_IFREG: return 'f';
        case S_IFSOCK: return 's';
        default: break;
    }

    return 'u'; // Unknown file format
}

static int fsearch_check_type(fsearch_cfg_t *pcfg, mode_t mode)
{
    if (!pcfg->file_types) return 1;
    char type = fsearch_get_type(pcfg, mode);

    switch (type)
    {
        case 'b':
            if (FSEARCH_CHECK_FL(pcfg->file_types, fsearch_block_device)) return 1; 
            break;
        case 'c':
            if (FSEARCH_CHECK_FL(pcfg->file_types, fsearch_char_device)) return 1;  
            break;
        case 'd':
            if (FSEARCH_CHECK_FL(pcfg->file_types, fsearch_directory)) return 1;  
            break;
        case 'p':
            if (FSEARCH_CHECK_FL(pcfg->file_types, fsearch_pipe)) return 1;  
            break;
        case 'l':
            if (FSEARCH_CHECK_FL(pcfg->file_types, fsearch_symlink)) return 1;  
            break;
        case 'f':
            if (FSEARCH_CHECK_FL(pcfg->file_types, fsearch_regular_file)) return 1;  
            break;
        case 's':
            if (FSEARCH_CHECK_FL(pcfg->file_types, fsearch_socket)) return 1;  
            break;
        default: break;
    }

    return 0; // Unknown file format
}

static size_t fsearch_get_depth(fsearch_cfg_t *pcfg, const char *path)
{
    char *saveptr_found, *saveptr_last;
    char found[FSEARCH_FULL_PATH_LEN];
    char last[FSEARCH_FULL_PATH_LEN];
    size_t depth = 0; 

    strcpy(last, pcfg->last_directory);
    strcpy(found, path);

    char *foundptr = strtok_r(found, "/", &saveptr_found);
    char *lastptr = strtok_r(last, "/", &saveptr_last);

    while (foundptr != NULL && lastptr != NULL)
    {
        if (strcmp(foundptr, lastptr)) return depth;
        depth += pcfg->indentation;

        foundptr = strtok_r(NULL, "/", &saveptr_found);
        lastptr = strtok_r(NULL, "/", &saveptr_last);  
    }

    return depth;
}

static int fsearch_get_chmod(char *output, size_t size, mode_t mode)
{
    int owner = (mode & S_IRUSR) ? 4 : 0;
    owner += (mode & S_IWUSR) ? 2 : 0;
    owner += (mode & S_IXUSR) ? 1 : 0;

    int group = (mode & S_IRGRP) ? 4 : 0;
    group += (mode & S_IWGRP) ? 2 : 0;
    group += (mode & S_IXGRP) ? 1 : 0;

    int others = (mode & S_IROTH) ? 4 : 0;
    others += (mode & S_IWOTH) ? 2 : 0;
    others += (mode & S_IXOTH) ? 1 : 0;

    return snprintf(output, size, "%d%d%d", owner, group, others);
}

static int fsearch_get_chmodstr(char *output, size_t size, mode_t mode)
{
    if (size < FSEARCH_PERM_LEN + 1) return 0;

    output[0] = (mode & S_IRUSR) ? 'r' : '-';
    output[1] = (mode & S_IWUSR) ? 'w' : '-';
    output[2] = (mode & S_IXUSR) ? 'x' : '-';

    output[3] = (mode & S_IRGRP) ? 'r' : '-';
    output[4] = (mode & S_IWGRP) ? 'w' : '-';
    output[5] = (mode & S_IXGRP) ? 'x' : '-';

    output[6] = (mode & S_IROTH) ? 'r' : '-';
    output[7] = (mode & S_IWOTH) ? 'w' : '-';
    output[8] = (mode & S_IXOTH) ? 'x' : '-';

    output[FSEARCH_PERM_LEN] = 0;
    return FSEARCH_PERM_LEN;
}

static int fsearch_check_permissions(fsearch_cfg_t *pcfg, mode_t mode)
{
    if (!pcfg->permissions) return 1;
    char buff[FSEARCH_PERM_LEN + 1];

    fsearch_get_chmod(buff, sizeof(buff), mode);
    return (atoi(buff) == pcfg->permissions) ? 1 : 0;
}

static int fsearch_get_info(fsearch_cfg_t *pcfg, struct stat *pstat, char *output, size_t size)
{
    if (!pcfg->verbose)
    {
        output[0] = '\0';
        return 0;
    }

    /* Create chmod string (e.g. 'rwxr-xr--') */
    char chmod[FSEARCH_PERM_LEN + 1];
    fsearch_get_chmodstr(chmod, sizeof(chmod), pstat->st_mode);
    char type = fsearch_get_type(pcfg, pstat->st_mode);

    /* Get username from uid */
    struct passwd *pws = getpwuid(pstat->st_uid);
    const char *uname = pws ? pws->pw_name : "";

    /* Get group name from gid */
    struct group *grp = getgrgid(pstat->st_gid);
    const char *gname = grp ? grp->gr_name : "";

    /* Get last access time */
    char stime[FSEARCH_TIME_LEN + 1];
    const char *ptime = ctime(&pstat->st_atime);

    /* Get only day and time from full date */
    strncpy(stime, &ptime[4], FSEARCH_TIME_LEN);
    stime[FSEARCH_TIME_LEN] = '\0';

    /* Create size string with indentation */
    char sizebuf[FSEARCH_SIZE_LEN + 1];
    snprintf(sizebuf, sizeof(sizebuf), "%.10ld", pstat->st_size);
    size_t i;

    /* Fill zero indentations with spaces */
    for (i = 0; i < FSEARCH_SIZE_LEN; i++)
    {
        if (sizebuf[i] != '0') break;
        sizebuf[i] = ' ';
    }

    sizebuf[FSEARCH_SIZE_LEN] = '\0';
    type = type == 'f' ? '-' : type;

    return snprintf(output, size, "%c%s  %lu  %s  %s  %s [%s] ",
        type, chmod, pstat->st_nlink, uname, gname, sizebuf, stime);
}

static void fsearch_printf(fsearch_cfg_t *pcfg, const char *pFmt, ...)
{
    va_list args;
    char line[LINE_MAX];

    va_start(args, pFmt);
    vsnprintf(line, sizeof(line), pFmt, args);
    va_end(args);

    if (pcfg->output[0] != '\0')
    {
        FILE *fp = fopen(pcfg->output, "a");
        if (fp != NULL)
        {
            fprintf(fp, "%s\n", line);
            fclose(fp);
        }
    }

    printf("%s\n", line);
}

static void fsearch_display_path(fsearch_cfg_t *pcfg, struct stat *pstat, const char *path)
{
    if (!pcfg->indentation)
    {
        char sinfo[FSEARCH_INFO_LEN + 1];
        fsearch_get_info(pcfg, pstat, sinfo, sizeof(sinfo));
        fsearch_printf(pcfg, "%s%s", sinfo, path);
        return;
    }

    /* Get last printed directory depth to draw tree   */
    size_t match_depth = fsearch_get_depth(pcfg, path);

    /* Copy path to another variable because strtok_r() 
       will insert null terminator afrer each token */
    char found[FSEARCH_FULL_PATH_LEN];
    strcpy(found, path);

    char *saveptr = NULL;
    size_t i, tabs = 0;

    char *ptr = strtok_r(found, "/", &saveptr);
    while (ptr != NULL)
    {
        /* Continue tree drawing from last matching directory */
        if ((!match_depth && tabs) || (match_depth && tabs >= match_depth))
        {
            /* Feel depth with '-' character */
            char suffix[tabs + 1];
            for (i = 0; i < tabs; i++) suffix[i] = '-';
            suffix[tabs] = '\0';

            /* Get next token from the path and if its last, 
               we must display it with the bold text */
            char *next = strtok_r(NULL, "/", &saveptr);
            if (!pcfg->criteria || next != NULL) fsearch_printf(pcfg, "|%s%s", suffix, ptr);
            else fsearch_printf(pcfg, "|%s%s%s%s", suffix, FSEARCH_STR_BOLD, ptr, FSEARCH_STR_RESET);

            tabs += pcfg->indentation;
            ptr = next;
            continue;
        }
        else if (!match_depth)
        {
            /* Display directory from the begining of tree */
            fsearch_printf(pcfg, "%s", ptr);
        }

        /* Get next sub directory */
        ptr = strtok_r(NULL, "/", &saveptr);
        tabs += pcfg->indentation;
    }
}

int fsearch_search_files(fsearch_cfg_t *pcfg, const char *pdirectory)
{
    DIR *pdir = opendir(pdirectory);
    if (pdir == NULL) return -1;

    size_t dir_len = strlen(pdirectory);
    struct dirent *entry = NULL;

    while ((entry = readdir(pdir)) != NULL && !__sync_add_and_fetch(pcfg->interrupted, 0))
    {
        /* Found an entry, but ignore . and .. */
        if(strcmp(".", entry->d_name) == 0 ||
           strcmp("..", entry->d_name) == 0)
           continue;

        struct stat statbuf;
        char path[PATH_MAX];

        /* Dont add slash twice if directory already contains slash character at the end */
        const char *slash = pdirectory[dir_len-1] != '/' ? "/" : "";
        snprintf(path, sizeof(path), "%s%s%s", pdirectory, slash, entry->d_name);

        if (lstat(path, &statbuf) < 0)
        {
            fsearch_log_error(pcfg, path);
            continue;
        }

        if (fsearch_check_name(pcfg, entry->d_name) &&
            fsearch_check_size(pcfg, statbuf.st_size) &&
            fsearch_check_type(pcfg, statbuf.st_mode) &&
            fsearch_check_links(pcfg, statbuf.st_nlink) &&
            fsearch_check_permissions(pcfg, statbuf.st_mode))
        {
            /* Display path with formatted tree */
            fsearch_display_path(pcfg, &statbuf, path);
            pcfg->is_found = 1;

            /* Update status */
            if (S_ISDIR(statbuf.st_mode)) snprintf(pcfg->last_directory, sizeof(pcfg->last_directory), "%s", path);
            else snprintf(pcfg->last_directory, sizeof(pcfg->last_directory), "%s", pdirectory);
        }

        /* Recursive search */
        if (pcfg->recursive && 
            S_ISDIR(statbuf.st_mode) && 
            fsearch_search_files(pcfg, path) < 0)
                fsearch_log_error(pcfg, pdirectory);
    }

    closedir(pdir);
    return 1;
}