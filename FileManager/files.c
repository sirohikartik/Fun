#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <stdbool.h>
#include <ctype.h>

#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_DIM     "\033[2m"

/* --- Data structures --- */

typedef struct {
    char name[256];
    char full_path[PATH_MAX];
    bool is_dir;
    bool is_exec;
    off_t size;
    mode_t mode;
    time_t mtime;
} FileEntry;

typedef struct {
    FileEntry *items;
    size_t count;
    size_t capacity;
} EntryList;

/* --- Utility Functions --- */

void list_init(EntryList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void list_push(EntryList *list, const FileEntry *entry) {
    if (list->count >= list->capacity) {
        size_t new_cap = (list->capacity == 0) ? 16 : list->capacity * 2;
        FileEntry *new_items = realloc(list->items, new_cap * sizeof(FileEntry));
        if (!new_items) {
            perror("realloc failed");
            return;
        }
        list->items = new_items;
        list->capacity = new_cap;
    }
    list->items[list->count++] = *entry;
}

void list_free(EntryList *list) {
    if (list->items) {
        free(list->items);
        list->items = NULL;
    }
    list->count = 0;
    list->capacity = 0;
}

int compare_entries(const void *a, const void *b) {
    const FileEntry *ea = (const FileEntry *)a;
    const FileEntry *eb = (const FileEntry *)b;

    // Directories first, then alphabetical
    if (ea->is_dir != eb->is_dir) {
        return eb->is_dir - ea->is_dir;
    }
    return strcasecmp(ea->name, eb->name);
}

void format_size(off_t bytes, char *out, size_t out_len) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int idx = 0;
    double d_size = (double)bytes;

    while (d_size >= 1024.0 && idx < 4) {
        d_size /= 1024.0;
        idx++;
    }

    if (idx == 0) {
        snprintf(out, out_len, "%4lld B", (long long)bytes);
    } else {
        snprintf(out, out_len, "%6.1f %s", d_size, units[idx]);
    }
}

void format_mode(mode_t mode, char *out) {
    out[0] = S_ISDIR(mode) ? 'd' : (S_ISLNK(mode) ? 'l' : '-');
    out[1] = (mode & S_IRUSR) ? 'r' : '-';
    out[2] = (mode & S_IWUSR) ? 'w' : '-';
    out[3] = (mode & S_IXUSR) ? 'x' : '-';
    out[4] = (mode & S_IRGRP) ? 'r' : '-';
    out[5] = (mode & S_IWGRP) ? 'w' : '-';
    out[6] = (mode & S_IXGRP) ? 'x' : '-';
    out[7] = (mode & S_IROTH) ? 'r' : '-';
    out[8] = (mode & S_IWOTH) ? 'w' : '-';
    out[9] = (mode & S_IXOTH) ? 'x' : '-';
    out[10] = '\0';
}

void format_time(time_t t, char *out, size_t out_len) {
    struct tm *tm_info = localtime(&t);
    if (tm_info) {
        strftime(out, out_len, "%Y-%m-%d %H:%M", tm_info);
    } else {
        snprintf(out, out_len, "unknown");
    }
}

bool is_path_safe_to_delete(const char *path) {
    char resolved[PATH_MAX];
    if (!realpath(path, resolved)) {
        // If it doesn't resolve (e.g. doesn't exist), let standard syscall report error
        return true;
    }
    // Disallow root, home folder, or parent system directories
    if (strcmp(resolved, "/") == 0) return false;

    const char *home = getenv("HOME");
    if (home && strcmp(resolved, home) == 0) return false;

    return true;
}

/* --- Core File Management Features --- */

