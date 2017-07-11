#include "std.h"

#include "emul.h"
#include "vars.h"
#include "util.h"

#ifdef EMUL_DEBUG
#include "dxerr9.h"
void printrdd(const char *pr, HRESULT r)
{
   color(CONSCLR_ERROR);
   printf("%s: %s\n", pr, DXGetErrorString(r));

#ifdef _DEBUG
   OutputDebugString(pr);
   OutputDebugString(": ");
   OutputDebugString(DXGetErrorString(r));
   OutputDebugString("\n");
#endif
}

void printrdi(const char *pr, HRESULT r)
{
   color(CONSCLR_ERROR);
   printf("%s: %s\n", pr, DXGetErrorString(r));

#ifdef _DEBUG
   OutputDebugString(pr);
   OutputDebugString(": ");
   OutputDebugString(DXGetErrorString(r));
   OutputDebugString("\n");
#endif
}

void printrmm(const char *pr, HRESULT r)
{
   char buf[200]; sprintf(buf, "unknown error (%08lX)", r);
   const char *str = buf;
   switch (r)
   {
      case MMSYSERR_NOERROR: str = "ok"; break;
      case MMSYSERR_INVALHANDLE: str = "MMSYSERR_INVALHANDLE"; break;
      case MMSYSERR_NODRIVER: str = "MMSYSERR_NODRIVER"; break;
      case WAVERR_UNPREPARED: str = "WAVERR_UNPREPARED"; break;
      case MMSYSERR_NOMEM: str = "MMSYSERR_NOMEM"; break;
      case MMSYSERR_ALLOCATED: str = "MMSYSERR_ALLOCATED"; break;
      case WAVERR_BADFORMAT: str = "WAVERR_BADFORMAT"; break;
      case WAVERR_SYNC: str = "WAVERR_SYNC"; break;
      case MMSYSERR_INVALFLAG: str = "MMSYSERR_INVALFLAG"; break;
   }
   color(CONSCLR_ERROR);
   printf("%s: %s\n", pr, str);
}

void printrds(const char *pr, HRESULT r)
{
   color(CONSCLR_ERROR);
   printf("%s: 0x%lX, %s\n", pr, r, DXGetErrorString(r));
}
#else
#define printrdd(x,y)
#define printrdi(x,y)
#define printrds(x,y)
#define printrmm(x,y)
#endif
