#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

int main(int argc, char** argv) {
    char root[PATH_MAX], python[PATH_MAX], script[PATH_MAX], libs[PATH_MAX*4];
    ssize_t n = readlink("/proc/self/exe", root, sizeof(root)-1);
    if (n < 0 || n >= sizeof(root)-1) return 1;
    root[n] = 0;
    char* slash = strrchr(root, '/');
    if (!slash) return 1;
    *slash = 0;
    snprintf(python,sizeof(python),"%s/python/bin/python3",root);
    snprintf(script,sizeof(script),"%s/app/app.py",root);
    snprintf(libs,sizeof(libs),"%s/app/runtime/linux-libs:%s/python/lib:%s/python/lib/python3.14/site-packages/PySide6/Qt/lib",
             root,root,root);
    setenv("LD_LIBRARY_PATH",libs,1);
    setenv("QT_OPENGL","software",0);
    char** command = calloc((size_t)argc+5,sizeof(char*));
    if (!command) return 1;
    command[0]=python; command[1]="-E"; command[2]="-s"; command[3]=script;
    for (int i=1;i<argc;i++) command[i+3]=argv[i];
    execv(python,command);
    perror("MK64 Extractor could not start");
    return 1;
}
