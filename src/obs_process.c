/* Portable OBS process control and sampling. MIT licensed. */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "obs_mcp/obs_process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <sys/stat.h>

int obsmcp_launch(obsmcp_instance_t *inst, char *err, size_t err_cap)
{
    char cmd[2048];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    snprintf(cmd, sizeof(cmd),
             "\"%s\" --portable --multi --profile %s --collection %s --scene Scene",
             inst->obs_exe, inst->profile, inst->collection);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                        NULL, inst->working_dir, &si, &pi)) {
        snprintf(err, err_cap, "CreateProcess failed: %lu", GetLastError());
        return 1;
    }
    inst->pid = (unsigned long long)pi.dwProcessId;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}

int obsmcp_is_running(const obsmcp_instance_t *inst)
{
    DWORD code = 0;
    HANDLE h;
    if (!inst->pid) return 0;
    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)inst->pid);
    if (!h) return 0;
    if (!GetExitCodeProcess(h, &code)) code = 1;
    CloseHandle(h);
    return code == STILL_ACTIVE;
}

int obsmcp_terminate(obsmcp_instance_t *inst, char *err, size_t err_cap)
{
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)inst->pid);
    if (!h) {
        snprintf(err, err_cap, "process %llu not found", inst->pid);
        return 1;
    }
    if (!TerminateProcess(h, 0)) {
        snprintf(err, err_cap, "TerminateProcess failed: %lu", GetLastError());
        CloseHandle(h);
        return 1;
    }
    CloseHandle(h);
    inst->pid = 0;
    return 0;
}

static int cpu_sample(unsigned long long pid, double *cpu_seconds, double *ws_mb)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, (DWORD)pid);
    FILETIME create, exit, kernel, user;
    PROCESS_MEMORY_COUNTERS pmc;
    if (!h) return 1;
    if (!GetProcessTimes(h, &create, &exit, &kernel, &user)) {
        CloseHandle(h);
        return 1;
    }
    *cpu_seconds = ((double)kernel.dwHighDateTime * 4294967296.0 + kernel.dwLowDateTime +
                    (double)user.dwHighDateTime * 4294967296.0 + user.dwLowDateTime) / 10000000.0;
    if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc)))
        *ws_mb = (double)pmc.WorkingSetSize / 1048576.0;
    else
        *ws_mb = 0.0;
    CloseHandle(h);
    return 0;
}

int obsmcp_cpu_stats(const obsmcp_instance_t *inst, unsigned window_ms,
                     double *cpu_seconds, double *cpu_percent, double *ws_mb,
                     char *err, size_t err_cap)
{
    double a = 0, b = 0, w = 0;
    if (cpu_sample(inst->pid, &a, &w)) {
        snprintf(err, err_cap, "cannot read process times");
        return 1;
    }
    Sleep(window_ms);
    if (cpu_sample(inst->pid, &b, &w)) {
        snprintf(err, err_cap, "process exited during sampling");
        return 1;
    }
    *cpu_seconds = b - a;
    *cpu_percent = 100.0 * (b - a) / ((double)window_ms / 1000.0) /
                   (double)GetMaximumProcessorCount(ALL_PROCESSOR_GROUPS);
    *ws_mb = w;
    return 0;
}

int obsmcp_log_tail(const obsmcp_instance_t *inst, unsigned lines,
                    char **out_text, char *err, size_t err_cap)
{
    char pattern[OBSMCP_MAX_PATH];
    WIN32_FIND_DATAA fd;
    HANDLE find;
    char newest[OBSMCP_MAX_PATH] = "";
    FILETIME newest_time = {0, 0};
    FILE *file;
    long size;
    char *buf;
    unsigned found = 0;
    size_t i;

    snprintf(pattern, sizeof(pattern), "%s/*.txt", inst->log_dir);
    find = FindFirstFileA(pattern, &fd);
    if (find == INVALID_HANDLE_VALUE) {
        snprintf(err, err_cap, "no OBS logs in %s", inst->log_dir);
        return 1;
    }
    do {
        if (CompareFileTime(&fd.ftLastWriteTime, &newest_time) > 0) {
            newest_time = fd.ftLastWriteTime;
            snprintf(newest, sizeof(newest), "%s/%s", inst->log_dir, fd.cFileName);
        }
    } while (FindNextFileA(find, &fd));
    FindClose(find);
    file = fopen(newest, "rb");
    if (!file) {
        snprintf(err, err_cap, "cannot open %s", newest);
        return 1;
    }
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    rewind(file);
    buf = (char *)malloc((size_t)size + 1);
    if (!buf) { fclose(file); return 1; }
    if (fread(buf, 1, (size_t)size, file) != (size_t)size) {
        free(buf); fclose(file); return 1;
    }
    buf[size] = '\0';
    fclose(file);
    i = strlen(buf);
    while (i > 0 && found < lines) {
        --i;
        if (buf[i] == '\n') ++found;
    }
    *out_text = (char *)malloc(strlen(buf + i) + 1);
    if (*out_text) strcpy(*out_text, buf + i);
    free(buf);
    return *out_text ? 0 : 1;
}

#else
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>

