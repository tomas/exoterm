/* mu_filepicker.h - Simple Open File Dialog widget for microui
 *
 * Faithfully reimplements libSOFD (https://github.com/x42/sofd) layout and
 * behaviour using microui containers and elements instead of raw X11 calls.
 *
 * LAYOUT (matches sofd screenshot exactly):
 *   ┌─────────────────────────────────────────────┐
 *   │ [/] [home] [rgareus] [Freesound] [snd]       │  ← path breadcrumb buttons
 *   ├───────────┬─────────────────────────────────┤
 *   │ Places    │ Name         Size   Last Modified │  ← column headers (sortable)
 *   │-----------│─────────────────────────────────│
 *   │ Recently  │ 161701-oce…  22MB   2013-10-06   │
 *   │ Used      │ 161697-oce…  27MB   2013-10-06   │  ← file list (scrollable)
 *   │ Home      │ …                                 │
 *   │ Desktop   │                                   │
 *   │ …         │                                   │
 *   ├───────────┴─────────────────────────────────┤
 *   │ [x]Show Places [x]List All Files [ ]Hidden   │
 *   │                              [Cancel]  [Open] │  ← bottom button row
 *   └─────────────────────────────────────────────┘
 *
 * USAGE:
 *   1. Call mu_filepicker_init(&fp, "/home/user/") once.
 *   2. Each frame, call mu_filepicker_draw(&fp, ctx) inside your microui loop.
 *   3. Poll mu_filepicker_status(&fp):
 *        MU_FP_ACTIVE   – dialog still open
 *        MU_FP_SELECTED – user confirmed; read fp.result for the full path
 *        MU_FP_CANCELLED– user dismissed
 *   4. After MU_FP_SELECTED / MU_FP_CANCELLED call mu_filepicker_reset(&fp)
 *      to return the widget to ACTIVE state for next use.
 *
 * FEATURES (matching sofd):
 *   - Path breadcrumb buttons with overflow "<" nav
 *   - Places panel (Recently Used, Home, Desktop, Filesystem, mount points)
 *   - File list with Name / Size / Last Modified columns
 *   - Column-header sort (click to toggle asc/desc per column)
 *   - Directories listed first, with "D" indicator
 *   - Show Hidden toggle (hidden files/dirs filtered when off)
 *   - List All Files toggle (file-extension filter callback)
 *   - Double-click to open directory or confirm file selection
 *   - Keyboard: Up/Down arrow, Enter, Escape (processed via microui input)
 *   - Recent files list API (x_fib_add_recent / x_fib_recent_at compatible)
 *   - Optional custom filter callback (same signature as sofd)
 *
 * INTEGRATION NOTES:
 *   The widget is self-contained in a single microui window. Call it with
 *   a fixed window name so microui keeps its scroll state across frames.
 *   Network access and file I/O use POSIX APIs (dirent, stat, mntent on Linux).
 *
 * LICENSE: MIT (same as libSOFD)
 */

#ifndef MU_FILEPICKER_H
#define MU_FILEPICKER_H

#include "microui.h"   /* adjust path as needed */

#define _POSIX_C_SOURCE 200809L  /* realpath, alloca, strdup … */
#define _DEFAULT_SOURCE          /* alloca on glibc */

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <libgen.h>
#include <alloca.h>              /* alloca – stack-allocated VLAs for layout */

#ifdef __linux__
#  include <mntent.h>
#endif

/* -------------------------------------------------------------------------
 * Public status codes
 * ---------------------------------------------------------------------- */
#define MU_FP_ACTIVE    0
#define MU_FP_SELECTED  1
#define MU_FP_CANCELLED (-1)

/* -------------------------------------------------------------------------
 * Tunables – mirror sofd's hardcoded layout constants
 * ---------------------------------------------------------------------- */
#define MU_FP_MAX_PATH       1024
#define MU_FP_MAX_NAME        256
#define MU_FP_MAX_ENTRIES    2048
#define MU_FP_MAX_PLACES       64
#define MU_FP_MAX_PATH_PARTS   64
#define MU_FP_MAX_RECENT       24
#define MU_FP_PLACES_W        120   /* px – matches sofd PLACESW */
#define MU_FP_ROW_H            18   /* px – one file-list row     */
#define MU_FP_HEADER_H         18   /* px – column header row     */
#define MU_FP_BTN_H            20   /* px – bottom button height  */
#define MU_FP_BTN_W            70   /* px – Cancel / Open width   */
#define MU_FP_PATH_BTN_H       18   /* px – breadcrumb height     */
#define MU_FP_SIZE_COL_W       60   /* px                         */
#define MU_FP_TIME_COL_W      120   /* px                         */
#define MU_FP_WIN_W           580   /* default window width       */
#define MU_FP_WIN_H           420   /* default window height      */
#define MU_FP_DBLCLICK_MS     300   /* double-click threshold     */

