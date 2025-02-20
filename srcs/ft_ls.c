#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <ft_list.h>

#define FLAG_l 0x00000001 /* long format */
#define FLAG_R 0x00000002 /* recursive */
#define FLAG_a 0x00000004 /* show hidden files */
#define FLAG_r 0x00000008 /* reverse order */
#define FLAG_t 0x00000010 /* sort by modification time */

#define FLAG_f 0x00000020 /* display files without order, just as i encounter them */ 
#define FLAG_g 0x00000040 /* display files without owner */
#define FLAG_d 0x00000080 /* list directories themselves, not their contents */
#define FLAG_u 0x00000100 /* use time of last access */
#define FLAG_c 0x00000200 /* use time of last modification of the inode */

#define BUFFER_SIZE 1024

typedef struct t_file
{
    char name[PATH_MAX];
    char name_orig[PATH_MAX];
    size_t name_len;
    struct stat info;
    char link_target[PATH_MAX];
} t_file;

typedef enum
{
    false,
    true
} bool;

typedef struct dirs
{
    char path[PATH_MAX];
    size_t path_len;
} dirs_todo;

typedef struct {
    uid_t uid;
    char name[LOGIN_NAME_MAX];
} uid_cache_entry;

typedef struct {
    gid_t gid;
    char name[LOGIN_NAME_MAX];
} gid_cache_entry;

static uid_cache_entry *uid_cache = NULL;
static size_t uid_cache_count = 0;
static size_t uid_cache_capacity = 0;

static gid_cache_entry *gid_cache = NULL;
static size_t gid_cache_count = 0;
static size_t gid_cache_capacity = 0;


static t_file* g_files = NULL;
static int malloc_size = 10000;

static int ignore_write;

static int g_ws_cols;

#define write(fd, str, len) do { ignore_write = write(fd, str, len); } while (0)

#ifndef UNBUFFERED_OUTPUT
#define OUTPUT_BUFFER_SIZE 8192
static char output_buffer[OUTPUT_BUFFER_SIZE];
static int output_index = 0;

    static void flush_output()
    {
        if (output_index > 0)
        {
            write(STDOUT_FILENO, output_buffer, output_index);
            output_index = 0;
        }
    }


    static void buffered_write(const char *data, size_t len)
    {
        if (output_index + (int)len > OUTPUT_BUFFER_SIZE)
        {
            flush_output();
        }
        if (len > OUTPUT_BUFFER_SIZE)
        {
            write(STDOUT_FILENO, data, len);
            return;
        }
        memcpy(output_buffer + output_index, data, len);
        output_index += (int)len;
    }
#else
    #define flush_output() do {} while (0)
    #define buffered_write(data, len) write(STDOUT_FILENO, data, len)
#endif

static int parse_args(int argc, char** argv)
{
    int options = 0;
    int i;
    int j;

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0)
        {
            write(1, "Usage: ft_ls [options] [file...]\n", 32);
            write(1, "Options:\n", 9);
            write(1, "  -l  use a long listing format\n", 31);
            write(1, "  -R  list subdirectories recursively\n", 38);
            write(1, "  -a  do not ignore entries starting with .\n", 45);
            write(1, "  -r  reverse order while sorting\n", 35);
            write(1, "  -t  sort by modification time, newest first\n", 47);
            write(1, "  -f  do not sort, enable -aU, disable -ls\n", 44);
            write(1, "  -g  like -l, but do not list owner\n", 37);
            write(1, "  -d  list directories themselves, not their contents\n", 54);
            write(1, "  -u  with -lt: sort by, and show, access time\n", 48);
            write(1, "  -c  with -lt: sort by, and show, change time\n", 47);
            return 0;
        }

        if (argv[i][0] == '-')
        {
            j = 1;
            while (argv[i][j])
            {
                switch (argv[i][j])
                {
                    case 'l':
                        options |= FLAG_l;
                        break;
                    case 'R':
                        options |= FLAG_R;
                        break;
                    case 'a':
                        options |= FLAG_a;
                        break;
                    case 'r':
                        options |= FLAG_r;
                        break;
                    case 't':
                        options |= FLAG_t;
                        break;
                    case 'f':
                        options |= FLAG_a;
                        options |= FLAG_f;
                        break;
                    case 'g':
                        options |= FLAG_g;
                        options |= FLAG_l;
                        break;
                    case 'd':
                        options |= FLAG_d;
                        break;
                    case 'u':
                        options |= FLAG_u;
                        break;
                    case 'c':
                        options |= FLAG_c;
                        break;
                    case '-':
                        break;
                    default:
                        write(2, "ft_ls: invalid option -- ", 26);
                        write(2, &argv[i][j], 1);
                        write(2, "\n", 1);
                        write(2, "Try 'ft_ls --help' for more information.\n", 42);
                        return -1;
                }
                j++;
            }
        }
    }
    
    if (options & FLAG_f)
    {
        options &= ~FLAG_l;
        options &= ~FLAG_t;
    }

    return options;
}