int obsmcp_launch(obsmcp_instance_t *inst, char *err, size_t err_cap)
{
    pid_t pid = fork();
    if (pid < 0) {
        snprintf(err, err_cap, "fork failed");
        return 1;
    }
    if (pid == 0) {
        const char *args[10];
        char profile[128], collection[128];
        snprintf(profile, sizeof(profile), "--profile=%s", inst->profile);
        snprintf(collection, sizeof(collection), "--collection=%s", inst->collection);
        args[0] = inst->obs_exe;
        args[1] = "--portable";
        args[2] = "--multi";
        args[3] = profile;
        args[4] = collection;
        args[5] = "--scene";
        args[6] = "Scene";
        args[7] = NULL;
        if (chdir(inst->working_dir) != 0) _exit(1);
        execvp(inst->obs_exe, (char **)args);
        _exit(1);
    }
    inst->pid = (unsigned long long)pid;
    return 0;
}

int obsmcp_is_running(const obsmcp_instance_t *inst)
{
    return inst->pid && kill((pid_t)inst->pid, 0) == 0;
}

int obsmcp_terminate(obsmcp_instance_t *inst, char *err, size_t err_cap)
{
    if (kill((pid_t)inst->pid, SIGTERM) != 0) {
        snprintf(err, err_cap, "kill failed");
        return 1;
    }
    inst->pid = 0;
    return 0;
}

int obsmcp_cpu_stats(const obsmcp_instance_t *inst, unsigned window_ms,
                     double *cpu_seconds, double *cpu_percent, double *ws_mb,
                     char *err, size_t err_cap)
{
#ifdef __linux__
    char path[128];
    char line[4096];
    unsigned long long utime = 0, stime = 0;
    double a = 0, b = 0;
    long ticks = sysconf(_SC_CLK_TCK);
    if (ticks <= 0) ticks = 100;

#define READ_CPU_TOTAL(OUT)                                                            \
    do {                                                                               \
        snprintf(path, sizeof(path), "/proc/%llu/stat", inst->pid);                   \
        FILE *f = fopen(path, "r");                                                    \
        if (!f) { snprintf(err, err_cap, "cannot read %s", path); return 1; }          \
        if (!fgets(line, sizeof(line), f)) { fclose(f); return 1; }                    \
        fclose(f);                                                                     \
        char *close = strrchr(line, ')');                                              \
        if (!close) return 1;                                                          \
        char *p = close + 2;                                                           \
        int field = 3;                                                                 \
        while (*p && field <= 15) {                                                    \
            while (*p == ' ') ++p;                                                     \
            char *start = p;                                                           \
            while (*p && *p != ' ') ++p;                                               \
            if (field == 14) utime = strtoull(start, NULL, 10);                        \
            if (field == 15) stime = strtoull(start, NULL, 10);                        \
            ++field;                                                                   \
        }                                                                              \
        (OUT) = (double)(utime + stime) / (double)ticks;                               \
    } while (0)

    READ_CPU_TOTAL(a);
    {
        struct timespec ts;
        ts.tv_sec = window_ms / 1000u;
        ts.tv_nsec = (long)(window_ms % 1000u) * 1000000L;
        nanosleep(&ts, NULL);
    }
    READ_CPU_TOTAL(b);
    *cpu_seconds = b - a;
    *cpu_percent = 100.0 * (b - a) / ((double)window_ms / 1000.0) /
                   (double)sysconf(_SC_NPROCESSORS_ONLN);

    snprintf(path, sizeof(path), "/proc/%llu/status", inst->pid);
    {
        FILE *f = fopen(path, "r");
        *ws_mb = 0.0;
        if (f) {
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "VmRSS:", 6) == 0) {
                    *ws_mb = (double)strtod(line + 6, NULL) / 1024.0;
                    break;
                }
            }
            fclose(f);
        }
    }
    return 0;
#else
    snprintf(err, err_cap, "CPU sampling is not supported on this platform");
    return 1;
#endif
}

int obsmcp_log_tail(const obsmcp_instance_t *inst, unsigned lines,
                    char **out_text, char *err, size_t err_cap)
{
    DIR *dir = opendir(inst->log_dir);
    struct dirent *entry;
    char newest[OBSMCP_MAX_PATH] = "";
    time_t newest_time = 0;
    struct stat st;
    FILE *file;
    long size;
    char *buf;
    unsigned found = 0;
    size_t i;

    if (!dir) {
        snprintf(err, err_cap, "no OBS logs in %s", inst->log_dir);
        return 1;
    }
    while ((entry = readdir(dir))) {
        char full[OBSMCP_MAX_PATH];
        size_t n = strlen(entry->d_name);
        if (n < 4 || strcmp(entry->d_name + n - 4, ".txt") != 0) continue;
        snprintf(full, sizeof(full), "%s/%s", inst->log_dir, entry->d_name);
        if (stat(full, &st) == 0 && st.st_mtime > newest_time) {
            newest_time = st.st_mtime;
            snprintf(newest, sizeof(newest), "%s", full);
        }
    }
    closedir(dir);
    if (!newest[0]) {
        snprintf(err, err_cap, "no OBS logs in %s", inst->log_dir);
        return 1;
    }
    file = fopen(newest, "rb");
    if (!file) { snprintf(err, err_cap, "cannot open log"); return 1; }
    fseek(file, 0, SEEK_END); size = ftell(file); rewind(file);
    buf = (char *)malloc((size_t)size + 1);
    if (!buf) { fclose(file); return 1; }
    if (fread(buf, 1, (size_t)size, file) != (size_t)size) { free(buf); fclose(file); return 1; }
    buf[size] = '\0'; fclose(file);
    i = strlen(buf);
    while (i > 0 && found < lines) { --i; if (buf[i] == '\n') ++found; }
    *out_text = (char *)malloc(strlen(buf + i) + 1);
    if (*out_text) strcpy(*out_text, buf + i);
    free(buf);
    return *out_text ? 0 : 1;
}
#endif