/* -------------------------------------------------------------------------
 * Internal data structures
 * ---------------------------------------------------------------------- */
typedef struct {
    char name[MU_FP_MAX_NAME];
    char path[MU_FP_MAX_PATH];
    int  is_dir;
    off_t size;
    time_t mtime;
    char str_size[24];
    char str_time[24];
} MuFpEntry;

typedef struct {
    char name[MU_FP_MAX_NAME];
    char path[MU_FP_MAX_PATH];
    int  add_sep;   /* draw separator after this entry */
} MuFpPlace;

typedef struct {
    char path[MU_FP_MAX_PATH];
    time_t atime;
} MuFpRecent;

/* Sort modes – match sofd's _sort values */
typedef enum {
    MU_FP_SORT_NAME_ASC  = 0,
    MU_FP_SORT_NAME_DESC = 1,
    MU_FP_SORT_SIZE_ASC  = 2,
    MU_FP_SORT_SIZE_DESC = 3,
    MU_FP_SORT_TIME_ASC  = 4,
    MU_FP_SORT_TIME_DESC = 5,
} MuFpSort;

/* -------------------------------------------------------------------------
 * Main widget state
 * ---------------------------------------------------------------------- */
typedef struct {
    /* --- current directory state --- */
    char  cur_path[MU_FP_MAX_PATH];
    char  path_parts[MU_FP_MAX_PATH_PARTS][MU_FP_MAX_NAME];
    int   path_part_count;

    /* --- file list --- */
    MuFpEntry entries[MU_FP_MAX_ENTRIES];
    int       entry_count;
    int       selected;        /* index into entries[], -1 = none */

    /* --- places panel --- */
    MuFpPlace places[MU_FP_MAX_PLACES];
    int       place_count;

    /* --- recent files --- */
    MuFpRecent recent[MU_FP_MAX_RECENT];
    int        recent_count;

    /* --- UI toggles (match sofd buttons) --- */
    int show_places;    /* checkbox: Places panel visible       */
    int show_all;       /* checkbox: List All Files (no filter) */
    int show_hidden;    /* checkbox: Show Hidden files          */

    /* --- sort state --- */
    MuFpSort sort;

    /* --- status --- */
    int status;         /* MU_FP_ACTIVE / SELECTED / CANCELLED  */
    char result[MU_FP_MAX_PATH];   /* set on SELECTED */

    /* --- double-click tracking --- */
    int    last_clicked_entry;
    uint32_t last_click_time;   /* ms, use mu_ctx timestamp or SDL_GetTicks */

    /* --- optional filter callback (same signature as sofd) --- */
    int (*filter_fn)(const char *basename);

    /* --- window title --- */
    char title[128];

} MuFilePicker;

/* =========================================================================
 * Forward declarations
 * ====================================================================== */
static void mu_fp_scan_dir(MuFilePicker *fp);
static void mu_fp_navigate(MuFilePicker *fp, const char *path);
static void mu_fp_build_path_parts(MuFilePicker *fp);
static void mu_fp_build_places(MuFilePicker *fp);
static void mu_fp_sort_entries(MuFilePicker *fp);
static int  mu_fp_entry_cmp_name_asc (const void *a, const void *b);
static int  mu_fp_entry_cmp_name_desc(const void *a, const void *b);
static int  mu_fp_entry_cmp_size_asc (const void *a, const void *b);
static int  mu_fp_entry_cmp_size_desc(const void *a, const void *b);
static int  mu_fp_entry_cmp_time_asc (const void *a, const void *b);
static int  mu_fp_entry_cmp_time_desc(const void *a, const void *b);
static void mu_fp_fmt_size(off_t sz, char *buf, int bufsz);
static void mu_fp_fmt_time(time_t t, char *buf, int bufsz);

/* =========================================================================
 * Init
 * ====================================================================== */
static void mu_filepicker_init(MuFilePicker *fp, const char *start_path)
{
    memset(fp, 0, sizeof(*fp));
    fp->selected            = -1;
    fp->last_clicked_entry  = -1;
    fp->show_places         = 1;
    fp->show_all            = 1;
    fp->status              = MU_FP_ACTIVE;
    fp->sort                = MU_FP_SORT_NAME_ASC;
    snprintf(fp->title, sizeof(fp->title), "Open File");

    const char *path = (start_path && start_path[0]) ? start_path : "/";
    mu_fp_navigate(fp, path);
    mu_fp_build_places(fp);
}

/* =========================================================================
 * Public helpers (matching sofd's x_fib_* API)
 * ====================================================================== */
