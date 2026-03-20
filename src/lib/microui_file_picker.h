/* microui_file_picker.h – Replicates original libsofd UI in microui */
#ifndef MICROUI_FILE_PICKER_H
#define MICROUI_FILE_PICKER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <time.h>
#include <libgen.h>   // for dirname
#include <mntent.h>   // for /proc/mounts (optional)

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * Public API
 * ================================================================ */
typedef enum {
    SOFD_MODE_OPEN_FILE,
    SOFD_MODE_OPEN_MULTIPLE,
    SOFD_MODE_SELECT_DIR
} SofdMode;

typedef struct SofdState SofdState;
typedef void (*SofdCallback)(const char** files, int count, void* userdata);

SofdState* sofd_new(void);
void sofd_free(SofdState* state);
void sofd_set_mode(SofdState* state, SofdMode mode);
void sofd_set_title(SofdState* state, const char* title);
void sofd_set_directory(SofdState* state, const char* path);
void sofd_set_filter(SofdState* state, const char* filter);
void sofd_set_callback(SofdState* state, SofdCallback callback, void* userdata);
void sofd_show(SofdState* state);
int sofd_is_open(SofdState* state);
void sofd_close(SofdState* state);
const char* sofd_get_selected(SofdState* state);
const char** sofd_get_selections(SofdState* state, int* count);
void sofd_clear_selections(SofdState* state);
void sofd_render(mu_Context* ctx, SofdState* state);

#ifdef __cplusplus
}
#endif

#endif /* MICROUI_FILE_PICKER_H */

#ifdef MICROUI_FILE_PICKER_IMPLEMENTATION

#include "microui.h"

/* ================================================================
 * Constants & Types
 * ================================================================ */
#define MAX_RECENT_ENTRIES 24
#define MAX_RECENT_AGE     15552000  /* 180 days in sec */
#define PLACES_WIDTH       150       /* fixed width for Places panel */

typedef struct {
    char name[256];
    off_t size;
    time_t mtime;
    char strsize[32];
    char strtime[32];
    int is_dir;
    int selected;
} SofdFileEntry;

typedef struct {
    char name[256];
    char path[1024];
} SofdPlace;

typedef struct {
    char path[1024];
    time_t atime;
} SofdRecentFile;

struct SofdState {
    /* UI state */
    int open;
    char title[256];
    char current_path[1024];
    char filter_pattern[256];
    SofdMode mode;

    /* File entries */
    SofdFileEntry* entries;
    int entry_count;

    /* Places */
    SofdPlace* places;
    int place_count;
    int show_places;          /* checkbox state */
    int show_all;             /* "List All Files" – disables filter */
    int show_hidden;          /* show .dot files */

    /* Sorting */
    int sort_column;          /* 0=name, 1=size, 2=time */
    int sort_order;           /* 0=ascending, 1=descending */

    /* Recent files */
    SofdRecentFile* recent;
    int recent_count;
    int show_recent;

    /* Path breadcrumbs */
    char** path_parts;
    int path_parts_count;

    /* Selection */
    int* selected_indices;    /* only used for multi‑selection */
    int selected_count;
    char* selected_buf;       /* concatenated strings for multi */

    /* Callback */
    SofdCallback callback;
    void* callback_userdata;

    /* Filter function (user supplied) */
    int (*filter_func)(const char* name, void* userdata);
    void* filter_userdata;

    /* Double‑click detection */
    unsigned long last_click_time;
    int last_click_index;
    int hover_index;

    /* Column widths (cached) */
    int col_name_w, col_size_w, col_time_w;
};

/* ================================================================
 * Utility functions
 * ================================================================ */
static int sofd_filter_by_pattern(const char* name, const char* pattern) {
    if (!pattern || pattern[0] == '\0') return 1;
    char pat[256];
    strncpy(pat, pattern, sizeof(pat)-1);
    pat[sizeof(pat)-1] = '\0';
    char* token = strtok(pat, ";");
    while (token) {
        while (*token == ' ') token++;
        char* end = token + strlen(token)-1;
        while (end > token && *end == ' ') end--;
        end[1] = '\0';
        if (token[0] == '*') {
            const char* ext = token+1;
            int len = strlen(name);
            int ext_len = strlen(ext);
            if (len >= ext_len && strcmp(name+len-ext_len, ext) == 0)
                return 1;
        } else if (strcmp(name, token) == 0)
            return 1;
        token = strtok(NULL, ";");
    }
    return 0;
}

