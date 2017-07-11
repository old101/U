#pragma once

class TCriticalSection
{
    CRITICAL_SECTION CritSect;
public:
    TCriticalSection() { InitializeCriticalSection(&CritSect); }
    ~TCriticalSection() { DeleteCriticalSection(&CritSect); }
    void Lock() { EnterCriticalSection(&CritSect); }
    void Unlock() { LeaveCriticalSection(&CritSect); }
};

class TEvent
{
    HANDLE Event;
public:
    TEvent(BOOL InitState) { Event = CreateEvent(0, TRUE, InitState, 0); }
    ~TEvent() { CloseHandle(Event); }
    void Set() { SetEvent(Event); }
    void Reset() { ResetEvent(Event); }
    bool Wait(DWORD TimeOut = INFINITE) { return WaitForSingleObject(Event, TimeOut) == WAIT_OBJECT_0; }
};


#define VK_ALT VK_MENU

#define WORD4(a,b,c,d) (((unsigned)(a)) | (((unsigned)(b)) << 8) | (((unsigned)(c)) << 16) | (((unsigned)(d)) << 24))
#define WORD2(a,b) ((a) | ((b)<<8))
#define align_by(a,b) (((ULONG_PTR)(a) + ((b)-1)) & ~((b)-1))
#define hexdigit(a) ((a) < 'A' ? (a)-'0' : toupper(a)-'A'+10)

extern const char nop;
extern const char * const nil;

void eat();
void trim(char *dst);
void errmsg(const char *err, const char *str = 0);
void err_printf(const char *format, ...);
void err_win32(DWORD errcode = 0xFFFFFFFF);
void color(int ink = CONSCLR_DEFAULT);
int ishex(char c);
unsigned char hex(char p);
unsigned char hex(const char *p);

void __declspec(noreturn) errexit(const char *err, const char *str = 0);
unsigned process_msgs();
bool dispatch(action *table);
bool dispatch_more(action *table);

void fillCpuString(char *dst);
unsigned cpuid(unsigned _eax, int ext);
unsigned __int64 GetCPUFrequency();

static forceinline u64 rdtsc()
{
    return __rdtsc();
}

bool wcmatch(char *string, char *wc);
