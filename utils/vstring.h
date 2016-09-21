#ifndef __VSTRING_H__
#define __VSTRING_H__

char* vstrcpy_s(char* dest, int capc, const char* src);
char* vstrncpy_s(char* dest, int capc, const char* src, int sz);

char* vstrdup_s(const char* src);

#endif

