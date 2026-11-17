/* Explicit initializer: no DllMain, startup hook, persistent install or code patch.
 * The x64 launcher verifies the complete executable SHA-256 before loading us. */
#include <stdint.h>
typedef uint16_t W;
typedef void *H;
#define API __declspec(dllimport)
API H GetModuleHandleW(const W *);
API int GetModuleHandleExW(uint32_t,const W *,H *);
API uint32_t GetModuleFileNameW(H,W *,uint32_t);
API H CreateFileW(const W *,uint32_t,uint32_t,void *,uint32_t,uint32_t,H);
API int WriteFile(H,const void *,uint32_t,uint32_t *,void *);
API int CloseHandle(H);
API H LoadLibraryExW(const W *,H,uint32_t);
API void *GetProcAddress(H,const char *);

typedef struct BootArgs { uint32_t size,mode; const W *root; } BootArgs;
static uint32_t length(const W *s,uint32_t max){uint32_t n=0;if(!s)return max;while(n<max&&s[n])n++;return n;}
static int append(W *out,uint32_t *n,const W *s){uint32_t i=0;while(s[i]){if(*n>=510)return 0;out[(*n)++]=s[i++];}out[*n]=0;return 1;}
static void log_line(H log,const char *s){uint32_t n=0,written;while(s[n])n++;WriteFile(log,s,n,&written,0);WriteFile(log,"\r\n",2,&written,0);}
static int named_struct(const uint8_t *base,uint32_t rva,uint64_t size,int32_t fields){
    const uint8_t *t=base+rva;
    if(*(const uint64_t *)(t)!=size || *(const uint64_t *)(t+8)!=8 || *(const uint64_t *)(t+104)!=1)return 0;
    const uint8_t *under=(const uint8_t *)(uintptr_t)*(const uint64_t *)(t+48);
    uintptr_t off=(uintptr_t)under-(uintptr_t)base;
    if(off<0x79f000 || off+112>0x7f6478)return 0;
    return *(const uint64_t *)(under+104)==19 && *(const int32_t *)(under+72)==fields;
}
__declspec(dllexport) uint32_t pip_loader_start(void *arg){
    const BootArgs *a=(const BootArgs *)arg;
    if(!a||a->size!=sizeof(BootArgs)||!a->root||(a->mode!=1&&a->mode!=2)||length(a->root,400)>=400)return 1;
    const uint8_t *base=(const uint8_t *)GetModuleHandleW(0);
    if(a->mode==2){
        if(!base||*(const uint16_t *)base!=0x5a4d)return 2;
        uint32_t pe=*(const uint32_t *)(base+0x3c);
        if(pe>0x100000||*(const uint32_t *)(base+pe)!=0x4550)return 2;
        if(*(const uint16_t *)(base+pe+4)!=0x8664 || *(const uint32_t *)(base+pe+8)!=0x6afa6a16 ||
           *(const uint32_t *)(base+pe+24+56)!=8785920)return 2;
    }
    W path[512];uint32_t n=0;
    if(!append(path,&n,a->root)||!append(path,&n,a->mode==1?(const W *)L"\\build\\PIP.Loader.selftest.log":(const W *)L"\\build\\PIP.Loader.log"))return 1;
    H log=CreateFileW(path,4,3,0,4,0x80,0);
    if(log==(H)(intptr_t)-1)return 3;
    log_line(log,a->mode==1?"PIP native bootstrap: self-test":"PIP native bootstrap: attached to verified game build 26692741");
    if(a->mode==2){
        if(!named_struct(base,0x7d2830,32,4)||!named_struct(base,0x7c96f0,112,6)||!named_struct(base,0x7c7690,560,12)){
            log_line(log,"ERROR: Odin runtime type layout mismatch; no plugin loaded.");CloseHandle(log);return 4;
        }
        log_line(log,"Read-only Odin RTTI checks passed: Gate, Safe_Cells, Home_Field.");
    }
    H self=0;
    if(!GetModuleHandleExW(6,(const W *)(uintptr_t)&pip_loader_start,&self)){
        log_line(log,"ERROR: cannot locate bootstrap module.");CloseHandle(log);return 5;
    }
    n=GetModuleFileNameW(self,path,512);
    if(!n||n>=511){CloseHandle(log);return 5;}
    while(n&&path[n-1]!='\\'&&path[n-1]!='/')n--;
    if(!n||!append(path,&n,(const W *)L"PIP.Core.dll")){CloseHandle(log);return 5;}
    H core=LoadLibraryExW(path,0,0x100|0x800);
    if(!core){log_line(log,"ERROR: PIP.Core.dll could not be loaded.");CloseHandle(log);return 5;}
    uint32_t (*version)(void)=(uint32_t (*)(void))GetProcAddress(core,"pip_abi_version");
    if(!version||version()!=1){log_line(log,"ERROR: PIP.Core ABI mismatch.");CloseHandle(log);return 6;}
    log_line(log,"PIP.Core.dll loaded in host process; C ABI version 1 verified.");
    log_line(log,"DIAGNOSTIC ONLY: no game functions hooked, no simulation state changed, evacuation is not active.");
    CloseHandle(log);return 0;
}