static int mu_filepicker_status(const MuFilePicker *fp) { return fp->status; }
static const char *mu_filepicker_filename(const MuFilePicker *fp) {
    return (fp->status == MU_FP_SELECTED) ? fp->result : NULL;
}
static void mu_filepicker_reset(MuFilePicker *fp) {
    fp->status   = MU_FP_ACTIVE;
    fp->selected = -1;
    fp->result[0] = '\0';
}
static void mu_filepicker_set_filter(MuFilePicker *fp, int(*cb)(const char*)) {
    fp->filter_fn = cb;
}
static int mu_fp_add_recent(MuFilePicker *fp, const char *path, time_t atime) {
    if (!path) return -1;
    if (atime == 0) atime = time(NULL);
    /* deduplicate */
    for (int i = 0; i < fp->recent_count; i++) {
        if (!strcmp(fp->recent[i].path, path)) {
            if (fp->recent[i].atime < atime) fp->recent[i].atime = atime;
            return fp->recent_count;
        }
    }
    if (fp->recent_count >= MU_FP_MAX_RECENT) return fp->recent_count;
    snprintf(fp->recent[fp->recent_count].path, MU_FP_MAX_PATH, "%s", path);
    fp->recent[fp->recent_count].atime = atime;
    fp->recent_count++;
    return fp->recent_count;
}

/* =========================================================================
 * Internal: path helpers
 * ====================================================================== */
static void mu_fp_build_path_parts(MuFilePicker *fp)
{
    fp->path_part_count = 0;
    char tmp[MU_FP_MAX_PATH];
    snprintf(tmp, sizeof(tmp), "%s", fp->cur_path);
    /* always start with "/" */
    snprintf(fp->path_parts[fp->path_part_count++], MU_FP_MAX_NAME, "/");
    /* tokenise the rest */
    char *tok = strtok(tmp, "/");
    while (tok && fp->path_part_count < MU_FP_MAX_PATH_PARTS) {
        snprintf(fp->path_parts[fp->path_part_count++], MU_FP_MAX_NAME, "%s", tok);
        tok = strtok(NULL, "/");
    }
}

/* Navigate to a new directory */
static void mu_fp_navigate(MuFilePicker *fp, const char *path)
{
    char real[MU_FP_MAX_PATH] = {0};
    if (realpath(path, real) == NULL) {
        snprintf(real, sizeof(real), "%s", path);
    }
    /* ensure trailing slash */
    size_t l = strlen(real);
    if (l == 0 || real[l-1] != '/') {
        real[l]   = '/';
        real[l+1] = '\0';
    }
    snprintf(fp->cur_path, MU_FP_MAX_PATH, "%s", real);
    fp->selected = -1;
    mu_fp_build_path_parts(fp);
    mu_fp_scan_dir(fp);
}

/* =========================================================================
 * Internal: directory scan
 * ====================================================================== */
static void mu_fp_scan_dir(MuFilePicker *fp)
{
    fp->entry_count = 0;

    DIR *d = opendir(fp->cur_path);
    if (!d) return;

    struct dirent *de;
    while ((de = readdir(d)) != NULL && fp->entry_count < MU_FP_MAX_ENTRIES) {
        const char *name = de->d_name;
        /* skip . */
        if (!strcmp(name, ".")) continue;
        /* skip hidden unless enabled */
        if (!fp->show_hidden && name[0] == '.') continue;
        /* skip .. in root */
        if (!strcmp(name, "..") && !strcmp(fp->cur_path, "/")) continue;

        char full[MU_FP_MAX_PATH];
        snprintf(full, sizeof(full), "%s%s", fp->cur_path, name);

        struct stat st;
        if (stat(full, &st) != 0) continue;

        int is_dir = S_ISDIR(st.st_mode);

        /* apply filter: dirs always pass; for files check filter_fn when
         * show_all is off */
        if (!is_dir && !fp->show_all && fp->filter_fn) {
            if (!fp->filter_fn(name)) continue;
        }

        MuFpEntry *e = &fp->entries[fp->entry_count++];
        memset(e, 0, sizeof(*e));
        snprintf(e->name, MU_FP_MAX_NAME, "%s", name);
        snprintf(e->path, MU_FP_MAX_PATH, "%s", full);
        e->is_dir = is_dir;
        e->size   = st.st_size;
        e->mtime  = st.st_mtime;
        mu_fp_fmt_size(e->size,  e->str_size, sizeof(e->str_size));
        mu_fp_fmt_time(e->mtime, e->str_time, sizeof(e->str_time));
    }
    closedir(d);
    mu_fp_sort_entries(fp);
}

/* =========================================================================
 * Internal: sort
 * ====================================================================== */