static int sofd_default_filter(const char* name, void* userdata) {
    SofdState* s = (SofdState*)userdata;
    if (s->show_all) return 1;
    return sofd_filter_by_pattern(name, s->filter_pattern);
}

/* Format file size (human readable) */
static void sofd_format_size(SofdState* s, SofdFileEntry* e) {
    off_t sz = e->size;
    if (sz > 10995116277760LL)      sprintf(e->strsize, "%.0f TB", sz / 1099511627776.0);
    else if (sz > 1099511627776LL)  sprintf(e->strsize, "%.1f TB", sz / 1099511627776.0);
    else if (sz > 10737418240LL)    sprintf(e->strsize, "%.0f GB", sz / 1073741824.0);
    else if (sz > 1073741824LL)     sprintf(e->strsize, "%.1f GB", sz / 1073741824.0);
    else if (sz > 10485760LL)       sprintf(e->strsize, "%.0f MB", sz / 1048576.0);
    else if (sz > 1048576LL)        sprintf(e->strsize, "%.1f MB", sz / 1048576.0);
    else if (sz > 10240LL)          sprintf(e->strsize, "%.0f KB", sz / 1024.0);
    else if (sz >= 1000LL)          sprintf(e->strsize, "%.1f KB", sz / 1024.0);
    else                            sprintf(e->strsize, "%lld B", (long long)sz);
    int tw = s->col_size_w;
    (void)tw; /* will be used later if we cache widths */
}

/* Format time */
static void sofd_format_time(SofdFileEntry* e) {
    struct tm* tm = localtime(&e->mtime);
    if (tm)
        strftime(e->strtime, sizeof(e->strtime), "%Y-%m-%d %H:%M", tm);
    else
        strcpy(e->strtime, "unknown");
}

/* Sorting comparators */
static int sofd_cmp_name_asc(const void* a, const void* b) {
    const SofdFileEntry* fa = (const SofdFileEntry*)a;
    const SofdFileEntry* fb = (const SofdFileEntry*)b;
    if (fa->is_dir && !fb->is_dir) return -1;
    if (!fa->is_dir && fb->is_dir) return 1;
    return strcmp(fa->name, fb->name);
}
static int sofd_cmp_name_desc(const void* a, const void* b) {
    const SofdFileEntry* fa = (const SofdFileEntry*)a;
    const SofdFileEntry* fb = (const SofdFileEntry*)b;
    if (fa->is_dir && !fb->is_dir) return -1;
    if (!fa->is_dir && fb->is_dir) return 1;
    return -strcmp(fa->name, fb->name);
}
static int sofd_cmp_size_asc(const void* a, const void* b) {
    const SofdFileEntry* fa = (const SofdFileEntry*)a;
    const SofdFileEntry* fb = (const SofdFileEntry*)b;
    if (fa->is_dir && !fb->is_dir) return -1;
    if (!fa->is_dir && fb->is_dir) return 1;
    if (fa->size == fb->size) return 0;
    return (fa->size < fb->size) ? -1 : 1;
}
static int sofd_cmp_size_desc(const void* a, const void* b) {
    const SofdFileEntry* fa = (const SofdFileEntry*)a;
    const SofdFileEntry* fb = (const SofdFileEntry*)b;
    if (fa->is_dir && !fb->is_dir) return -1;
    if (!fa->is_dir && fb->is_dir) return 1;
    if (fa->size == fb->size) return 0;
    return (fa->size > fb->size) ? -1 : 1;
}
static int sofd_cmp_time_asc(const void* a, const void* b) {
    const SofdFileEntry* fa = (const SofdFileEntry*)a;
    const SofdFileEntry* fb = (const SofdFileEntry*)b;
    if (fa->is_dir && !fb->is_dir) return -1;
    if (!fa->is_dir && fb->is_dir) return 1;
    if (fa->mtime == fb->mtime) return 0;
    return (fa->mtime < fb->mtime) ? -1 : 1;
}
static int sofd_cmp_time_desc(const void* a, const void* b) {
    const SofdFileEntry* fa = (const SofdFileEntry*)a;
    const SofdFileEntry* fb = (const SofdFileEntry*)b;
    if (fa->is_dir && !fb->is_dir) return -1;
    if (!fa->is_dir && fb->is_dir) return 1;
    if (fa->mtime == fb->mtime) return 0;
    return (fa->mtime > fb->mtime) ? -1 : 1;
}

