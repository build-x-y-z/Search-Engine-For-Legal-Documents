#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_loader.h"

#ifdef _WIN32
/* MinGW may not declare popen/pclose in strict C99 mode. */
FILE* popen(const char* command, const char* mode);
int pclose(FILE* stream);
#endif

static DocumentSinkFn g_sink = NULL;
static void* g_sink_user = NULL;

void file_loader_set_sink(DocumentSinkFn sink, void* user) {
    g_sink = sink;
    g_sink_user = user;
}

static void rtrim_newline(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

static char* read_entire_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long n = ftell(f);
    if (n < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    char* buf = (char*)malloc((size_t)n + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

int load_documents_from_directory(const char* path) {
    if (!path || !path[0]) return 0;
    if (!g_sink) return 0;

#ifdef _WIN32
    /*
    Windows recursive crawl: `dir /b /s root_dir\*.txt`
    Uses only stdio/stdlib/string; relies on the OS shell for directory listing.
    */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "dir /b /s \"%s\\*.txt\"", path);

    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        printf("Error: unable to list files under %s\n", path);
        return 0;
    }

    int loaded = 0;
    char file_path[1024];
    while (fgets(file_path, (int)sizeof(file_path), pipe)) {
        rtrim_newline(file_path);
        if (file_path[0] == '\0') continue;

        /* Avoid indexing the old manifest-style documents file as a normal document. */
        {
            const char* leaf = strrchr(file_path, '\\');
            leaf = leaf ? (leaf + 1) : file_path;
            if (strcmp(leaf, "documents.txt") == 0) continue;
        }

        char* content = read_entire_file(file_path);
        if (!content) continue;

        g_sink(file_path, content, g_sink_user);
        loaded++;
        free(content);
    }

    pclose(pipe);
    return loaded;
#else
    (void)path;
    printf("Directory indexing is only implemented on Windows builds.\n");
    return 0;
#endif
}

