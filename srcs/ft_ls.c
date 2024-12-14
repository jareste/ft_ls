#include <stdio.h>

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

typedef enum
{
    false,
    true
} bool;

#include <string.h>

int parse_args(int argc, char** argv)
{
    int options = 0;
    int i;
    int j;

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0)
        {
            printf("Usage: ft_ls [options] [file...]\n");
            printf("Options:\n");
            printf("  -l  use a long listing format\n");
            printf("  -R  list subdirectories recursively\n");
            printf("  -a  do not ignore entries starting with .\n");
            printf("  -r  reverse order while sorting\n");
            printf("  -t  sort by modification time, newest first\n");
            printf("  -f  do not sort, enable -aU, disable -ls\n");
            printf("  -g  like -l, but do not list owner\n");
            printf("  -d  list directories themselves, not their contents\n");
            printf("  -u  with -lt: sort by, and show, access time\n");
            printf("  -c  with -lt: sort by, and show, change time\n");
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
                        fprintf(stderr, "ft_ls: illegal option -- %c\n", argv[i][j]);
                        return -1;
                }
                j++;
            }
        }
    }
    
    return options;
}



int main(int argc, char **argv)
{
    if (argc == 1)
    {
        // list_directory(".", 0);
        return 0;
    }

    int options = parse_args(argc, argv);

    if (options == -1)
    {
        return 1;
    }

    printf("Options: %d\n", options);

    bool called_once = false;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-')
        {
            called_once = true;
            // list_directory(argv[i], options);
        }
    }

    if (!called_once)
    {
        // list_directory(".", options);
    }

    // free(g_files);
    return 0;
}