int cmd_list(const char *path, bool show_hidden) {
    const char *target = (path && strlen(path) > 0) ? path : ".";
    DIR *dp = opendir(target);
    if (!dp) {
        fprintf(stderr, COLOR_RED "Error opening directory '%s': %s" COLOR_RESET "\n", target, strerror(errno));
        return 1;
    }

    EntryList list;
    list_init(&list);

    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (!show_hidden && entry->d_name[0] == '.') {
            continue;
        }

        FileEntry fe;
        memset(&fe, 0, sizeof(fe));
        strncpy(fe.name, entry->d_name, sizeof(fe.name) - 1);

        snprintf(fe.full_path, sizeof(fe.full_path), "%s/%s", target, entry->d_name);

        struct stat st;
        if (lstat(fe.full_path, &st) == 0) {
            fe.size = st.st_size;
            fe.mode = st.st_mode;
            fe.mtime = st.st_mtime;
            fe.is_dir = S_ISDIR(st.st_mode);
            fe.is_exec = (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
        } else {
            fe.is_dir = (entry->d_type == DT_DIR);
        }

        list_push(&list, &fe);
    }
    closedir(dp);

    qsort(list.items, list.count, sizeof(FileEntry), compare_entries);

    printf("\n" COLOR_BOLD "Contents of %s (%zu items):" COLOR_RESET "\n", target, list.count);
    printf(COLOR_DIM "  PERMISSIONS   SIZE        MODIFIED         NAME" COLOR_RESET "\n");
    printf(COLOR_DIM "  ------------------------------------------------------------" COLOR_RESET "\n");

    for (size_t i = 0; i < list.count; i++) {
        char perm[12];
        char sz[16];
        char tm[32];

        format_mode(list.items[i].mode, perm);
        format_size(list.items[i].size, sz, sizeof(sz));
        format_time(list.items[i].mtime, tm, sizeof(tm));

        const char *name_color = COLOR_RESET;
        const char *suffix = "";
        if (list.items[i].is_dir) {
            name_color = COLOR_BLUE;
            suffix = "/";
        } else if (list.items[i].is_exec) {
            name_color = COLOR_GREEN;
            suffix = "*";
        }

        printf("  %-11s  %-9s  %-16s %s%s%s%s\n",
               perm, sz, tm, name_color, list.items[i].name, suffix, COLOR_RESET);
    }
    printf("\n");

    list_free(&list);
    return 0;
}