static void sofd_sort_entries(SofdState* s) {
    if (s->entry_count == 0) return;
    int (*cmp)(const void*, const void*) = NULL;
    switch (s->sort_column) {
        case 0: cmp = s->sort_order ? sofd_cmp_name_desc : sofd_cmp_name_asc; break;
        case 1: cmp = s->sort_order ? sofd_cmp_size_desc : sofd_cmp_size_asc; break;
        case 2: cmp = s->sort_order ? sofd_cmp_time_desc : sofd_cmp_time_asc; break;
    }
    if (cmp) qsort(s->entries, s->entry_count, sizeof(SofdFileEntry), cmp);
}

/* ================================================================
 * Places population
 * ================================================================ */
static void sofd_add_place(SofdState* s, const char* name, const char* path) {
    s->places = (SofdPlace*)realloc(s->places, (s->place_count+1)*sizeof(SofdPlace));
    strncpy(s->places[s->place_count].name, name, 255);
    strncpy(s->places[s->place_count].path, path, 1023);
    s->place_count++;
}

static void sofd_populate_places(SofdState* s) {
    s->place_count = 0;
    free(s->places); s->places = NULL;

    /* Recently Used */
    if (s->recent_count > 0)
        sofd_add_place(s, "Recently Used", "");

    /* Home */
    const char* home = getenv("HOME");
    if (!home) { struct passwd* pw = getpwuid(getuid()); home = pw->pw_dir; }
    sofd_add_place(s, "Home", home);

    /* Desktop */
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s/Desktop", home);
    sofd_add_place(s, "Desktop", tmp);

    /* Filesystem */
    sofd_add_place(s, "Filesystem", "/");

    /* Mount points (from /proc/mounts) */
    FILE* mt = fopen("/proc/mounts", "r");
    if (mt) {
        struct mntent* mnt;
        while ((mnt = getmntent(mt))) {
            if (strncmp(mnt->mnt_dir, "/", 1) != 0) continue;
            if (strcmp(mnt->mnt_dir, "/") == 0) continue;
            if (strncmp(mnt->mnt_dir, "/home", 5) == 0) continue;
            if (strstr(mnt->mnt_type, "proc") || strstr(mnt->mnt_type, "sysfs") ||
                strstr(mnt->mnt_type, "devtmpfs") || strstr(mnt->mnt_type, "tmpfs"))
                continue;
            char* base = strrchr(mnt->mnt_dir, '/');
            const char* name = base ? base+1 : mnt->mnt_dir;
            sofd_add_place(s, name, mnt->mnt_dir);
        }
        fclose(mt);
    }
}

/* ================================================================
 * Directory scanning
 * ================================================================ */
