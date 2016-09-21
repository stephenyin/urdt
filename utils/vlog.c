#include <stdio.h>
#include <stdarg.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif
#include "vlog.h"

#ifdef __ANDROID__
#define MAX_BUF_SZ ((int)1024)
#endif

enum {
    VLOG_DEBUG,
    VLOG_INFO,
    VLOG_ERR,
    VLOG_BUTT
};

static int g_stdout_level = VLOG_ERR;

/*
 * the routine to log debug message.
 */
int vlogD(const char* fmt, ...)
{
    va_list args;

    if (!fmt) {
        return 0;
    }
    if (g_stdout_level < VLOG_DEBUG) {
        return 0;
    }

    if (1) {
#if defined(__WIN32__) || defined(__APPLE__)
        va_start(args, fmt);
        printf("[D]");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
#elif defined(__ANDROID__)
        char buf[MAX_BUF_SZ] = {0};
        va_start(args, fmt);
        vsnprintf(buf, MAX_BUF_SZ, fmt, args);
        __android_log_print(ANDROID_LOG_DEBUG, "SDK[D]", "%s", buf);
        va_end(args);
#else
        va_start(args, fmt);
        printf("\e[0;33m[D]");
        vprintf(fmt, args);
        printf("\e[0m\n");
        va_end(args);
#endif
    }
    return 0;
}

/*
 * the routine to log debug message on given condition.
 * @cond: condition.
 */
int vlogDv(int cond, const char* fmt, ...)
{
    va_list(args);

    if (!fmt) {
        return 0;
    }
    if ((!cond) || (g_stdout_level < VLOG_DEBUG)){
        return 0;
    }

    if (1) {
#if defined(__WIN32__) || defined(__APPLE__)
        va_start(args, fmt);
        printf("[D]");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
#elif defined(__ANDROID__)
        char buf[MAX_BUF_SZ] = {0};
        va_start(args, fmt);
        vsnprintf(buf, MAX_BUF_SZ, fmt, args);
        __android_log_print(ANDROID_LOG_DEBUG, "SDK[D]", "%s", buf);
        va_end(args);
#else
        va_start(args, fmt);
        printf("\e[0;33m[D]");
        vprintf(fmt, args);
        printf("\e[0m\n");
        va_end(args);
#endif
    }
    return 0;
}

/*
 * the routine to log inform message;
 */
int vlogI(const char* fmt, ...)
{
    va_list args;

    if (!fmt) {
        return 0;
    }
    if (g_stdout_level < VLOG_INFO) {
        return 0;
    }

    if (1) {
#if defined(__WIN32__) || defined(__APPLE__)
        va_start(args, fmt);
        printf("[D]");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
#elif defined(__ANDROID__)
        char buf[MAX_BUF_SZ] = {0};
        va_start(args, fmt);
        vsnprintf(buf, MAX_BUF_SZ, fmt, args);
        __android_log_print(ANDROID_LOG_INFO, "SDK[I]", "%s", buf);
        va_end(args);
#else
        va_start(args, fmt);
        printf("\e[0;32m[I]");
        vprintf(fmt, args);
        printf("\e[0m\n");
        va_end(args);
#endif
    }
    return 0;
}

/*
 * the routine to log inform message on given condition.
 * @cond: condition.
 */
int vlogIv(int cond, const char* fmt, ...)
{
    va_list args;

    if (!fmt) {
        return 0;
    }
    if (!cond || (g_stdout_level < VLOG_INFO)) {
        return 0;
    }

    if (1) {
#if defined(__WIN32__) || defined(__APPLE__)
        va_start(args, fmt);
        printf("[D]");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
#elif defined(__ANDROID__)
        char buf[MAX_BUF_SZ] = {0};
        va_start(args, fmt);
        vsnprintf(buf, MAX_BUF_SZ, fmt, args);
        __android_log_print(ANDROID_LOG_INFO, "SDK[I]", "%s", buf);
        va_end(args);
#else
        va_start(args, fmt);
        printf("\e[0;32m[I]");
        vprintf(fmt, args);
        printf("\e[0m\n");
        va_end(args);
#endif
    }
    return 0;
}

/*
 * the routine to log error message
 */
int vlogE(const char* fmt, ...)
{
    va_list(args);

    if (!fmt) {
        return 0;
    }
    if (1) {
#if defined(__WIN32__) || defined(__APPLE__)
        va_start(args, fmt);
        printf("[D]");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
#elif defined(__ANDROID__)
        char buf[MAX_BUF_SZ] = {0};
        va_start(args, fmt);
        vsnprintf(buf, MAX_BUF_SZ, fmt, args);
        __android_log_print(ANDROID_LOG_ERROR, "SDK[E]", "%s", buf);
        va_end(args);
#else
        va_start(args, fmt);
        printf("\e[0;31m[E]");
        vprintf(fmt, args);
        printf("\e[0m\n");
        va_end(args);
#endif
    }
    return 0;
}

/*
 * the routine to log error message on given condition.
 * @cond: condition.
 */
int vlogEv(int cond, const char* fmt, ...)
{
    va_list(args);

    if (!fmt) {
        return 0;
    }
    if (!cond) {
        return 0;
    }

    if (1) {
#if defined(__WIN32__) || defined(__APPLE__)
        va_start(args, fmt);
        printf("[D]");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
#elif defined(__ANDROID__)
        char buf[MAX_BUF_SZ] = {0};
        va_start(args, fmt);
        vsnprintf(buf, MAX_BUF_SZ, fmt, args);
        __android_log_print(ANDROID_LOG_ERROR, "SDK[E]", "%s", buf);
        va_end(args);
#else
        va_start(args, fmt);
        printf("\e[0;31m[E]");
        vprintf(fmt, args);
        printf("\e[0m\n");
        va_end(args);
#endif
    }
    return 0;
}