void tree_recursive(const char *path, const char *prefix, int depth, int max_depth) {
    if (depth >= max_depth) return;

    DIR *dp = opendir(path);
    if (!dp) return;

    EntryList list;
    list_init(&list);

    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        FileEntry fe;
        memset(&fe, 0, sizeof(fe));
        strncpy(fe.name, entry->d_name, sizeof(fe.name) - 1);
        snprintf(fe.full_path, sizeof(fe.full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (lstat(fe.full_path, &st) == 0) {
            fe.is_dir = S_ISDIR(st.st_mode);
        } else {
            fe.is_dir = (entry->d_type == DT_DIR);
        }
        list_push(&list, &fe);
    }
    closedir(dp);

    qsort(list.items, list.count, sizeof(FileEntry), compare_entries);

    for (size_t i = 0; i < list.count; i++) {
        bool is_last = (i == list.count - 1);
        const char *branch = is_last ? "└── " : "├── ";
        const char *color = list.items[i].is_dir ? COLOR_BLUE : COLOR_RESET;
        const char *slash = list.items[i].is_dir ? "/" : "";

        printf("%s%s%s%s%s%s\n", prefix, branch, color, list.items[i].name, slash, COLOR_RESET);

        if (list.items[i].is_dir) {
            char next_prefix[512];
            snprintf(next_prefix, sizeof(next_prefix), "%s%s", prefix, is_last ? "    " : "│   ");
            tree_recursive(list.items[i].full_path, next_prefix, depth + 1, max_depth);
        }
    }

    list_free(&list);
}

int cmd_tree(const char *path, int max_depth) {
    const char *target = (path && strlen(path) > 0) ? path : ".";
    printf("\n" COLOR_BLUE COLOR_BOLD "%s/" COLOR_RESET "\n", target);
    tree_recursive(target, "", 0, max_depth > 0 ? max_depth : 4);
    printf("\n");
    return 0;
}

int cmd_info(const char *path) {
    if (!path || strlen(path) == 0) {
        fprintf(stderr, COLOR_RED "Usage: info <path>" COLOR_RESET "\n");
        return 1;
    }

    struct stat st;
    if (lstat(path, &st) != 0) {
        fprintf(stderr, COLOR_RED "Error inspecting '%s': %s" COLOR_RESET "\n", path, strerror(errno));
        return 1;
    }

    char resolved[PATH_MAX];
    if (!realpath(path, resolved)) {
        strncpy(resolved, path, sizeof(resolved) - 1);
    }

    char sz[16], perm[12], tm_mod[32], tm_acc[32];
    format_size(st.st_size, sz, sizeof(sz));
    format_mode(st.st_mode, perm);
    format_time(st.st_mtime, tm_mod, sizeof(tm_mod));
    format_time(st.st_atime, tm_acc, sizeof(tm_acc));

    const char *type_str = "Regular File";
    if (S_ISDIR(st.st_mode)) type_str = "Directory";
    else if (S_ISLNK(st.st_mode)) type_str = "Symbolic Link";
    else if (S_ISCHR(st.st_mode)) type_str = "Character Device";
    else if (S_ISBLK(st.st_mode)) type_str = "Block Device";
    else if (S_ISFIFO(st.st_mode)) type_str = "FIFO / Named Pipe";
    else if (S_ISSOCK(st.st_mode)) type_str = "Socket";

    printf("\n" COLOR_BOLD "File Details:" COLOR_RESET "\n");
    printf("  " COLOR_CYAN "Path:" COLOR_RESET "         %s\n", resolved);
    printf("  " COLOR_CYAN "Type:" COLOR_RESET "         %s\n", type_str);
    printf("  " COLOR_CYAN "Size:" COLOR_RESET "         %lld bytes (%s)\n", (long long)st.st_size, sz);
    printf("  " COLOR_CYAN "Permissions:" COLOR_RESET "  %s (octal: %04o)\n", perm, st.st_mode & 07777);
    printf("  " COLOR_CYAN "Owner / Grp:" COLOR_RESET "  UID %d / GID %d\n", st.st_uid, st.st_gid);
    printf("  " COLOR_CYAN "Last Modified:" COLOR_RESET "%s\n", tm_mod);
    printf("  " COLOR_CYAN "Last Accessed:" COLOR_RESET "%s\n", tm_acc);
    printf("\n");
    return 0;
}

int cmd_cat(const char *path, size_t max_lines) {
    if (!path || strlen(path) == 0) {
        fprintf(stderr, COLOR_RED "Usage: cat <path>" COLOR_RESET "\n");
        return 1;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, COLOR_RED "Error opening file '%s': %s" COLOR_RESET "\n", path, strerror(errno));
        return 1;
    }

    // Safety: check if file seems binary
    unsigned char check_buf[512];
    size_t n = fread(check_buf, 1, sizeof(check_buf), f);
    for (size_t i = 0; i < n; i++) {
        if (check_buf[i] == 0) {
            printf(COLOR_YELLOW "[Binary file detected, skipping preview to protect terminal]" COLOR_RESET "\n");
            fclose(f);
            return 0;
        }
    }
    rewind(f);

    printf(COLOR_DIM "--- Begin of %s ---" COLOR_RESET "\n", path);
    char line[1024];
    size_t count = 0;
    while (fgets(line, sizeof(line), f)) {
        fputs(line, stdout);
        count++;
        if (max_lines > 0 && count >= max_lines) {
            printf(COLOR_YELLOW "\n... [truncated at %zu lines] ..." COLOR_RESET "\n", max_lines);
            break;
        }
    }
    printf(COLOR_DIM "\n--- End of file (%zu lines displayed) ---" COLOR_RESET "\n", count);
    fclose(f);
    return 0;
}

int cmd_touch(const char *path) {
    if (!path || strlen(path) == 0) {
        fprintf(stderr, COLOR_RED "Usage: touch <path>" COLOR_RESET "\n");
        return 1;
    }

    FILE *f = fopen(path, "a+b");
    if (!f) {
        fprintf(stderr, COLOR_RED "Error creating/touching '%s': %s" COLOR_RESET "\n", path, strerror(errno));
        return 1;
    }
    fclose(f);
    printf(COLOR_GREEN "✓ Touched '%s'" COLOR_RESET "\n", path);
    return 0;
}

