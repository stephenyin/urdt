#ifndef __VLOG_H__
#define __VLOG_H__

int vlogD (const char*, ...);
int vlogI (const char*, ...);
int vlogE (const char*, ...);
int vlogDv(int, const char*, ...);
int vlogIv(int, const char*, ...);
int vlogEv(int, const char*, ...);

int vlogDp(const char*, ...);
int vlogIp(const char*, ...);

#endif

