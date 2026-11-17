#ifndef PIP_WIN_H
#define PIP_WIN_H
#include <stdint.h>
#include <stddef.h>
typedef void *Handle;
typedef uint16_t Wide;
#define WIN __declspec(dllimport)
WIN Handle VirtualAlloc(void *,size_t,uint32_t,uint32_t);
WIN int VirtualFree(void *,size_t,uint32_t);
WIN int VirtualProtect(void *,size_t,uint32_t,uint32_t *);
WIN int FlushInstructionCache(Handle,const void *,size_t);
WIN Handle GetCurrentProcess(void);
WIN uint32_t GetCurrentProcessId(void);
WIN uint32_t GetCurrentThreadId(void);
WIN Handle GetModuleHandleW(const Wide *);
WIN Handle CreateFileW(const Wide *,uint32_t,uint32_t,void *,uint32_t,uint32_t,Handle);
WIN int WriteFile(Handle,const void *,uint32_t,uint32_t *,void *);
WIN int CloseHandle(Handle);
WIN Handle CreateToolhelp32Snapshot(uint32_t,uint32_t);
typedef struct ThreadEntry {uint32_t size,usage,tid,pid;int32_t priority,delta;uint32_t flags;} ThreadEntry;
WIN int Thread32First(Handle,ThreadEntry *);
WIN int Thread32Next(Handle,ThreadEntry *);
WIN Handle OpenThread(uint32_t,int,uint32_t);
WIN uint32_t SuspendThread(Handle);
WIN uint32_t ResumeThread(Handle);
WIN int GetThreadContext(Handle,void *);
WIN int SetThreadContext(Handle,const void *);
WIN uint32_t GetLastError(void);
static inline void *alloc(size_t n){return VirtualAlloc(0,n,0x3000,4);}
static inline void release(void *p){if(p)VirtualFree(p,0,0x8000);}
static inline void bytes_copy(void *out,const void *in,size_t n){uint8_t *d=out;const uint8_t *s=in;for(size_t i=0;i<n;i++)d[i]=s[i];}
static inline void bytes_zero(void *p,size_t n){uint8_t *d=p;for(size_t i=0;i<n;i++)d[i]=0;}
#endif