static void sofd_scan_directory(SofdState* s) {
    free(s->entries); s->entries = NULL;
    s->entry_count = 0;
    free(s->selected_indices); s->selected_indices = NULL;
    free(s->selected_buf); s->selected_buf = NULL;
    s->selected_count = 0;

    /* If showing recent files, create entries from recent list */
    if (s->show_recent && s->recent_count > 0) {
        s->entry_count = s->recent_count;
        s->entries = (SofdFileEntry*)calloc(s->entry_count, sizeof(SofdFileEntry));
        for (int i = 0; i < s->recent_count; i++) {
            char* name = strrchr(s->recent[i].path, '/');
            name = name ? name+1 : s->recent[i].path;
            strncpy(s->entries[i].name, name, 255);
            s->entries[i].size = 0;
            s->entries[i].mtime = s->recent[i].atime;
            s->entries[i].is_dir = 0;
            sofd_format_time(&s->entries[i]);
            s->entries[i].selected = 0;
            strcpy(s->entries[i].strsize, " ");
        }
        sofd_sort_entries(s);
        s->selected_indices = (int*)calloc(s->entry_count, sizeof(int));
        return;
    }

    /* Normal directory scan */
    DIR* dir = opendir(s->current_path);
    if (!dir) return;

    struct dirent* de;
    while ((de = readdir(dir))) {
        if (!s->show_hidden && de->d_name[0] == '.') continue;
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;

        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", s->current_path, de->d_name);
        struct stat st;
        if (stat(fullpath, &st) != 0) continue;

        int is_dir = S_ISDIR(st.st_mode);
        if (s->mode == SOFD_MODE_SELECT_DIR && !is_dir) continue;
        if (s->mode != SOFD_MODE_SELECT_DIR && !is_dir && !s->filter_func(de->d_name, s->filter_userdata))
            continue;

        s->entry_count++;
        s->entries = (SofdFileEntry*)realloc(s->entries, s->entry_count * sizeof(SofdFileEntry));
        SofdFileEntry* e = &s->entries[s->entry_count-1];
        strncpy(e->name, de->d_name, 255);
        e->size = st.st_size;
        e->mtime = st.st_mtime;
        e->is_dir = is_dir;
        e->selected = 0;
        sofd_format_size(s, e);
        sofd_format_time(e);
    }
    closedir(dir);
    sofd_sort_entries(s);
    s->selected_indices = (int*)calloc(s->entry_count, sizeof(int));
    if (s->mode == SOFD_MODE_OPEN_MULTIPLE)
        s->selected_buf = (char*)calloc(s->entry_count * 1024, 1);
}

/* ================================================================
 * Path breadcrumbs
 * ================================================================ */
static void sofd_update_path_parts(SofdState* s) {
    if (s->path_parts) {
        for (int i = 0; i < s->path_parts_count; i++) free(s->path_parts[i]);
        free(s->path_parts);
        s->path_parts = NULL;
    }
    if (s->show_recent) {
        s->path_parts_count = 1;
        s->path_parts = (char**)calloc(1, sizeof(char*));
        s->path_parts[0] = strdup("Recently Used");
        return;
    }
    char path[1024];
    strncpy(path, s->current_path, 1023);
    s->path_parts_count = 0;
    char* tok = strtok(path, "/");
    while (tok) { s->path_parts_count++; tok = strtok(NULL, "/"); }
    s->path_parts = (char**)calloc(s->path_parts_count, sizeof(char*));
    strncpy(path, s->current_path, 1023);
    int i = 0;
    tok = strtok(path, "/");
    while (tok) { s->path_parts[i] = strdup(tok); i++; tok = strtok(NULL, "/"); }
}

static void sofd_navigate_to(SofdState* s, const char* path) {
    if (strcmp(path, "recent") == 0) {
        s->show_recent = 1;
        strcpy(s->current_path, "");
    } else {
        s->show_recent = 0;
        strncpy(s->current_path, path, 1023);
        if (s->current_path[strlen(s->current_path)-1] == '/')
            s->current_path[strlen(s->current_path)-1] = '\0';
    }
    sofd_scan_directory(s);
    sofd_update_path_parts(s);
    s->hover_index = -1;
    s->last_click_index = -1;
}

/* ================================================================
 * Recent files
 * ================================================================ */
static void sofd_add_recent(SofdState* s, const char* path) {
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return;
    for (int i = 0; i < s->recent_count; i++) {
        if (strcmp(s->recent[i].path, path) == 0) {
            s->recent[i].atime = time(NULL);
            return;
        }
    }
    s->recent = (SofdRecentFile*)realloc(s->recent, (s->recent_count+1)*sizeof(SofdRecentFile));
    strncpy(s->recent[s->recent_count].path, path, 1023);
    s->recent[s->recent_count].atime = time(NULL);
    s->recent_count++;
    if (s->recent_count > MAX_RECENT_ENTRIES) s->recent_count = MAX_RECENT_ENTRIES;
}

/* ================================================================
 * Public API implementation
 * ================================================================ */