int cmd_mkdir(const char *path) {
    if (!path || strlen(path) == 0) {
        fprintf(stderr, COLOR_RED "Usage: mkdir <path>" COLOR_RESET "\n");
        return 1;
    }

    if (mkdir(path, 0755) != 0) {
        fprintf(stderr, COLOR_RED "Error creating directory '%s': %s" COLOR_RESET "\n", path, strerror(errno));
        return 1;
    }
    printf(COLOR_GREEN "✓ Created directory '%s'" COLOR_RESET "\n", path);
    return 0;
}

int cmd_cp(const char *src, const char *dst) {
    if (!src || !dst || strlen(src) == 0 || strlen(dst) == 0) {
        fprintf(stderr, COLOR_RED "Usage: cp <source> <destination>" COLOR_RESET "\n");
        return 1;
    }

    struct stat st;
    if (stat(src, &st) != 0) {
        fprintf(stderr, COLOR_RED "Source file '%s' not found: %s" COLOR_RESET "\n", src, strerror(errno));
        return 1;
    }
    if (S_ISDIR(st.st_mode)) {
        fprintf(stderr, COLOR_RED "'%s' is a directory (recursive copy not supported in cp)" COLOR_RESET "\n", src);
        return 1;
    }

    // If destination is a directory, append src filename
    char final_dst[PATH_MAX];
    struct stat dst_st;
    if (stat(dst, &dst_st) == 0 && S_ISDIR(dst_st.st_mode)) {
        const char *base = strrchr(src, '/');
        base = base ? base + 1 : src;
        snprintf(final_dst, sizeof(final_dst), "%s/%s", dst, base);
    } else {
        strncpy(final_dst, dst, sizeof(final_dst) - 1);
    }

    FILE *f_in = fopen(src, "rb");
    if (!f_in) {
        fprintf(stderr, COLOR_RED "Cannot open source '%s': %s" COLOR_RESET "\n", src, strerror(errno));
        return 1;
    }

    FILE *f_out = fopen(final_dst, "wb");
    if (!f_out) {
        fprintf(stderr, COLOR_RED "Cannot write to destination '%s': %s" COLOR_RESET "\n", final_dst, strerror(errno));
        fclose(f_in);
        return 1;
    }

    char buffer[8192];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), f_in)) > 0) {
        if (fwrite(buffer, 1, n, f_out) != n) {
            fprintf(stderr, COLOR_RED "Write error while copying to '%s'" COLOR_RESET "\n", final_dst);
            fclose(f_in);
            fclose(f_out);
            return 1;
        }
    }

    fclose(f_in);
    fclose(f_out);

    chmod(final_dst, st.st_mode & 07777);
    printf(COLOR_GREEN "✓ Copied '%s' -> '%s'" COLOR_RESET "\n", src, final_dst);
    return 0;
}

int cmd_mv(const char *src, const char *dst) {
    if (!src || !dst || strlen(src) == 0 || strlen(dst) == 0) {
        fprintf(stderr, COLOR_RED "Usage: mv <source> <destination>" COLOR_RESET "\n");
        return 1;
    }

    char final_dst[PATH_MAX];
    struct stat dst_st;
    if (stat(dst, &dst_st) == 0 && S_ISDIR(dst_st.st_mode)) {
        const char *base = strrchr(src, '/');
        base = base ? base + 1 : src;
        snprintf(final_dst, sizeof(final_dst), "%s/%s", dst, base);
    } else {
        strncpy(final_dst, dst, sizeof(final_dst) - 1);
    }

    if (rename(src, final_dst) == 0) {
        printf(COLOR_GREEN "✓ Moved '%s' -> '%s'" COLOR_RESET "\n", src, final_dst);
        return 0;
    }

    // If rename failed across devices, fall back to copy + remove
    if (errno == EXDEV) {
        if (cmd_cp(src, final_dst) == 0) {
            unlink(src);
            printf(COLOR_GREEN "✓ Moved '%s' -> '%s' (cross-device)" COLOR_RESET "\n", src, final_dst);
            return 0;
        }
    }

    fprintf(stderr, COLOR_RED "Error moving '%s' to '%s': %s" COLOR_RESET "\n", src, final_dst, strerror(errno));
    return 1;
}