#ifdef USE_MERGE_SORT
static int compare_by_name(const t_file *a, const t_file *b)
{
    return strcmp(a->name, b->name);
}

static int compare_by_time(const t_file *a, const t_file *b, int flags)
{
    time_t time_a = (flags & FLAG_u) ? a->info.st_atime :
                    (flags & FLAG_c) ? a->info.st_ctime :
                    a->info.st_mtime;
    time_t time_b = (flags & FLAG_u) ? b->info.st_atime :
                    (flags & FLAG_c) ? b->info.st_ctime :
                    b->info.st_mtime;

    if (time_a > time_b)
        return -1;
    else if (time_a < time_b)
        return 1;
    return compare_by_name(a, b);
}

/* TODO
 * Move it to top of the file
 */
static void merge(t_file *files, int left, int mid, int right, int flags);

static void merge_sort(t_file *files, int left, int right, int flags)
{
    if (left < right)
    {
        int mid = (left + right) / 2;
        merge_sort(files, left, mid, flags);
        merge_sort(files, mid + 1, right, flags);
        merge(files, left, mid, right, flags);
    }
}

static void merge(t_file *files, int left, int mid, int right, int flags)
{
    int n1 = mid - left + 1;
    int n2 = right - mid;
    t_file *L = malloc(n1 * sizeof(t_file));
    t_file *R = malloc(n2 * sizeof(t_file));
    
    for (int i = 0; i < n1; i++) L[i] = files[left + i];
    for (int i = 0; i < n2; i++) R[i] = files[mid + 1 + i];

    int i = 0, j = 0, k = left;
    while (i < n1 && j < n2)
    {
        int cmp = (flags & FLAG_t) ? compare_by_time(&L[i], &R[j], flags) : compare_by_name(&L[i], &R[j]);
        if ((flags & FLAG_r) ? cmp > 0 : cmp <= 0)
        {
            files[k++] = L[i++];
        }
        else
        {
            files[k++] = R[j++];
        }
    }

    while (i < n1) files[k++] = L[i++];
    while (j < n2) files[k++] = R[j++];
    free(L);
    free(R);
}
#else
int g_sort_flags = 0;

static int compare_files(const void *a, const void *b, int flags)
{
    const t_file *file_a = (const t_file *)a;
    const t_file *file_b = (const t_file *)b;

    int cmp;
    if (flags & FLAG_t)
    {
        time_t time_a = (flags & FLAG_u) ? file_a->info.st_atime :
                        (flags & FLAG_c) ? file_a->info.st_ctime :
                        file_a->info.st_mtime;
        time_t time_b = (flags & FLAG_u) ? file_b->info.st_atime :
                        (flags & FLAG_c) ? file_b->info.st_ctime :
                        file_b->info.st_mtime;

        if (time_a > time_b)
            cmp = -1;
        else if (time_a < time_b)
            cmp = 1;
        else
            cmp = strcmp(file_a->name, file_b->name);
    }
    else
    {
        cmp = strcmp(file_a->name, file_b->name);
    }

    return (flags & FLAG_r) ? -cmp : cmp;
}

