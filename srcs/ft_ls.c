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
    size_t name_len;
    struct stat info;
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


t_file* g_files = NULL;
int malloc_size = 500;

int parse_args(int argc, char** argv)
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
            return -1;
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
                        options |= FLAG_f;
                        break;
                    case 'g':
                        options |= FLAG_g;
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
                    default:
                        write(2, "ft_ls: illegal option -- ", 26);
                        write(2, &argv[i][j], 1);
                        write(2, "\n", 1);
                        return -1;
                }
                j++;
            }
        }
    }
    
    return options;
}

int compare_by_name(const t_file *a, const t_file *b)
{
    int add_a = a->name[0] == '.';
    int add_b = b->name[0] == '.';
    return strcasecmp(a->name + add_a, b->name + add_b);
}

int compare_by_time(const t_file *a, const t_file *b, int flags)
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
void merge(t_file *files, int left, int mid, int right, int flags);

void merge_sort(t_file *files, int left, int right, int flags)
{
    if (left < right)
    {
        int mid = (left + right) / 2;
        merge_sort(files, left, mid, flags);
        merge_sort(files, mid + 1, right, flags);
        merge(files, left, mid, right, flags);
    }
}

void merge(t_file *files, int left, int mid, int right, int flags)
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

void get_permissions(mode_t mode, char *buffer)
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

void format_time(time_t file_time, char *buffer, size_t size)
{
    struct tm *time_info = localtime(&file_time);
    strftime(buffer, size, "%b %d %H:%M", time_info);
}

int get_max_name_length(t_file *files, int count)
{
    int max_length = 0;
    for (int i = 0; i < count; i++)
    {
        int length = files[i].name_len;
        if (length > max_length)
        {
            max_length = length;
        }
    }
    return max_length;
}

void calculate_field_widths(t_file *files, int count, int *link_width, int *owner_width, int *group_width, int *size_width) {
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

void display_files(t_file *files, int count, int flags, size_t max_name_length)
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
                struct passwd *user = getpwuid(files[i].info.st_uid);
                const char *owner_name = user ? user->pw_name : "";
                size_t owner_len = strlen(owner_name);
                memcpy(buffer + buffer_index, owner_name, owner_len);
                buffer_index += owner_len;

                for (unsigned long j = 0; j < owner_width - owner_len + 1; j++)
                {
                    buffer[buffer_index++] = ' ';
                }
            }

            struct group *group = getgrgid(files[i].info.st_gid);
            const char *group_name = group ? group->gr_name : "UNKNOWN";
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

            memcpy(buffer + buffer_index, files[i].name, files[i].name_len);
            buffer_index += files[i].name_len;

            buffer[buffer_index++] = '\n';

            write(STDOUT_FILENO, buffer, buffer_index);
        }
    }
    else
    {
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
        {
            ws.ws_col = 80;
        }

        size_t column_width = max_name_length + 2;
        int columns = ws.ws_col / column_width;
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

                memcpy(buffer + buffer_index, files[index].name, files[index].name_len);
                buffer_index += files[index].name_len;

                int padding = column_width - files[index].name_len;
                for (int j = 0; j < padding; j++) {
                    buffer[buffer_index++] = ' ';
                }
            }

            buffer[buffer_index++] = '\n';

            if ((unsigned long)buffer_index >= BUFFER_SIZE - column_width)
            {
                write(STDOUT_FILENO, buffer, buffer_index);
                buffer_index = 0;
            }
        }

        if (buffer_index > 0)
        {
            write(STDOUT_FILENO, buffer, buffer_index);
        }
    }
}

void list_directory(const char *path, int options)
{

    // printf("options: %d|| flag_r set: %d\n", options, options & FLAG_R);
    if (access(path, F_OK) == -1)
    {
        write(2, "ft_ls: Cannot access '", 21);
        write(2, path, strlen(path));
        write(2, "': ", 3);
        perror("");
        return;
    }

    DIR *dir = opendir(path);
    struct dirent *entry;
    struct stat file_stat;

    if (dir == NULL)
    {
        write(2, "ft_ls: Cannot open directory '", 29);
        write(2, path, strlen(path));
        write(2, "': ", 3);
        perror("");
        return;
    }

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
        {
            continue;
        }

        name_len = strlen(entry->d_name);
        

        memcpy(full_path + path_len, entry->d_name, strlen(entry->d_name) + 1);
        full_path[path_len + name_len] = '\0';

        if (lstat(full_path, &file_stat) == -1)
        {
            write(2, "ft_ls: Cannot stat file '", 25);
            write(2, full_path, strlen(full_path));
            write(2, "': ", 3);
            perror("");
            continue;
        }

        memcpy(g_files[index].name, entry->d_name, strlen(entry->d_name) + 1);
        g_files[index].name_len = name_len;
        if (!(options & FLAG_l) && name_len > max_len)
        {
            max_len = name_len;
        }
        g_files[index].info = file_stat;
        index++;
        if (index >= malloc_size)
        {
            malloc_size += 10000;
            g_files = realloc(g_files, malloc_size * sizeof(t_file));
        }

    }

    closedir(dir);

    merge_sort(g_files, 0, index - 1, options);    

    display_files(g_files, index, options, max_len);

    if (options & FLAG_R)
    {
        printf("flag R\n");
        path_len = strlen(path);
        int dir_capacity = 10000;
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
                memcpy(dir_entries[dirs_index].path + path_len + 1, g_files[i].name, g_files[i].name_len + 1);
                dir_entries[dirs_index].path_len = path_len + g_files[i].name_len + 1;

                dirs_index++;

                if (dirs_index >= dir_capacity)
                {
                    dir_capacity += 10000;
                    dir_entries = realloc(dir_entries, sizeof(dirs_todo) * dir_capacity);
                }
            }
        }

        char buffer[BUFFER_SIZE];
        for (int i = 0; i < dirs_index; i++)
        {
            int len = 0;
            buffer[len++] = '\n';
            memcpy(buffer + len, dir_entries[i].path, dir_entries[i].path_len);
            len += dir_entries[i].path_len;
            buffer[len++] = ':';
            buffer[len++] = '\n';
            write(STDOUT_FILENO, buffer, len);

            list_directory(dir_entries[i].path, options);
        }
        free(dir_entries);
    }
}


int main(int argc, char **argv)
{
    int options = 0;
    if (argc == 1)
    {
        list_directory(".", options);
        return 0;
    }

    options = parse_args(argc, argv);

    if (options == -1)
    {
        return 1;
    }

    bool called_once = false;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-')
        {
            called_once = true;
            list_directory(argv[i], options);
        }
    }

    if (!called_once)
    {
        list_directory(".", options);
    }

    free(g_files);
    return 0;
}