int remove_recursive(const char *path) {
    DIR *dp = opendir(path);
    if (!dp) {
        return unlink(path);
    }

    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char subpath[PATH_MAX];
        snprintf(subpath, sizeof(subpath), "%s/%s", path, entry->d_name);

        struct stat st;
        if (lstat(subpath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                remove_recursive(subpath);
            } else {
                unlink(subpath);
            }
        }
    }
    closedir(dp);
    return rmdir(path);
}

int cmd_rm(const char *path, bool recursive, bool force) {
    if (!path || strlen(path) == 0) {
        fprintf(stderr, COLOR_RED "Usage: rm [-r] [-f] <path>" COLOR_RESET "\n");
        return 1;
    }

    if (!is_path_safe_to_delete(path)) {
        fprintf(stderr, COLOR_RED "SAFETY ABORT: Refusing to delete critical or root path '%s'!" COLOR_RESET "\n", path);
        return 1;
    }

    struct stat st;
    if (lstat(path, &st) != 0) {
        fprintf(stderr, COLOR_RED "Cannot remove '%s': %s" COLOR_RESET "\n", path, strerror(errno));
        return 1;
    }

    if (S_ISDIR(st.st_mode) && !recursive) {
        fprintf(stderr, COLOR_RED "'%s' is a directory. Use 'rm -r %s' to remove recursively." COLOR_RESET "\n", path, path);
        return 1;
    }

    if (!force) {
        printf(COLOR_YELLOW "Are you sure you want to delete '%s'%s? [y/N]: " COLOR_RESET,
               path, S_ISDIR(st.st_mode) ? " (and all its contents)" : "");
        char answer[16] = {0};
        if (!fgets(answer, sizeof(answer), stdin) || (answer[0] != 'y' && answer[0] != 'Y')) {
            printf(COLOR_DIM "Aborted." COLOR_RESET "\n");
            return 0;
        }
    }

    int res = 0;
    if (S_ISDIR(st.st_mode)) {
        res = remove_recursive(path);
    } else {
        res = unlink(path);
    }

    if (res == 0) {
        printf(COLOR_GREEN "✓ Removed '%s'" COLOR_RESET "\n", path);
    } else {
        fprintf(stderr, COLOR_RED "Failed to remove '%s': %s" COLOR_RESET "\n", path, strerror(errno));
    }
    return res;
}

void find_recursive(const char *path, const char *query, int depth, int max_depth, int *match_count) {
    if (depth > max_depth) return;

    DIR *dp = opendir(path);
    if (!dp) return;

    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);

        if (strstr(entry->d_name, query) != NULL) {
            (*match_count)++;
            printf("  " COLOR_GREEN "•" COLOR_RESET " %s\n", full);
        }

        struct stat st;
        if (lstat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
            find_recursive(full, query, depth + 1, max_depth, match_count);
        }
    }
    closedir(dp);
}

int cmd_find(const char *query, const char *start_path) {
    if (!query || strlen(query) == 0) {
        fprintf(stderr, COLOR_RED "Usage: find <substring> [search_root]" COLOR_RESET "\n");
        return 1;
    }
    const char *root = (start_path && strlen(start_path) > 0) ? start_path : ".";
    printf("\n" COLOR_BOLD "Searching for '%s' starting from '%s':" COLOR_RESET "\n", query, root);
    int matches = 0;
    find_recursive(root, query, 0, 8, &matches);
    printf(COLOR_DIM "Found %d match(es).\n" COLOR_RESET "\n", matches);
    return 0;
}