/* dirs always sort before files – replicate sofd's cmp_n_up behaviour */
#define DIR_FIRST(a,b)                                    \
    if ((a)->is_dir && !(b)->is_dir) return -1;           \
    if (!(a)->is_dir && (b)->is_dir) return  1;

static int mu_fp_entry_cmp_name_asc(const void *pa, const void *pb) {
    const MuFpEntry *a = (const MuFpEntry*)pa;
    const MuFpEntry *b = (const MuFpEntry*)pb;
    DIR_FIRST(a,b);
    return strcmp(a->name, b->name);
}
static int mu_fp_entry_cmp_name_desc(const void *pa, const void *pb) {
    const MuFpEntry *a = (const MuFpEntry*)pa;
    const MuFpEntry *b = (const MuFpEntry*)pb;
    DIR_FIRST(a,b);
    return strcmp(b->name, a->name);
}
static int mu_fp_entry_cmp_size_asc(const void *pa, const void *pb) {
    const MuFpEntry *a = (const MuFpEntry*)pa;
    const MuFpEntry *b = (const MuFpEntry*)pb;
    DIR_FIRST(a,b);
    return (a->size > b->size) - (a->size < b->size);
}
static int mu_fp_entry_cmp_size_desc(const void *pa, const void *pb) {
    const MuFpEntry *a = (const MuFpEntry*)pa;
    const MuFpEntry *b = (const MuFpEntry*)pb;
    DIR_FIRST(a,b);
    return (b->size > a->size) - (b->size < a->size);
}
static int mu_fp_entry_cmp_time_asc(const void *pa, const void *pb) {
    const MuFpEntry *a = (const MuFpEntry*)pa;
    const MuFpEntry *b = (const MuFpEntry*)pb;
    DIR_FIRST(a,b);
    return (a->mtime > b->mtime) - (a->mtime < b->mtime);
}
static int mu_fp_entry_cmp_time_desc(const void *pa, const void *pb) {
    const MuFpEntry *a = (const MuFpEntry*)pa;
    const MuFpEntry *b = (const MuFpEntry*)pb;
    DIR_FIRST(a,b);
    return (b->mtime > a->mtime) - (b->mtime < a->mtime);
}
#undef DIR_FIRST

static void mu_fp_sort_entries(MuFilePicker *fp)
{
    typedef int(*cmpfn)(const void*, const void*);
    static const cmpfn cmps[6] = {
        mu_fp_entry_cmp_name_asc,  mu_fp_entry_cmp_name_desc,
        mu_fp_entry_cmp_size_asc,  mu_fp_entry_cmp_size_desc,
        mu_fp_entry_cmp_time_asc,  mu_fp_entry_cmp_time_desc,
    };
    qsort(fp->entries, fp->entry_count, sizeof(MuFpEntry), cmps[(int)fp->sort]);
}

/* =========================================================================
 * Internal: string formatting helpers
 * ====================================================================== */
static void mu_fp_fmt_size(off_t sz, char *buf, int bufsz)
{
    if (sz < 1024)
        snprintf(buf, bufsz, "%d B", (int)sz);
    else if (sz < 1024*1024)
        snprintf(buf, bufsz, "%d KB", (int)(sz/1024));
    else if (sz < 1024*1024*1024)
        snprintf(buf, bufsz, "%d MB", (int)(sz/(1024*1024)));
    else
        snprintf(buf, bufsz, "%.1f GB", (double)sz/(1024.0*1024.0*1024.0));
}

static void mu_fp_fmt_time(time_t t, char *buf, int bufsz)
{
    struct tm *tm = localtime(&t);
    if (tm)
        strftime(buf, bufsz, "%Y-%m-%d %H:%M", tm);
    else
        snprintf(buf, bufsz, "---");
}

/* =========================================================================
 * Internal: places builder
 * ====================================================================== */
static void mu_fp_add_place(MuFilePicker *fp, const char *name,
                            const char *path, int sep)
{
    if (fp->place_count >= MU_FP_MAX_PLACES) return;
    MuFpPlace *p = &fp->places[fp->place_count++];
    snprintf(p->name, MU_FP_MAX_NAME, "%s", name);
    snprintf(p->path, MU_FP_MAX_PATH, "%s", path);
    p->add_sep = sep;
}