static int qsort_comparator(const void *a, const void *b)
{
    extern int g_sort_flags;
    return compare_files(a, b, g_sort_flags);
}

void sort_files(t_file *files, int count, int flags)
{
    extern int g_sort_flags;
    g_sort_flags = flags;
    qsort(files, count, sizeof(t_file), qsort_comparator);
}
#endif

static void get_permissions(mode_t mode, char *buffer)
{
    buffer[0] = S_ISDIR(mode) ? 'd' : S_ISLNK(mode) ? 'l' : '-';
    buffer[1] = (mode & S_IRUSR) ? 'r' : '-';
    buffer[2] = (mode & S_IWUSR) ? 'w' : '-';
    buffer[3] = (mode & S_IXUSR) ? 'x' : '-';
    buffer[4] = (mode & S_IRGRP) ? 'r' : '-';
    buffer[5] = (mode & S_IWGRP) ? 'w' : '-';
    buffer[6] = (mode & S_IXGRP) ? 'x' : '-';
    buffer[7] = (mode & S_IROTH) ? 'r' : '-';
    buffer[8] = (mode & S_IWOTH) ? 'w' : '-';
    buffer[9] = (mode & S_IXOTH) ? 'x' : '-';
    buffer[10] = '\0';
}

static void format_time(time_t file_time, char *buffer, size_t size)
{
    struct tm *time_info = localtime(&file_time);
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    if (size < 13)
    {
        if (size > 0) buffer[0] = '\0';
        return;
    }

    const char *month = months[time_info->tm_mon];
    int day = time_info->tm_mday;
    int hour = time_info->tm_hour;
    int minute = time_info->tm_min;

    buffer[0] = month[0];
    buffer[1] = month[1];
    buffer[2] = month[2];
    buffer[3] = ' ';
    buffer[4] = '0' + (day / 10);
    buffer[5] = '0' + (day % 10);
    buffer[6] = ' ';
    buffer[7] = '0' + (hour / 10);
    buffer[8] = '0' + (hour % 10);
    buffer[9] = ':';
    buffer[10] = '0' + (minute / 10);
    buffer[11] = '0' + (minute % 10);
    buffer[12] = '\0';
}

static void calculate_field_widths(t_file *files, int count, int *link_width, int *owner_width, int *group_width, int *size_width)
{
    *link_width = *owner_width = *group_width = *size_width = 0;

    for (int i = 0; i < count; i++)
    {
        int link_count = files[i].info.st_nlink;
        int link_len = 1;
        while (link_count /= 10) link_len++;
        if (link_len > *link_width) *link_width = link_len;

        struct passwd *user = getpwuid(files[i].info.st_uid);
        if (user)
        {
            int owner_len = strlen(user->pw_name);
            if (owner_len > *owner_width) *owner_width = owner_len;
        }

        struct group *group = getgrgid(files[i].info.st_gid);
        if (group)
        {
            int group_len = strlen(group->gr_name);
            if (group_len > *group_width) *group_width = group_len;
        }

        off_t file_size = files[i].info.st_size;
        int size_len = 1;
        while (file_size /= 10) size_len++;
        if (size_len > *size_width) *size_width = size_len;
    }
}

static void cache_uid_name(uid_t uid, const char *name)
{
    if (uid_cache_count == uid_cache_capacity)
    {
        uid_cache_capacity = uid_cache_capacity == 0 ? 64 : uid_cache_capacity * 2;
        uid_cache = realloc(uid_cache, uid_cache_capacity * sizeof(uid_cache_entry));
    }
    uid_cache[uid_cache_count].uid = uid;
    memcpy(uid_cache[uid_cache_count].name, name, strlen(name) + 1);
    uid_cache_count++;
}

static const char* get_uid_name(uid_t uid)
{
    for (size_t i = 0; i < uid_cache_count; i++)
        if (uid_cache[i].uid == uid)
            return uid_cache[i].name;

    struct passwd *pw = getpwuid(uid);
    if (!pw)
        return "";
    cache_uid_name(uid, pw->pw_name);
    return pw->pw_name;
}

