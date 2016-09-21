#include <string.h>
#include "vassert.h"
#include "vstring.h"

char* vstrcpy_s(char* dest, int capc, const char* src)
{
    vassert(dest);
    vassert(capc > 0);
    vassert(src);

#if defined(__WIN32__)
    return strcpy_s(dest, capc, src) ? NULL: dest;
#else
    (void)capc;
    return strcpy(dest, src);
#endif
}

char* vstrncpy_s(char* dest, int capc, const char* src, int sz)
{
    vassert(dest);
    vassert(capc > 0);
    vassert(src);
    vassert(sz > 0);

#if defined(__WIN32__)
    return strncpy_s(dest, capc, src, sz) ? NULL: dest;
#else
    (void)capc;
    return strncpy(dest, src, sz);
#endif
}

char* vstrdup_s(const char* src)
{
    vassert(src);

#if defined(__WIN32__)
    return _strdup(src);
#else
    return strdup(src);
#endif
}