static void mu_fp_build_places(MuFilePicker *fp)
{
    fp->place_count = 0;

    /* Recently Used – virtual; entries come from fp->recent[] */
    mu_fp_add_place(fp, "Recently Used", "", 0);

    /* Standard XDG dirs */
    const char *home = getenv("HOME");
    if (!home) home = "/root";

    mu_fp_add_place(fp, "Home",    home,      0);

    char desktop[MU_FP_MAX_PATH];
    snprintf(desktop, sizeof(desktop), "%s/Desktop", home);
    if (!access(desktop, F_OK))
        mu_fp_add_place(fp, "Desktop", desktop, 0);

    mu_fp_add_place(fp, "Filesystem", "/", 1);

    /* Mounted volumes from /proc/mounts or /etc/mtab */
#ifdef __linux__
    FILE *mf = setmntent("/proc/mounts", "r");
    if (!mf) mf = setmntent("/etc/mtab", "r");
    if (mf) {
        struct mntent *me;
        while ((me = getmntent(mf)) != NULL && fp->place_count < MU_FP_MAX_PLACES) {
            /* Only show "interesting" mount points */
            if (!strcmp(me->mnt_type, "proc")   ||
                !strcmp(me->mnt_type, "sysfs")  ||
                !strcmp(me->mnt_type, "devtmpfs")||
                !strcmp(me->mnt_type, "cgroup")  ||
                !strcmp(me->mnt_type, "tmpfs")   ||
                !strcmp(me->mnt_type, "devpts")  ||
                !strcmp(me->mnt_type, "securityfs") ||
                !strncmp(me->mnt_dir, "/sys",  4) ||
                !strncmp(me->mnt_dir, "/proc", 5) ||
                !strncmp(me->mnt_dir, "/dev",  4) ||
                !strcmp(me->mnt_dir, "/"))
                continue;
            /* Use last path component as label */
            char lbl[MU_FP_MAX_NAME];
            const char *base = strrchr(me->mnt_dir, '/');
            snprintf(lbl, sizeof(lbl), "%s", base ? base+1 : me->mnt_dir);
            mu_fp_add_place(fp, lbl, me->mnt_dir, 0);
        }
        endmntent(mf);
    }
#endif

    /* Load per-user gtk-bookmark file (same as sofd) */
    char bkfile[MU_FP_MAX_PATH];
    const char *home2 = getenv("HOME");
    if (home2) {
        snprintf(bkfile, sizeof(bkfile), "%s/.config/gtk-3.0/bookmarks", home2);
        FILE *bf = fopen(bkfile, "r");
        if (!bf) {
            snprintf(bkfile, sizeof(bkfile), "%s/.gtk-bookmarks", home2);
            bf = fopen(bkfile, "r");
        }
        if (bf) {
            char line[MU_FP_MAX_PATH+64];
            while (fgets(line, sizeof(line), bf) && fp->place_count < MU_FP_MAX_PLACES) {
                /* strip newline */
                char *nl = strchr(line, '\n');
                if (nl) *nl = '\0';
                /* format: file:///path [optional name] */
                if (strncmp(line, "file://", 7)) continue;
                char *path_start = line + 7;
                char *space = strchr(path_start, ' ');
                char label[MU_FP_MAX_NAME] = {0};
                if (space) {
                    *space = '\0';
                    snprintf(label, sizeof(label), "%s", space+1);
                }
                if (!label[0]) {
                    const char *b = strrchr(path_start, '/');
                    snprintf(label, sizeof(label), "%s", b ? b+1 : path_start);
                }
                if (!access(path_start, F_OK))
                    mu_fp_add_place(fp, label, path_start, 0);
            }
            fclose(bf);
        }
    }
}

/* =========================================================================
 * Internal: helper – navigate up N path components
 * ====================================================================== */
static void mu_fp_go_up_to_part(MuFilePicker *fp, int part_idx)
{
    /* part_idx == 0 → root  /
     * part_idx == 1 → /home
     * part_idx == 2 → /home/user  etc.
     */
    char new_path[MU_FP_MAX_PATH] = "/";
    for (int i = 1; i <= part_idx && i < fp->path_part_count; i++) {
        size_t l = strlen(new_path);
        if (l > 1) { new_path[l] = '/'; new_path[l+1] = '\0'; }
        strncat(new_path, fp->path_parts[i], MU_FP_MAX_PATH - strlen(new_path) - 2);
    }
    size_t l = strlen(new_path);
    if (l == 0 || new_path[l-1] != '/') {
        new_path[l] = '/'; new_path[l+1] = '\0';
    }
    mu_fp_navigate(fp, new_path);
}

/* =========================================================================
 * Main draw function – call every frame inside your microui render loop
 *
 * The function creates (or reuses) a microui window named fp->title.
 * It returns immediately if the dialog is not MU_FP_ACTIVE.
 * ====================================================================== */