static void cache_gid_name(gid_t gid, const char *name)
{
    if (gid_cache_count == gid_cache_capacity)
    {
        gid_cache_capacity = gid_cache_capacity == 0 ? 64 : gid_cache_capacity * 2;
        gid_cache = realloc(gid_cache, gid_cache_capacity * sizeof(gid_cache_entry));
    }
    gid_cache[gid_cache_count].gid = gid;
    memcpy(gid_cache[gid_cache_count].name, name, strlen(name) + 1);
    gid_cache_count++;
}

static const char* get_gid_name(gid_t gid)
{
    for (size_t i = 0; i < gid_cache_count; i++)
        if (gid_cache[i].gid == gid)
            return gid_cache[i].name;

    struct group *gr = getgrgid(gid);
    if (!gr)
        return "UNKNOWN";

    cache_gid_name(gid, gr->gr_name);
    return gr->gr_name;
}

static void free_caches()
{
    free(uid_cache);
    free(gid_cache);
}

static void display_files(t_file *files, int count, int flags, size_t max_name_length)
{
    char permissions[11];
    char time_buffer[20];

    if (flags & FLAG_l)
    {
        int link_width, owner_width, group_width, size_width;
        calculate_field_widths(files, count, &link_width, &owner_width, &group_width, &size_width);

        char buffer[BUFFER_SIZE];
        char temp_buffer[20];

        for (int i = 0; i < count; i++)
        {
            int buffer_index = 0;

            get_permissions(files[i].info.st_mode, permissions);
            memcpy(buffer + buffer_index, permissions, 10);
            buffer_index += 10;
            buffer[buffer_index++] = ' ';

            long link_count = files[i].info.st_nlink;
            int link_digits = 0;
            long temp = link_count;
            do
            {
                temp /= 10;
                link_digits++;
            } while (temp > 0);

            for (int j = 0; j < link_width - link_digits; j++)
            {
                buffer[buffer_index++] = ' ';
            }

            for (int j = link_digits - 1; j >= 0; j--)
            {
                temp_buffer[j] = '0' + (link_count % 10);
                link_count /= 10;
            }
            memcpy(buffer + buffer_index, temp_buffer, link_digits);
            buffer_index += link_digits;
            buffer[buffer_index++] = ' ';

            if (!(flags & FLAG_g))
            {
                const char *owner_name = (flags & FLAG_g) ? "" : get_uid_name(files[i].info.st_uid);
                size_t owner_len = strlen(owner_name);
                memcpy(buffer + buffer_index, owner_name, owner_len);
                buffer_index += owner_len;

                for (unsigned long j = 0; j < owner_width - owner_len + 1; j++)
                {
                    buffer[buffer_index++] = ' ';
                }
            }

            const char *group_name = get_gid_name(files[i].info.st_gid);

            size_t group_len = strlen(group_name);
            memcpy(buffer + buffer_index, group_name, group_len);
            buffer_index += group_len;

            for (unsigned long j = 0; j < group_width - group_len + 1; j++)
            {
                buffer[buffer_index++] = ' ';
            }

            long file_size = files[i].info.st_size;
            int size_digits = 0;
            temp = file_size;
            do
            {
                temp /= 10;
                size_digits++;
            } while (temp > 0);

            for (int j = 0; j < size_width - size_digits; j++)
            {
                buffer[buffer_index++] = ' ';
            }

            for (int j = size_digits - 1; j >= 0; j--)
            {
                temp_buffer[j] = '0' + (file_size % 10);
                file_size /= 10;
            }
            memcpy(buffer + buffer_index, temp_buffer, size_digits);
            buffer_index += size_digits;
            buffer[buffer_index++] = ' ';

            format_time(files[i].info.st_mtime, time_buffer, sizeof(time_buffer));
            size_t time_len = strlen(time_buffer);
            memcpy(buffer + buffer_index, time_buffer, time_len);
            buffer_index += time_len;
            buffer[buffer_index++] = ' ';

            memcpy(buffer + buffer_index, files[i].name_orig, files[i].name_len);
            buffer_index += files[i].name_len;

            if (files[i].link_target[0] != '\0')
                buffer_index += snprintf(buffer + buffer_index, BUFFER_SIZE - buffer_index, " -> %s", files[i].link_target);

            buffer[buffer_index++] = '\n';

            buffered_write(buffer, buffer_index);
        }
    }
    else
    {
        size_t column_width = max_name_length + 2;
        int columns = g_ws_cols / column_width;
        if (columns == 0) columns = 1;

        int rows = (count + columns - 1) / columns;

        char buffer[BUFFER_SIZE];
        int buffer_index = 0;

        for (int row = 0; row < rows; row++)
        {
            for (int col = 0; col < columns; col++)
            {
                int index = col * rows + row;
                if (index >= count) break;

                memcpy(buffer + buffer_index, files[index].name_orig, files[index].name_len);
                buffer_index += files[index].name_len;

                int padding = column_width - files[index].name_len;
                for (int j = 0; j < padding; j++) {
                    buffer[buffer_index++] = ' ';
                }
            }

            buffer[buffer_index++] = '\n';

            if ((unsigned long)buffer_index >= BUFFER_SIZE - column_width)
            {
                buffered_write(buffer, buffer_index);
                buffer_index = 0;
            }
        }

        if (buffer_index > 0)
        {
            buffered_write(buffer, buffer_index);
        }
    }
}