SofdState* sofd_new(void) {
    SofdState* s = (SofdState*)calloc(1, sizeof(SofdState));
    s->mode = SOFD_MODE_OPEN_FILE;
    s->filter_func = sofd_default_filter;
    s->filter_userdata = s;
    s->show_places = 1;
    s->show_all = 0;
    s->show_hidden = 0;
    s->sort_column = 0;
    s->sort_order = 0;
    s->hover_index = -1;
    s->last_click_index = -1;
    strcpy(s->title, "Open File");

    const char* home = getenv("HOME");
    if (!home) { struct passwd* pw = getpwuid(getuid()); home = pw->pw_dir; }
    sofd_navigate_to(s, home);
    sofd_populate_places(s);
    return s;
}

void sofd_free(SofdState* s) {
    if (!s) return;
    free(s->entries);
    free(s->selected_indices);
    free(s->selected_buf);
    free(s->recent);
    if (s->path_parts) {
        for (int i = 0; i < s->path_parts_count; i++) free(s->path_parts[i]);
        free(s->path_parts);
    }
    if (s->places) free(s->places);
    free(s);
}

void sofd_set_mode(SofdState* s, SofdMode mode) { s->mode = mode; sofd_scan_directory(s); }
void sofd_set_title(SofdState* s, const char* title) { strncpy(s->title, title, 255); }
void sofd_set_directory(SofdState* s, const char* path) { sofd_navigate_to(s, path); }
void sofd_set_filter(SofdState* s, const char* filter) { strncpy(s->filter_pattern, filter, 255); sofd_scan_directory(s); }
void sofd_set_callback(SofdState* s, SofdCallback cb, void* ud) { s->callback = cb; s->callback_userdata = ud; }
void sofd_show(SofdState* s) { s->open = 1; }
int sofd_is_open(SofdState* s) { return s->open; }
void sofd_close(SofdState* s) { s->open = 0; sofd_clear_selections(s); }

const char* sofd_get_selected(SofdState* s) {
    static char fullpath[1024];
    for (int i = 0; i < s->entry_count; i++) {
        if (s->selected_indices[i]) {
            snprintf(fullpath, sizeof(fullpath), "%s/%s", s->current_path, s->entries[i].name);
            return fullpath;
        }
    }
    return NULL;
}

const char** sofd_get_selections(SofdState* s, int* count) {
    *count = 0;
    if (!s->selected_buf) return NULL;
    /* This is a simple version – it returns the list of filenames (not full paths) for brevity.
       In a real app you'd probably want full paths; adjust as needed. */
    char* p = s->selected_buf;
    for (int i = 0; i < s->entry_count; i++) {
        if (s->selected_indices[i]) {
            strcpy(p, s->entries[i].name);
            p += strlen(s->entries[i].name) + 1;
            (*count)++;
        }
    }
    return (const char**)s->selected_buf;
}

void sofd_clear_selections(SofdState* s) {
    if (s->selected_indices) memset(s->selected_indices, 0, s->entry_count * sizeof(int));
    s->selected_count = 0;
}

/* ================================================================
 * Rendering (the main UI)
 * ================================================================ */