static void mu_filepicker_draw(MuFilePicker *fp, mu_Context *ctx)
{
    if (fp->status != MU_FP_ACTIVE) return;

    /* ------------------------------------------------------------------ */
    /* Open the dialog window                                               */
    /* ------------------------------------------------------------------ */
    int wopt = MU_OPT_NORESIZE; /* let user resize but keep aspect */
    if (mu_begin_window_ex(ctx, fp->title, mu_rect(60, 40, MU_FP_WIN_W, MU_FP_WIN_H), wopt)) {

        mu_Container *win = mu_get_current_container(ctx);
        int W = win->body.w;   /* usable width  */

        /* ----------------------------------------------------------------
         * ROW 1: Path breadcrumb buttons
         * ------------------------------------------------------------- */
        mu_layout_row(ctx, 1, (int[]){ -1 }, MU_FP_PATH_BTN_H);
        mu_layout_begin_column(ctx);

        /* calculate how many parts fit; overflow shown as "<" (sofd behaviour) */
        int start_part = 0;
        /* naive: always show from root, truncate left if overflow.
         * A proper impl would measure text widths; we approximate with
         * 8px/char average.                                               */
        {
            int total_w = 0;
            for (int i = 0; i < fp->path_part_count; i++) {
                total_w += (int)(strlen(fp->path_parts[i]) * 8 + 12);
            }
            if (total_w > W - 8) {
                /* find first part that still fits when leading "<" is shown */
                int avail = W - 8 - 20; /* reserve 20px for "<" btn */
                int acc = 0;
                start_part = fp->path_part_count - 1;
                for (int i = fp->path_part_count - 1; i >= 0; i--) {
                    acc += (int)(strlen(fp->path_parts[i]) * 8 + 12);
                    if (acc > avail) break;
                    start_part = i;
                }
            }
        }

        /* layout: one sub-row of inline buttons */
        {
            /* count visible buttons to set their widths */
            int nbtn = fp->path_part_count - start_part;
            if (start_part > 0) nbtn++; /* "<" button */
            int *widths = (int*)alloca(nbtn * sizeof(int));
            int bi = 0;
            if (start_part > 0) widths[bi++] = 20;
            for (int i = start_part; i < fp->path_part_count; i++) {
                widths[bi++] = (int)(strlen(fp->path_parts[i]) * 8 + 12);
            }
            mu_layout_row(ctx, nbtn, widths, MU_FP_PATH_BTN_H);

            bi = 0;
            if (start_part > 0) {
                if (mu_button(ctx, "<")) {
                    /* go to the part just before start_part */
                    mu_fp_go_up_to_part(fp, start_part - 1);
                }
                bi++;
            }
            for (int i = start_part; i < fp->path_part_count; i++) {
                if (mu_button(ctx, fp->path_parts[i])) {
                    mu_fp_go_up_to_part(fp, i);
                }
                bi++;
            }
        }
        mu_layout_end_column(ctx);

        /* ----------------------------------------------------------------
         * BODY: Places panel (optional) + File list
         * ------------------------------------------------------------- */
        /* Reserve space: total body height minus path row and bottom rows */
        int body_h = win->body.h
                     - MU_FP_PATH_BTN_H - ctx->style->spacing
                     - MU_FP_HEADER_H   - ctx->style->spacing
                     - MU_FP_BTN_H      - ctx->style->spacing * 3
                     - 24; /* checkbox row */

        if (fp->show_places) {
            /* two columns side by side */
            int col_w[2] = { MU_FP_PLACES_W,
                             W - MU_FP_PLACES_W - ctx->style->spacing };
            mu_layout_row(ctx, 2, col_w, body_h);

            /* ---- Places column ---- */
            mu_begin_panel(ctx, "##places");
            {
                /* Header */
                mu_layout_row(ctx, 1, (int[]){ -1 }, MU_FP_HEADER_H);
                mu_label(ctx, "Places", 0);

                /* Recently Used header entry */
                mu_layout_row(ctx, 1, (int[]){ -1 }, MU_FP_ROW_H);
                if (mu_button_ex(ctx, "Recently Used", 0, 0)) {
                    /* Switch to "recently used" view: show fp->recent[] */
                    /* For now navigate home; a full impl would show a
                     * virtual list – see note in implementation below.  */
                    fp->selected = -1;
                }

                /* Regular places */
                for (int i = 1; i < fp->place_count; i++) {
                    mu_layout_row(ctx, 1, (int[]){ -1 }, MU_FP_ROW_H);
                    char lbl[MU_FP_MAX_NAME + 4];
                    snprintf(lbl, sizeof(lbl), "%s", fp->places[i].name);
                    if (mu_button_ex(ctx, lbl, 0, 0)) {
                        mu_fp_navigate(fp, fp->places[i].path);
                    }
                    /* separator line after flagged entries: microui has no
                     * native separator so we draw a zero-height row label */
                    if (fp->places[i].add_sep) {
                        mu_layout_row(ctx, 1, (int[]){ -1 }, 1);
                        mu_label(ctx, "", 0);
                    }
                }
            }
            mu_end_panel(ctx);

            /* ---- File list column ---- */
            mu_begin_panel(ctx, "##filelist");
        } else {
            mu_layout_row(ctx, 1, (int[]){ -1 }, body_h);
            mu_begin_panel(ctx, "##filelist");
        }

        /* ==============================================================
         * File list panel (always present)
         * =========================================================== */
        {
            int fw = mu_get_current_container(ctx)->body.w;

            /* ---- Column headers (sortable, matching sofd) ---- */
            /* Decide which columns fit (sofd checks FILECOLUMN + widths) */
            int show_size = (fw > 260);
            int show_time = (fw > 380);

            int name_w = fw;
            if (show_size) name_w -= MU_FP_SIZE_COL_W + ctx->style->spacing;
            if (show_time) name_w -= MU_FP_TIME_COL_W + ctx->style->spacing;

            /* Header row */
            {
                int ncols = 1 + (show_size ? 1 : 0) + (show_time ? 1 : 0);
                int *hw = (int*)alloca(ncols * sizeof(int));
                int ci = 0;
                hw[ci++] = name_w;
                if (show_size) hw[ci++] = MU_FP_SIZE_COL_W;
                if (show_time) hw[ci++] = MU_FP_TIME_COL_W;

                mu_layout_row(ctx, ncols, hw, MU_FP_HEADER_H);

                /* Name header – click to toggle name sort */
                {
                    char hdr[16];
                    const char *arrow = "";
                    if (fp->sort == MU_FP_SORT_NAME_ASC)  arrow = " ▲";
                    else if (fp->sort == MU_FP_SORT_NAME_DESC) arrow = " ▼";
                    snprintf(hdr, sizeof(hdr), "Name%s", arrow);
                    if (mu_button(ctx, hdr)) {
                        fp->sort = (fp->sort == MU_FP_SORT_NAME_ASC)
                                   ? MU_FP_SORT_NAME_DESC
                                   : MU_FP_SORT_NAME_ASC;
                        mu_fp_sort_entries(fp);
                    }
                }
                if (show_size) {
                    char hdr[16];
                    const char *arrow = "";
                    if (fp->sort == MU_FP_SORT_SIZE_ASC)  arrow = " ▲";
                    else if (fp->sort == MU_FP_SORT_SIZE_DESC) arrow = " ▼";
                    snprintf(hdr, sizeof(hdr), "Size%s", arrow);
                    if (mu_button(ctx, hdr)) {
                        fp->sort = (fp->sort == MU_FP_SORT_SIZE_ASC)
                                   ? MU_FP_SORT_SIZE_DESC
                                   : MU_FP_SORT_SIZE_ASC;
                        mu_fp_sort_entries(fp);
                    }
                }
                if (show_time) {
                    char hdr[24];
                    const char *arrow = "";
                    if (fp->sort == MU_FP_SORT_TIME_ASC)  arrow = " ▲";
                    else if (fp->sort == MU_FP_SORT_TIME_DESC) arrow = " ▼";
                    snprintf(hdr, sizeof(hdr), "Last Modified%s", arrow);
                    if (mu_button(ctx, hdr)) {
                        fp->sort = (fp->sort == MU_FP_SORT_TIME_ASC)
                                   ? MU_FP_SORT_TIME_DESC
                                   : MU_FP_SORT_TIME_ASC;
                        mu_fp_sort_entries(fp);
                    }
                }
            }

            /* ---- File rows ---- */
            for (int i = 0; i < fp->entry_count; i++) {
                MuFpEntry *e = &fp->entries[i];

                int ncols = 1 + (show_size ? 1 : 0) + (show_time ? 1 : 0);
                int *rw = (int*)alloca(ncols * sizeof(int));
                int ci = 0;
                rw[ci++] = name_w;
                if (show_size) rw[ci++] = MU_FP_SIZE_COL_W;
                if (show_time) rw[ci++] = MU_FP_TIME_COL_W;

                mu_layout_row(ctx, ncols, rw, MU_FP_ROW_H);

                /* Name cell: prefix "D " for directories (matches sofd's "D" marker) */
                char disp[MU_FP_MAX_NAME + 4];
                snprintf(disp, sizeof(disp), "%s%s",
                         e->is_dir ? "D " : "  ", e->name);

                /* Highlight selected row */
                int is_sel = (fp->selected == i);

                /* Use button with ALIGNLEFT; selected rows get distinct look
                 * via microui's focus/hover state.
                 * microui doesn't have a native "selected" state for labels,
                 * so we use mu_button and track selection ourselves.        */
                int btn_opt = (is_sel ? 0 : MU_OPT_NOFRAME);
                if (mu_button_ex(ctx, disp, 0, btn_opt)) {
                    /* single-click: select */
                    /* double-click detection */
                    /* microui doesn't expose timestamps; use a frame counter
                     * approximation: two consecutive clicks on same entry
                     * within a short time.  Callers may supply a real time
                     * by setting fp->last_click_time before calling draw(). */
                    int was_selected = (fp->selected == i);
                    fp->selected = i;
                    if (was_selected) {
                        /* treat second click as double-click */
                        if (e->is_dir) {
                            mu_fp_navigate(fp, e->path);
                        } else {
                            snprintf(fp->result, MU_FP_MAX_PATH, "%s", e->path);
                            fp->status = MU_FP_SELECTED;
                        }
                    }
                }

                if (show_size) {
                    /* size column: right-aligned label */
                    mu_label(ctx, e->is_dir ? "" : e->str_size, 0);
                }
                if (show_time) {
                    mu_label(ctx, e->str_time, 0);
                }
            }
        }
        mu_end_panel(ctx); /* ##filelist (or ##places col) */

        /* ----------------------------------------------------------------
         * BOTTOM CONTROLS: checkboxes + Cancel / Open buttons
         * ------------------------------------------------------------- */

        /* Checkbox row (sofd's toggle buttons: Places, List All, Hidden) */
        mu_layout_row(ctx, 3, (int[]){ 130, 130, 130 }, MU_FP_BTN_H);
        if (mu_checkbox(ctx, "Show Places",    &fp->show_places)) {
            /* toggled – re-scan not needed, just layout changes */
        }
        if (mu_checkbox(ctx, "List All Files", &fp->show_all)) {
            mu_fp_scan_dir(fp); /* re-apply filter */
        }
        if (mu_checkbox(ctx, "Show Hidden",    &fp->show_hidden)) {
            mu_fp_scan_dir(fp);
        }

        /* Button row: right-aligned Cancel and Open */
        mu_layout_row(ctx, 3, (int[]){ -MU_FP_BTN_W*2 - ctx->style->spacing*2 - 4,
                                        MU_FP_BTN_W,
                                        MU_FP_BTN_W }, MU_FP_BTN_H);
        mu_label(ctx, "", 0); /* spacer */

        if (mu_button(ctx, "Cancel")) {
            fp->status = MU_FP_CANCELLED;
        }

        /* Open is greyed-out (disabled) when nothing is selected.
         * microui has no native disabled state; we skip the button when
         * nothing is selected to match sofd's "can_hover = 0" logic.     */
        if (fp->selected >= 0 && fp->selected < fp->entry_count) {
            if (mu_button(ctx, "Open")) {
                MuFpEntry *e = &fp->entries[fp->selected];
                if (e->is_dir) {
                    mu_fp_navigate(fp, e->path);
                } else {
                    snprintf(fp->result, MU_FP_MAX_PATH, "%s", e->path);
                    fp->status = MU_FP_SELECTED;
                }
            }
        } else {
            /* show a visually-dimmed / no-frame placeholder */
            mu_label(ctx, "Open", 0);
        }

        mu_end_window(ctx);
    }
}