static void to_lowercase(char *str)
{
    for (; *str; str++) {
        if (*str >= 'A' && *str <= 'Z') {
            *str = *str + ('a' - 'A');
        }
    }
}

static bool open_directory(const char *path, DIR **dir)
{
    if (access(path, F_OK) == -1)
    {
        write(2, "ft_ls: Cannot access '", 21);
        write(2, path, strlen(path));
        write(2, "': ", 3);
        perror("");
        return false;
    }

    *dir = opendir(path);
    if (*dir == NULL)
    {
        write(2, "ft_ls: Cannot open directory '", 29);
        write(2, path, strlen(path));
        write(2, "': ", 3);
        perror("");
        return false;
    }
    return true;
}

static void list_directory(const char *path, int options, DIR* dir)
{
    struct dirent *entry;
    struct stat file_stat;
    bool need_stat = (options & FLAG_l) || (options & FLAG_t) || (options & FLAG_R);

    if (g_files == NULL)
        g_files = malloc(malloc_size * sizeof(t_file));
    
    int index = 0;
    char full_path[PATH_MAX];
    size_t path_len;
    size_t name_len;
    size_t max_len = 0;

    path_len = strlen(path);

    memcpy(full_path, path, path_len);

    if (full_path[path_len - 1] != '/')
    {
        full_path[path_len] = '/';
        path_len++;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_name[0] == '.' && !(options & FLAG_a))
            continue;

        name_len = strlen(entry->d_name);
        
        memcpy(full_path + path_len, entry->d_name, name_len + 1);
        full_path[path_len + name_len] = '\0';
        if (need_stat)
        {
            if (fstatat(dirfd(dir), entry->d_name, &file_stat, AT_SYMLINK_NOFOLLOW) == -1)
            {
                write(2, "ft_ls: Cannot stat file '", 25);
                write(2, full_path, strlen(full_path));
                write(2, "': ", 3);
                perror("");
                continue;
            }

            if (S_ISLNK(file_stat.st_mode))
            {
                ssize_t link_len = readlink(full_path, g_files[index].link_target, PATH_MAX);
                if (link_len == -1)
                {
                    write(2, "ft_ls: Cannot read link '", 25);
                    write(2, full_path, strlen(full_path));
                    write(2, "': ", 3);
                    perror("");
                    continue;
                }
                g_files[index].link_target[link_len] = '\0';
            }
            else
            {
                g_files[index].link_target[0] = '\0';
            }
        }

        if (entry->d_name[0] == '.' && entry->d_name[1] != '\0')
            memcpy(g_files[index].name, entry->d_name + 1, name_len);
        else
            memcpy(g_files[index].name, entry->d_name, name_len + 1);

        memcpy(g_files[index].name_orig, entry->d_name, name_len + 1);
        
        to_lowercase(g_files[index].name);

        g_files[index].name_len = name_len;
        if (!(options & FLAG_l) && name_len > max_len)
        {
            max_len = name_len;
        }
        
        if (need_stat)
            g_files[index].info = file_stat;

        index++;
        if (index >= malloc_size)
        {
            malloc_size += 10000;
            g_files = realloc(g_files, malloc_size * sizeof(t_file));
        }

    }

    closedir(dir);

