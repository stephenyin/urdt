#ifndef __VASSERT_H__
#define __VASSERT_H__

#include <stdio.h>

#if !defined(_DEBUG)
#define _DEBUG
#endif

#ifdef _DEBUG
#define vassert(cond) do { \
            if (!(cond)) { \
                printf("{assert}[%s:%d]\n", __FUNCTION__,__LINE__); \
                *((int*)0) = 0; \
            } \
        } while(0)

#define vwhere() do { \
            printf("$${where}[%s:%d]\n", __FUNCTION__,__LINE__); \
        } while(0)

#define vdump(s) do { \
            printf("##"); \
            (s); \
            printf("\n"); \
        }while(0)

#else
#define vassert()
#define vwhere()
#define vdump(s)
#endif


#define retE(cond, err) do { if ((cond)) return err; } while(0)

#endif