int cmd_cd(const char *path) {
    const char *target = (path && strlen(path) > 0) ? path : getenv("HOME");
    if (!target) target = ".";

    if (chdir(target) != 0) {
        fprintf(stderr, COLOR_RED "chdir to '%s' failed: %s" COLOR_RESET "\n", target, strerror(errno));
        return 1;
    }
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        printf(COLOR_CYAN "Current directory: %s" COLOR_RESET "\n", cwd);
    }
    return 0;
}

int cmd_pwd(void) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        printf(COLOR_CYAN "%s" COLOR_RESET "\n", cwd);
        return 0;
    }
    perror("getcwd");
    return 1;
}

/* --- Safe Dummy Sandbox Generator --- */

int cmd_create_sandbox(const char *target_dir) {
    const char *base = (target_dir && strlen(target_dir) > 0) ? target_dir : "dummy_sandbox";

    printf("\n" COLOR_YELLOW "Creating safe dummy sandbox at ./%s..." COLOR_RESET "\n", base);

    mkdir(base, 0755);

    char sub_docs[PATH_MAX], sub_code[PATH_MAX], sub_logs[PATH_MAX];
    snprintf(sub_docs, sizeof(sub_docs), "%s/documents", base);
    snprintf(sub_code, sizeof(sub_code), "%s/src", base);
    snprintf(sub_logs, sizeof(sub_logs), "%s/logs", base);

    mkdir(sub_docs, 0755);
    mkdir(sub_code, 0755);
    mkdir(sub_logs, 0755);

    // Create dummy files
    char file_path[PATH_MAX];

    snprintf(file_path, sizeof(file_path), "%s/README.md", base);
    FILE *f = fopen(file_path, "w");
    if (f) {
        fputs("# Dummy Sandbox Environment\n\nThis folder contains test files for safe file manager experiments.\nFeel free to copy, move, edit, or delete anything inside here.\n", f);
        fclose(f);
    }

    snprintf(file_path, sizeof(file_path), "%s/notes.txt", sub_docs);
    f = fopen(file_path, "w");
    if (f) {
        fputs("Meeting notes:\n1. Test list, info, and tree commands.\n2. Verify file copy and move operations.\n3. Test safe deletion.\n", f);
        fclose(f);
    }

    snprintf(file_path, sizeof(file_path), "%s/main.c", sub_code);
    f = fopen(file_path, "w");
    if (f) {
        fputs("#include <stdio.h>\n\nint main() {\n    printf(\"Hello from dummy sandbox!\\n\");\n    return 0;\n}\n", f);
        fclose(f);
    }

    snprintf(file_path, sizeof(file_path), "%s/app.log", sub_logs);
    f = fopen(file_path, "w");
    if (f) {
        fputs("[2026-10-02 11:30:00] INFO: Sandbox initialized successfully.\n[2026-10-02 11:30:01] DEBUG: All test files created.\n", f);
        fclose(f);
    }

    printf(COLOR_GREEN "✓ Sandbox created successfully! Try running: tree %s" COLOR_RESET "\n\n", base);
    return 0;
}

void print_help(void) {
    printf("\n" COLOR_BOLD "C File Manager - Command Reference:" COLOR_RESET "\n");
    printf("  " COLOR_CYAN "ls [path]" COLOR_RESET "             List directory contents with detailed metadata\n");
    printf("  " COLOR_CYAN "la [path]" COLOR_RESET "             List including hidden files (dotfiles)\n");
    printf("  " COLOR_CYAN "tree [path] [depth]" COLOR_RESET "   Display hierarchical ASCII directory tree\n");
    printf("  " COLOR_CYAN "info <path>" COLOR_RESET "           Show full file/directory metadata\n");
    printf("  " COLOR_CYAN "cat <path> [lines]" COLOR_RESET "    Display file contents safely\n");
    printf("  " COLOR_CYAN "touch <path>" COLOR_RESET "          Create an empty file or update timestamp\n");
    printf("  " COLOR_CYAN "mkdir <path>" COLOR_RESET "          Create a new directory\n");
    printf("  " COLOR_CYAN "cp <src> <dst>" COLOR_RESET "        Copy a file to another path/directory\n");
    printf("  " COLOR_CYAN "mv <src> <dst>" COLOR_RESET "        Move or rename a file\n");
    printf("  " COLOR_CYAN "rm [-r] [-f] <path>" COLOR_RESET "   Delete file or folder (-r for directory)\n");
    printf("  " COLOR_CYAN "find <substring> [dir]" COLOR_RESET "Search for files matching name\n");
    printf("  " COLOR_CYAN "cd <path>" COLOR_RESET "             Change working directory\n");
    printf("  " COLOR_CYAN "pwd" COLOR_RESET "                   Print current working directory\n");
    printf("  " COLOR_CYAN "sandbox [name]" COLOR_RESET "        Create safe dummy test folder with mock files\n");
    printf("  " COLOR_CYAN "help" COLOR_RESET "                  Display this help message\n");
    printf("  " COLOR_CYAN "exit / quit" COLOR_RESET "           Exit file manager\n\n");
}