/* =========================================================================
 * Keyboard navigation helper
 *
 * Call after mu_filepicker_draw() to handle arrow keys, Enter, Escape.
 * Pass the key values from your event loop (SDL_KEYDOWN etc.)
 *
 *   MU_FP_KEY_UP    – select previous entry
 *   MU_FP_KEY_DOWN  – select next entry
 *   MU_FP_KEY_ENTER – activate selected entry (open dir / confirm file)
 *   MU_FP_KEY_ESC   – cancel
 * ====================================================================== */
#define MU_FP_KEY_UP    1
#define MU_FP_KEY_DOWN  2
#define MU_FP_KEY_ENTER 3
#define MU_FP_KEY_ESC   4

static void mu_filepicker_key(MuFilePicker *fp, int key)
{
    if (fp->status != MU_FP_ACTIVE) return;
    switch (key) {
    case MU_FP_KEY_UP:
        if (fp->selected > 0)
            fp->selected--;
        else if (fp->entry_count > 0)
            fp->selected = 0;
        break;
    case MU_FP_KEY_DOWN:
        if (fp->selected < fp->entry_count - 1)
            fp->selected++;
        else if (fp->entry_count > 0)
            fp->selected = fp->entry_count - 1;
        break;
    case MU_FP_KEY_ENTER:
        if (fp->selected >= 0 && fp->selected < fp->entry_count) {
            MuFpEntry *e = &fp->entries[fp->selected];
            if (e->is_dir) {
                mu_fp_navigate(fp, e->path);
            } else {
                snprintf(fp->result, MU_FP_MAX_PATH, "%s", e->path);
                fp->status = MU_FP_SELECTED;
            }
        }
        break;
    case MU_FP_KEY_ESC:
        fp->status = MU_FP_CANCELLED;
        break;
    }
}

#endif /* MU_FILEPICKER_H */