void sofd_render(mu_Context* ctx, SofdState* s) {
    if (!s->open) return;

    int w = 720, h = 480;
    int x = 100, y = 100;
    if (!mu_begin_window_ex(ctx, s->title, mu_rect(x, y, w, h), 0)) return;

    /* ===== Path bar ===== */
    mu_layout_row(ctx, 1, (int[]){-1}, 0);
    mu_begin_panel_ex(ctx, "pathbar", 0);

    int nb = s->path_parts_count + 2;
    int* widths = (int*)alloca(nb * sizeof(int));
    for (int i = 0; i < nb; i++) widths[i] = -1;
    mu_layout_row(ctx, nb, widths, 0);

    if (mu_button_ex(ctx, "Recent", 0, 0)) sofd_navigate_to(s, "recent");
    char fullpath[1024] = "";
    for (int i = 0; i < s->path_parts_count; i++) {
        if (i > 0) strcat(fullpath, "/");
        strcat(fullpath, s->path_parts[i]);
        if (mu_button_ex(ctx, s->path_parts[i], 0, 0)) {
            char p[1024] = "/";
            strcat(p, fullpath);
            sofd_navigate_to(s, p);
        }
    }
    if (s->path_parts_count > 0 && !s->show_recent) {
        if (mu_button_ex(ctx, "Up", 0, 0)) {
            char parent[1024];
            strncpy(parent, s->current_path, 1023);
            char* last = strrchr(parent, '/');
            if (last) {
                *last = '\0';
                if (parent[0] == '\0') strcpy(parent, "/");
                sofd_navigate_to(s, parent);
            }
        }
    }
    mu_end_panel(ctx);

    /* ===== Two‑column main area ===== */
    mu_layout_row(ctx, 2, (int[]){ PLACES_WIDTH, -1 }, -25);

    /* ----- Places panel ----- */
    mu_begin_panel_ex(ctx, "places", 0);
    if (s->show_places) {
        mu_layout_row(ctx, 1, (int[]){ -1 }, 0);
        for (int i = 0; i < s->place_count; i++) {
            if (mu_button_ex(ctx, s->places[i].name, 0, 0)) {
                if (strcmp(s->places[i].name, "Recently Used") == 0)
                    sofd_navigate_to(s, "recent");
                else
                    sofd_navigate_to(s, s->places[i].path);
            }
        }
    }
    mu_end_panel(ctx);

    /* ----- File list panel (table) ----- */
    mu_begin_panel_ex(ctx, "filelist", 0);

    /* Column headers (with sort indicators) */
    int header_widths[] = { -1, 80, 130 };  // name, size, time
    mu_layout_row(ctx, 3, header_widths, 0);

    // Name header
    if (mu_button_ex(ctx, s->sort_column == 0 ?
        (s->sort_order ? "Name ▲" : "Name ▼") : "Name", 0, 0)) {
        if (s->sort_column == 0) s->sort_order = !s->sort_order;
        else { s->sort_column = 0; s->sort_order = 0; }
        sofd_sort_entries(s);
    }

    // Size header
    if (mu_button_ex(ctx, s->sort_column == 1 ?
        (s->sort_order ? "Size ▲" : "Size ▼") : "Size", 0, 0)) {
        if (s->sort_column == 1) s->sort_order = !s->sort_order;
        else { s->sort_column = 1; s->sort_order = 0; }
        sofd_sort_entries(s);
    }

    // Time header
    if (mu_button_ex(ctx, s->sort_column == 2 ?
        (s->sort_order ? "Last Modified ▲" : "Last Modified ▼") : "Last Modified", 0, 0)) {
        if (s->sort_column == 2) s->sort_order = !s->sort_order;
        else { s->sort_column = 2; s->sort_order = 0; }
        sofd_sort_entries(s);
    }

    /* Table rows */
    int item_height = ctx->style->size.y + ctx->style->spacing;
    mu_layout_row(ctx, 3, header_widths, item_height);

    for (int i = 0; i < s->entry_count; i++) {
        SofdFileEntry* e = &s->entries[i];
        int selected = s->selected_indices && s->selected_indices[i];

        /* Get rects for the three columns */
        mu_Rect name_rect  = mu_layout_next(ctx);
        mu_Rect size_rect  = mu_layout_next(ctx);
        mu_Rect time_rect  = mu_layout_next(ctx);

        /* Combine to get the whole row rect */
        mu_Rect row_rect = {
            name_rect.x,
            name_rect.y,
            size_rect.x + size_rect.w - name_rect.x,
            name_rect.h
        };

        /* Hover detection */
        if (mu_mouse_over(ctx, row_rect)) s->hover_index = i;

        /* Draw selection / hover background */
        if (selected) {
            mu_draw_rect(ctx, row_rect, mu_color(70, 150, 220, 100));
        } else if (s->hover_index == i) {
            mu_draw_rect(ctx, row_rect, mu_color(100, 100, 100, 80));
        }

        /* Name column: interactive button */
        char name_buf[256];
        if (e->is_dir) snprintf(name_buf, sizeof(name_buf), "[D] %s", e->name);
        else           snprintf(name_buf, sizeof(name_buf), "    %s", e->name);

        /* Use a button that covers only the name column */
        mu_push_id(ctx, &i, sizeof(i));
        if (mu_button_ex(ctx, name_buf, 0, 0)) {
            if (e->is_dir) {
                char newpath[1024];
                snprintf(newpath, sizeof(newpath), "%s/%s", s->current_path, e->name);
                sofd_navigate_to(s, newpath);
            } else {
                if (s->mode == SOFD_MODE_OPEN_MULTIPLE) {
                    s->selected_indices[i] = !selected;
                } else {
                    sofd_clear_selections(s);
                    s->selected_indices[i] = 1;
                }
            }
        }
        mu_pop_id(ctx);

        /* Size column: right‑aligned text */
        mu_draw_text(ctx, ctx->style->font, e->strsize, -1,
            mu_vec2(size_rect.x + size_rect.w - ctx->text_width(ctx->style->font, e->strsize, -1),
                    size_rect.y + (size_rect.h - ctx->font_height(ctx->style->font)) / 2),
            ctx->style->colors[MU_COLOR_TEXT]);

        /* Time column: right‑aligned text */
        mu_draw_text(ctx, ctx->style->font, e->strtime, -1,
            mu_vec2(time_rect.x + time_rect.w - ctx->text_width(ctx->style->font, e->strtime, -1),
                    time_rect.y + (time_rect.h - ctx->font_height(ctx->style->font)) / 2),
            ctx->style->colors[MU_COLOR_TEXT]);

        /* Double‑click detection (only on files) */
        if (s->hover_index == i && !e->is_dir) {
            if (ctx->mouse_pressed & MU_MOUSE_LEFT) {
                if (ctx->frame - s->last_click_time < 20 && s->last_click_index == i) {
                    char fullpath[1024];
                    snprintf(fullpath, sizeof(fullpath), "%s/%s", s->current_path, e->name);
                    const char* path = fullpath;
                    if (s->callback) s->callback(&path, 1, s->callback_userdata);
                    sofd_close(s);
                }
                s->last_click_time = ctx->frame;
                s->last_click_index = i;
            }
        }
    }

    mu_end_panel(ctx);

    /* ===== Checkboxes & Buttons ===== */
    mu_layout_row(ctx, 4, (int[]){ -1, -1, -1, -1 }, 0);
    mu_checkbox(ctx, "Show Places", &s->show_places);
    mu_checkbox(ctx, "List All Files", &s->show_all);
    mu_checkbox(ctx, "Show Hidden", &s->show_hidden);
    mu_layout_next(ctx);  // spacer

    mu_layout_row(ctx, 2, (int[]){ -1, 80 }, 0);
    mu_layout_set_next(ctx, mu_rect(0, 0, 1, 1), 1);
    if (mu_button_ex(ctx, "Cancel", 0, 0)) sofd_close(s);
    if (mu_button_ex(ctx, "Open", 0, 0)) {
        if (s->mode == SOFD_MODE_OPEN_MULTIPLE) {
            int cnt = 0;
            for (int i = 0; i < s->entry_count; i++) if (s->selected_indices[i]) cnt++;
            if (cnt > 0) {
                const char** paths = (const char**)alloca(cnt * sizeof(const char*));
                int idx = 0;
                for (int i = 0; i < s->entry_count; i++) {
                    if (s->selected_indices[i]) {
                        char* fullpath = (char*)alloca(1024);
                        snprintf(fullpath, 1024, "%s/%s", s->current_path, s->entries[i].name);
                        paths[idx++] = fullpath;
                    }
                }
                if (s->callback) s->callback(paths, cnt, s->callback_userdata);
                for (int i = 0; i < cnt; i++) sofd_add_recent(s, paths[i]);
                sofd_close(s);
            }
        } else {
            for (int i = 0; i < s->entry_count; i++) {
                if (s->selected_indices[i]) {
                    char fullpath[1024];
                    snprintf(fullpath, sizeof(fullpath), "%s/%s", s->current_path, s->entries[i].name);
                    const char* path = fullpath;
                    if (s->callback) s->callback(&path, 1, s->callback_userdata);
                    sofd_add_recent(s, path);
                    sofd_close(s);
                    break;
                }
            }
        }
    }

    mu_end_window(ctx);
}

#endif /* MICROUI_FILE_PICKER_IMPLEMENTATION */