#ifdef USE_MERGE_SORT
    if (!(options & FLAG_f))
        merge_sort(g_files, 0, index - 1, options);
#else
    if (!(options & FLAG_f))
        sort_files(g_files, index, options);
#endif

    display_files(g_files, index, options, max_len);

    if (options & FLAG_R)
    {
        path_len = strlen(path);
        int dir_capacity = 100;
        dirs_todo *dir_entries = malloc(dir_capacity * sizeof(dirs_todo));
        int dirs_index = 0;

        for (int i = 0; i < index; i++)
        {
            if (S_ISDIR(g_files[i].info.st_mode) &&
                strcmp(g_files[i].name, ".") != 0 &&
                strcmp(g_files[i].name, "..") != 0)
            {

                memcpy(dir_entries[dirs_index].path, path, path_len);
                dir_entries[dirs_index].path[path_len] = '/';
                memcpy(dir_entries[dirs_index].path + path_len + 1, g_files[i].name_orig, g_files[i].name_len + 1);
                dir_entries[dirs_index].path[path_len + g_files[i].name_len + 1] = '\0';
                dir_entries[dirs_index].path_len = path_len + g_files[i].name_len + 1;

                dirs_index++;

                if (dirs_index >= dir_capacity)
                {
                    dir_capacity += 1000;
                    dir_entries = realloc(dir_entries, sizeof(dirs_todo) * dir_capacity);
                }
            }
        }

        char buffer[BUFFER_SIZE];
        for (int i = 0; i < dirs_index; i++)
        {
            DIR* subdir;
            if (!open_directory(dir_entries[i].path, &subdir))
            {
                continue;
            }

            int len = 0;
            buffer[len++] = '\n';
            memcpy(buffer + len, dir_entries[i].path, dir_entries[i].path_len);
            len += dir_entries[i].path_len;
            buffer[len++] = ':';
            buffer[len++] = '\n';

            buffered_write(buffer, len);
            
            list_directory(dir_entries[i].path, options, subdir);
        }
        free(dir_entries);
    }
}

void set_g_ws_cols(int options)
{
    if (options & FLAG_l)
    {
        return;
    }
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        ws.ws_col = 80;
    }
    g_ws_cols = ws.ws_col;
}

int main(int argc, char **argv)
{
    int options = 0;
    DIR *dir;
    if (argc == 1)
    {
        set_g_ws_cols(options);
        if (open_directory(".", &dir))
            list_directory(".", options, dir);
        flush_output();
        free(g_files);
        return 0;
    }

    options = parse_args(argc, argv);

    if (options == -1)
    {
        return 1;
    }

    set_g_ws_cols(options);

    bool called_once = false;
    bool more_than_one = false;

    for (int i =1; i < argc; i++)
    {
        if (argv[i][0] != '-')
        {
            if (called_once)
            {
                more_than_one = true;
                break;
            }
            called_once = true;
        }
    }

    called_once = false;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-')
        {
            called_once = true;
            if (open_directory(argv[i], &dir))
            {
                if (more_than_one)
                {
                    buffered_write(argv[i], strlen(argv[i]));
                    buffered_write(":\n", 2);
                }
                list_directory(argv[i], options, dir);
            }
        }
    }

    if (!called_once)
    {
        if (open_directory(".", &dir))
            list_directory(".", options, dir);
    }

    flush_output();

    free_caches();
    free(g_files);
    return 0;
}