/* --- Interactive Shell --- */

void interactive_mode(void) {
    char line[1024];
    printf(COLOR_BOLD COLOR_CYAN "=== Safe C File Manager ===" COLOR_RESET "\n");
    printf("Type " COLOR_BOLD "help" COLOR_RESET " for commands, " COLOR_BOLD "sandbox" COLOR_RESET " to generate dummy test files, or " COLOR_BOLD "exit" COLOR_RESET " to quit.\n\n");

    while (1) {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) strncpy(cwd, ".", sizeof(cwd));

        // Get relative or base folder for cleaner prompt
        const char *display_cwd = cwd;
        const char *home = getenv("HOME");
        char rel_cwd[PATH_MAX];
        if (home && strncmp(cwd, home, strlen(home)) == 0) {
            snprintf(rel_cwd, sizeof(rel_cwd), "~%s", cwd + strlen(home));
            display_cwd = rel_cwd;
        }

        printf(COLOR_BLUE "%s" COLOR_RESET " " COLOR_BOLD "fileman>" COLOR_RESET " ", display_cwd);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        // Trim newline and whitespace
        char *p = line;
        while (isspace((unsigned char)*p)) p++;
        char *end = p + strlen(p) - 1;
        while (end >= p && isspace((unsigned char)*end)) *end-- = '\0';

        if (strlen(p) == 0) continue;

        // Parse command and arguments
        char *cmd = strtok(p, " \t");
        if (!cmd) continue;

        if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0 || strcmp(cmd, "q") == 0) {
            printf(COLOR_DIM "Exiting File Manager. Goodbye!" COLOR_RESET "\n");
            break;
        } else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
            print_help();
        } else if (strcmp(cmd, "pwd") == 0) {
            cmd_pwd();
        } else if (strcmp(cmd, "cd") == 0) {
            char *arg = strtok(NULL, " \t");
            cmd_cd(arg);
        } else if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "dir") == 0) {
            char *arg = strtok(NULL, " \t");
            cmd_list(arg, false);
        } else if (strcmp(cmd, "la") == 0 || strcmp(cmd, "ll") == 0) {
            char *arg = strtok(NULL, " \t");
            cmd_list(arg, true);
        } else if (strcmp(cmd, "tree") == 0) {
            char *arg1 = strtok(NULL, " \t");
            char *arg2 = strtok(NULL, " \t");
            int depth = arg2 ? atoi(arg2) : 4;
            cmd_tree(arg1, depth);
        } else if (strcmp(cmd, "info") == 0) {
            char *arg = strtok(NULL, " \t");
            cmd_info(arg);
        } else if (strcmp(cmd, "cat") == 0 || strcmp(cmd, "view") == 0) {
            char *arg1 = strtok(NULL, " \t");
            char *arg2 = strtok(NULL, " \t");
            size_t max_l = arg2 ? (size_t)atoi(arg2) : 50;
            cmd_cat(arg1, max_l);
        } else if (strcmp(cmd, "touch") == 0) {
            char *arg = strtok(NULL, " \t");
            cmd_touch(arg);
        } else if (strcmp(cmd, "mkdir") == 0) {
            char *arg = strtok(NULL, " \t");
            cmd_mkdir(arg);
        } else if (strcmp(cmd, "cp") == 0) {
            char *src = strtok(NULL, " \t");
            char *dst = strtok(NULL, " \t");
            cmd_cp(src, dst);
        } else if (strcmp(cmd, "mv") == 0 || strcmp(cmd, "rename") == 0) {
            char *src = strtok(NULL, " \t");
            char *dst = strtok(NULL, " \t");
            cmd_mv(src, dst);
        } else if (strcmp(cmd, "rm") == 0) {
            char *arg1 = strtok(NULL, " \t");
            bool rec = false;
            bool force = false;
            while (arg1 && arg1[0] == '-') {
                if (strchr(arg1, 'r') || strchr(arg1, 'R')) rec = true;
                if (strchr(arg1, 'f')) force = true;
                arg1 = strtok(NULL, " \t");
            }
            cmd_rm(arg1, rec, force);
        } else if (strcmp(cmd, "find") == 0 || strcmp(cmd, "search") == 0) {
            char *query = strtok(NULL, " \t");
            char *dir = strtok(NULL, " \t");
            cmd_find(query, dir);
        } else if (strcmp(cmd, "sandbox") == 0 || strcmp(cmd, "demo") == 0) {
            char *arg = strtok(NULL, " \t");
            cmd_create_sandbox(arg);
        } else {
            printf(COLOR_RED "Unknown command: '%s'. Type 'help' for available commands." COLOR_RESET "\n", cmd);
        }
    }
}

