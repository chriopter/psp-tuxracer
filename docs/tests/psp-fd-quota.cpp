// SPDX-License-Identifier: GPL-2.0-or-later
// Test-only fault injection; never linked into the release EBOOT.
#include <pspiofilemgr.h>
#include <cstdio>
#include <cstring>
extern "C" SceUID __real_sceIoOpen(const char *, int, SceMode);
extern "C" int __real_sceIoClose(SceUID);
static SceUID held[128];
static unsigned count=0,peak=0;
extern "C" SceUID __wrap_sceIoOpen(const char *path, int flags, SceMode mode) {
  bool limited = path && (strstr(path,"ms0:") || strstr(path,"ef0:"));
  if (limited && count >= 8) {
    fprintf(stderr,"INJECTED OPEN QUOTA: held=%u path=%s\n",count,path);
    return (int)0x80010018;
  }
  SceUID fd=__real_sceIoOpen(path,flags,mode);
  if(limited && fd>=0) {
    held[count++]=fd;
    if(count>peak) {peak=count;fprintf(stderr,"TEST OPEN PEAK: %u\n",peak);}
  }
  return fd;
}
extern "C" int __wrap_sceIoClose(SceUID fd) {
  int result=__real_sceIoClose(fd);
  if(result>=0) for(unsigned i=0;i<count;++i) if(held[i]==fd){held[i]=held[--count];break;}
  return result;
}
