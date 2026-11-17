#include "win.h"
/* These two instruction ranges contain no relative operands. Match the entire
 * range before patching. The launcher separately pins the full executable. */
static const uint8_t dispatch_bytes[19]={0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x55,0x53,0x48,0x81,0xec,0x78,0x08,0,0};
static const uint8_t move_bytes[18]={0x45,0x84,0xf6,0x0f,0x95,0xc0,0x8b,0x8c,0x24,0xdc,0,0,0,0x80,0xf1,1,0x08,0xc1};
typedef struct Patch {uint8_t *at,*trampoline;const uint8_t *expected;uint32_t size,protect;void *hook;} Patch;
typedef struct Context {__declspec(align(16)) uint8_t bytes[1232];} Context;
static Handle threads[512];
static Context contexts[512];
static int same(const uint8_t *a,const uint8_t *b,uint32_t n){for(uint32_t i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
static void jump(uint8_t *p,void *target){p[0]=0xff;p[1]=0x25;p[2]=p[3]=p[4]=p[5]=0;bytes_copy(p+6,&target,8);}
int pip_install_hooks(uint8_t *image,void *dispatch,void *move,void **original,void **continuation){
    Patch p[2]={{image+0x1d5370,0,dispatch_bytes,19,0,dispatch},{image+0x228fff,0,move_bytes,18,0,move}};
    for(uint32_t i=0;i<2;i++)if(!same(p[i].at,p[i].expected,p[i].size))return 0;
    uint8_t *tramp=VirtualAlloc(0,4096,0x3000,4);if(!tramp)return 0;
    for(uint32_t i=0;i<2;i++){p[i].trampoline=tramp+i*128;bytes_copy(p[i].trampoline,p[i].expected,p[i].size);jump(p[i].trampoline+p[i].size,p[i].at+p[i].size);}
    uint32_t old;if(!VirtualProtect(tramp,4096,0x20,&old)){release(tramp);return 0;}
    FlushInstructionCache(GetCurrentProcess(),tramp,4096);
    *original=p[0].trampoline;*continuation=p[1].at+p[1].size;
    Handle snap=CreateToolhelp32Snapshot(4,0);if(snap==(Handle)(intptr_t)-1)return 0;
    ThreadEntry entry={sizeof(ThreadEntry),0,0,0,0,0,0};uint32_t count=0,suspended=0;int ok=1;
    uint32_t pid=GetCurrentProcessId(),tid=GetCurrentThreadId();
    if(!Thread32First(snap,&entry))ok=0;
    else do{
        if(entry.pid!=pid||entry.tid==tid)continue;
        if(count==512){ok=0;break;}
        Handle t=OpenThread(0x5a,0,entry.tid);
        if(!t){if(GetLastError()==87)continue;ok=0;break;}
        threads[count++]=t;
    }while(Thread32Next(snap,&entry));
    CloseHandle(snap);
    /* No allocation, file IO or loader operations while other threads sleep. */
    if(ok)for(;suspended<count;suspended++){
        if(SuspendThread(threads[suspended])==0xffffffffu){ok=0;break;}
    }
    if(ok)for(uint32_t i=0;i<count;i++){
        bytes_zero(&contexts[i],sizeof(Context));*(uint32_t *)(contexts[i].bytes+48)=0x100001;
        if(!GetThreadContext(threads[i],&contexts[i])){ok=0;break;}
        uint64_t *rip=(uint64_t *)(contexts[i].bytes+248);
        for(uint32_t j=0;j<2;j++){
            uintptr_t offset=(uintptr_t)*rip-(uintptr_t)p[j].at;
            if(offset<p[j].size){*rip=(uintptr_t)p[j].trampoline+offset;if(!SetThreadContext(threads[i],&contexts[i]))ok=0;break;}
        }
        if(!ok)break;
    }
    uint32_t writable=0;
    if(ok)for(;writable<2;writable++)if(!VirtualProtect(p[writable].at,p[writable].size,0x40,&p[writable].protect)){ok=0;break;}
    if(ok){
        for(uint32_t i=0;i<2;i++){jump(p[i].at,p[i].hook);for(uint32_t j=14;j<p[i].size;j++)p[i].at[j]=0x90;}
        FlushInstructionCache(GetCurrentProcess(),p[0].at,p[0].size);FlushInstructionCache(GetCurrentProcess(),p[1].at,p[1].size);
    }
    for(uint32_t i=0;i<writable;i++)VirtualProtect(p[i].at,p[i].size,p[i].protect,&old);
    while(suspended)ResumeThread(threads[--suspended]);
    for(uint32_t i=0;i<count;i++)CloseHandle(threads[i]);
    /* Keep trampolines even on failure: a suspended IP may have been relocated. */
    return ok;
}