/* --- Main CLI Dispatcher --- */

int main(int argc, char *argv[]) {
    if (argc < 2) {
        interactive_mode();
        return 0;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "-i") == 0 || strcmp(cmd, "--interactive") == 0) {
        interactive_mode();
        return 0;
    }

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        print_help();
        return 0;
    }

    if (strcmp(cmd, "ls") == 0) {
        return cmd_list(argc > 2 ? argv[2] : ".", false);
    } else if (strcmp(cmd, "la") == 0) {
        return cmd_list(argc > 2 ? argv[2] : ".", true);
    } else if (strcmp(cmd, "tree") == 0) {
        int depth = argc > 3 ? atoi(argv[3]) : 4;
        return cmd_tree(argc > 2 ? argv[2] : ".", depth);
    } else if (strcmp(cmd, "info") == 0) {
        return cmd_info(argc > 2 ? argv[2] : NULL);
    } else if (strcmp(cmd, "cat") == 0) {
        size_t max_l = argc > 3 ? (size_t)atoi(argv[3]) : 50;
        return cmd_cat(argc > 2 ? argv[2] : NULL, max_l);
    } else if (strcmp(cmd, "touch") == 0) {
        return cmd_touch(argc > 2 ? argv[2] : NULL);
    } else if (strcmp(cmd, "mkdir") == 0) {
        return cmd_mkdir(argc > 2 ? argv[2] : NULL);
    } else if (strcmp(cmd, "cp") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s cp <source> <dest>\n", argv[0]);
            return 1;
        }
        return cmd_cp(argv[2], argv[3]);
    } else if (strcmp(cmd, "mv") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s mv <source> <dest>\n", argv[0]);
            return 1;
        }
        return cmd_mv(argv[2], argv[3]);
    } else if (strcmp(cmd, "rm") == 0) {
        bool rec = false;
        bool force = false;
        const char *target = NULL;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "-rf") == 0) rec = true;
            if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "-rf") == 0) force = true;
            if (argv[i][0] != '-') target = argv[i];
        }
        return cmd_rm(target, rec, force);
    } else if (strcmp(cmd, "find") == 0) {
        return cmd_find(argc > 2 ? argv[2] : NULL, argc > 3 ? argv[3] : ".");
    } else if (strcmp(cmd, "pwd") == 0) {
        return cmd_pwd();
    } else if (strcmp(cmd, "sandbox") == 0) {
        return cmd_create_sandbox(argc > 2 ? argv[2] : "dummy_sandbox");
    }

    fprintf(stderr, "Unknown command '%s'. Run '%s help' for usage.\n", cmd, argv[0]);
    return 1;
}
