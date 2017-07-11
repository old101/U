#include "std.h"

#include "resource.h"
#include "emul.h"
#include "vars.h"
#include "snapshot.h"
#include "tape.h"
#include "memory.h"
#include "opendlg.h"
#include "draw.h"
#include "config.h"
#include "z80.h"
#include "sshot_png.h"

#include "util.h"

int readSP();
int readSNA48();
int readSNA128();
int readZ80();
int writeSNA(FILE*);

int load_arc(char *fname);

unsigned char what_is(char *filename)
{
   FILE *ff = fopen(filename, "rb");
   if (!ff) return snNOFILE;
   snapsize = fread(snbuf, 1, sizeof snbuf, ff);
   fclose(ff);
   if (snapsize == sizeof snbuf) return snTOOLARGE;
   unsigned char type = snUNKNOWN;
   char *ptr = strrchr(filename, '.');
   unsigned ext = ptr? (*(int*)(ptr+1) | 0x20202020) : 0;
   if (snapsize < 32) return type;

   if (snapsize == 131103 || snapsize == 147487) type = snSNA_128;
   if (snapsize == 49179) type = snSNA_48;
   if (ext == WORD4('t','a','p',' ')) type = snTAP;
   if (ext == WORD4('z','8','0',' ')) type = snZ80;
   if (conf.trdos_present)
   {
      if (!snbuf[13] && snbuf[14] && (int)snapsize == snbuf[14]*256+17) type = snHOB;
      if (snapsize >= 8192 && !(snapsize & 0xFF) && ext == WORD4('t','r','d',' ')) type = snTRD;

      if (snapsize >= 8192 && ext == WORD4('i','s','d',' '))
          type = snISD;

      if (snapsize >= 8192 && ext == WORD4('p','r','o',' '))
          type = snPRO;

      if (!memcmp(snbuf, "SINCLAIR", 8))
      {
          unsigned nfiles = snbuf[8];
          unsigned nsec = 0;
          for(unsigned i = 0; i < nfiles; i++)
          {
              nsec += snbuf[9+14*i+13];
          }
          
          if(snapsize >= 9 + nfiles * 14 + nsec * 0x100)
              type = snSCL;
      }
      if (!memcmp(snbuf, "FDI", 3) && *(unsigned short*)(snbuf+4) <= MAX_CYLS && *(unsigned short*)(snbuf+6) <= 2) type = snFDI;
      if (((*(short*)snbuf|0x2020) == WORD2('t','d')) && snbuf[4] >= 10 && snbuf[4] <= 21 && snbuf[9] <= 2) type = snTD0;
      if (*(unsigned*)snbuf == WORD4('U','D','I','!') && *(unsigned*)(snbuf+4)==snapsize-4 && snbuf[9] < MAX_CYLS && snbuf[10]<2 && !snbuf[8]) type = snUDI;
   }
   if (snapsize > 10 && !memcmp(snbuf, "ZXTape!\x1A", 8))
   {
       type = snTZX;
   }
   if (snapsize > 0x22 && !memcmp(snbuf, "Compressed Square Wave\x1A", 23)) type = snCSW;
   if (*(unsigned short*)snbuf == WORD2('S','P') && *(unsigned short*)(snbuf+2)+0x26 == (int)snapsize) type = snSP;
   return type;
}

int loadsnap(char *filename)
{
   if (load_arc(filename))
       return 1;
   unsigned char type = what_is(filename);

   if (type >= snHOB)
   {

      if (trd_toload == -1)
      {
         int last = -1;
         for (int i = 0; i < 4; i++) if (trd_loaded[i]) last = i;
         trd_toload = (last == -1)? 0 : ((type == snHOB)? last : (last+1) & 3);
      }

      for (unsigned k = 0; k < 4; k++)
      {
         if (k != trd_toload && !stricmp(comp.wd.fdd[k].name, filename))
         {
            static char err[] = "This disk image is already loaded to drive X:\n"
                                "Do you still want to load it to drive Y:?";
            err[43] = k+'A';
            err[84] = trd_toload+'A';
            if (MessageBox(GetForegroundWindow(), err, "Warning", MB_YESNO | MB_ICONWARNING) == IDNO) return 1;
         }
      }

      FDD *drive = comp.wd.fdd + trd_toload;
      if (!drive->test())
          return 0;
      int ok = drive->read(type);
      if (ok)
      {
         if (*conf.appendboot)
             drive->addboot();
         strcpy(drive->name, filename);
         if (GetFileAttributes(filename) & FILE_ATTRIBUTE_READONLY)
            conf.trdos_wp[trd_toload] = 1;
         drive->snaptype = (type == snHOB || type == snSCL)? snTRD : type;
         trd_loaded[trd_toload] = 1;
//---------Alone Coder
         char *name = filename;
         for (char *x = name; *x; x++)
             if (*x == '\\') name = x+1;
         strcpy(temp.LastSnapName, name);
         char *p = strrchr(temp.LastSnapName, '.');
         if(p)
         {
             *p = 0;
         }
         char wintitle[0x200];
         strcpy(wintitle,name);
         strcat(wintitle," - UnrealSpeccy");
         SetWindowText(wnd, wintitle);
//~---------Alone Coder
      }
      return ok;
   }

   if (type == snSP) return readSP();
   if (type == snSNA_48) return readSNA48();
   if (type == snSNA_128) return readSNA128();
   if (type == snZ80) return readZ80();
   if (type == snTAP) return readTAP();
   if (type == snTZX) return readTZX();
   if (type == snCSW) return readCSW();

   return 0;
}

int readSNA128()
{
   if(conf.mem_model == MM_PENTAGON && conf.Sna128Lock)
   {
       conf.ramsize = 128;
       temp.ram_mask = (conf.ramsize-1) >> 4;
   }

   hdrSNA128 *hdr = (hdrSNA128*)snbuf;
   reset(hdr->trdos? RM_DOS : RM_SOS);
   cpu.alt.af = hdr->altaf;
   cpu.alt.bc = hdr->altbc;
   cpu.alt.de = hdr->altde;
   cpu.alt.hl = hdr->althl;
   cpu.af = hdr->af;
   cpu.bc = hdr->bc;
   cpu.de = hdr->de;
   cpu.hl = hdr->hl;
   cpu.ix = hdr->ix;
   cpu.iy = hdr->iy;
   cpu.sp = hdr->sp;
   cpu.pc = hdr->pc;
   cpu.i = hdr->i;
   cpu.r_low = hdr->r;
   cpu.r_hi = hdr->r & 0x80;
   cpu.im = hdr->im;
   cpu.iff1 = cpu.iff2 = (hdr->iff >> 2) & 1;
   comp.p7FFD = hdr->p7FFD;
   comp.pFE = hdr->pFE;
   comp.border_attr = comp.pFE & 7;
   memcpy(memory+PAGE*5, hdr->page5, PAGE);
   memcpy(memory+PAGE*2, hdr->page2, PAGE);
   memcpy(memory+PAGE*(hdr->p7FFD & 7), hdr->active_page, PAGE);
   unsigned char *newpage = snbuf+0xC01F;
   unsigned char mapped = 0x24 | (1 << (hdr->p7FFD & 7));
   for (unsigned char i = 0; i < 8; i++)
   {
      if (!(mapped & (1 << i)))
      {
         memcpy(memory + PAGE*i, newpage, PAGE);
         newpage += PAGE;
      }
   }
   set_banks();
   return 1;
}

int readSNA48()
{
   //conf.mem_model = MM_PENTAGON; conf.ramsize = 128;  // molodcov_alex
   reset(RM_SOS);
   hdrSNA128 *hdr = (hdrSNA128*)snbuf;
   cpu.alt.af = hdr->altaf; cpu.alt.bc = hdr->altbc;
   cpu.alt.de = hdr->altde; cpu.alt.hl = hdr->althl;
   cpu.af = hdr->af; cpu.bc = hdr->bc; cpu.de = hdr->de; cpu.hl = hdr->hl;
   cpu.ix = hdr->ix; cpu.iy = hdr->iy; cpu.sp = hdr->sp;
   cpu.i = hdr->i; cpu.r_low = hdr->r; cpu.r_hi = hdr->r & 0x80; cpu.im = hdr->im;
   cpu.iff1 = cpu.iff2 = (hdr->iff >> 2) & 1; comp.p7FFD = 0x30;
   comp.pEFF7 |= EFF7_LOCKMEM; //Alone Coder
   comp.pFE = hdr->pFE; comp.border_attr = comp.pFE & 7;
   memcpy(memory+PAGE*5, hdr->page5, PAGE);
   memcpy(memory+PAGE*2, hdr->page2, PAGE);
   memcpy(memory+PAGE*0, hdr->active_page, PAGE);
   cpu.pc = cpu.DirectRm(cpu.sp)+0x100*cpu.DirectRm(cpu.sp+1); cpu.sp += 2;
   set_banks(); return 1;
}

int readSP()
{
   //conf.mem_model = MM_PENTAGON; conf.ramsize = 128;  // molodcov_alex
   reset(RM_SOS);
   hdrSP *hdr = (hdrSP*)snbuf;
   cpu.alt.af = hdr->altaf; cpu.alt.bc = hdr->altbc;
   cpu.alt.de = hdr->altde; cpu.alt.hl = hdr->althl;
   cpu.af = hdr->af; cpu.bc = hdr->bc; cpu.de = hdr->de; cpu.hl = hdr->hl;
   cpu.ix = hdr->ix; cpu.iy = hdr->iy; cpu.sp = hdr->sp; cpu.pc = hdr->pc;
   cpu.i = hdr->i; cpu.r_low = hdr->r; cpu.r_hi = hdr->r & 0x80;
   cpu.iff1 = (hdr->flags & 1);
   cpu.im = 1 + ((hdr->flags >> 1) & 1);
   cpu.iff2 = (hdr->flags >> 2) & 1;
   comp.p7FFD = 0x30;
   comp.pEFF7 |= EFF7_LOCKMEM; //Alone Coder
   comp.pFE = hdr->pFE; comp.border_attr = comp.pFE & 7;
   for (unsigned i = 0; i < hdr->len; i++)
      cpu.DirectWm(hdr->start + i, snbuf[i + 0x26]);
   set_banks(); return 1;
}

int writeSNA(FILE *ff)
{
/*   if (conf.ramsize != 128) {
      MessageBox(GetForegroundWindow(), "SNA format can hold only\r\n128kb memory models", "Save", MB_ICONERROR);
      return 0;
   }*/ //Alone Coder
   hdrSNA128 *hdr = (hdrSNA128*)snbuf;
   hdr->trdos = (comp.flags & CF_TRDOS)? 1 : 0;
   hdr->altaf = cpu.alt.af; hdr->altbc = cpu.alt.bc;
   hdr->altde = cpu.alt.de; hdr->althl = cpu.alt.hl;
   hdr->af = cpu.af; hdr->bc = cpu.bc; hdr->de = cpu.de; hdr->hl = cpu.hl;
   hdr->ix = cpu.ix; hdr->iy = cpu.iy; hdr->sp = cpu.sp; hdr->pc = cpu.pc;
   hdr->i = cpu.i; hdr->r = (cpu.r_low & 0x7F)+cpu.r_hi; hdr->im = cpu.im;
   hdr->iff = cpu.iff2 ? 4 : 0;
   hdr->p7FFD = comp.p7FFD;
   hdr->pFE = comp.pFE; comp.border_attr = comp.pFE & 7;
   unsigned savesize = sizeof(hdrSNA128);
   unsigned char mapped = 0x24 | (1 << (comp.p7FFD & 7));
   if (comp.p7FFD == 0x30)
   { // save 48k
      mapped = 0xFF;
      savesize = 0xC01B;
      hdr->sp -= 2;
      cpu.DirectWm(hdr->sp, cpu.pcl);
      cpu.DirectWm(hdr->sp+1, cpu.pch);
   }
   memcpy(hdr->page5, memory+PAGE*5, PAGE);
   memcpy(hdr->page2, memory+PAGE*2, PAGE);
   memcpy(hdr->active_page, memory+PAGE*(comp.p7FFD & 7), PAGE);
   if (fwrite(hdr, 1, savesize, ff) != savesize) return 0;
   for (unsigned char i = 0; i < 8; i++)
      if (!(mapped & (1 << i))) {
         if (fwrite(memory + PAGE*i, 1, PAGE, ff) != PAGE) return 0;
      }
   return 1;
}

void unpack_page(unsigned char *dst, int dstlen, unsigned char *src, int srclen)
{
   memset(dst, 0, dstlen);
   while (srclen > 0 && dstlen > 0) {
      if (srclen >= 4 && *(unsigned short*)src == WORD2(0xED, 0xED)) {
         for (unsigned char i = src[2]; i; i--)
            *dst++ = src[3], dstlen--;
         srclen -= 4; src += 4;
      } else *dst++ = *src++, dstlen--, srclen--;
   }
}

int readZ80()
{
   //conf.mem_model = MM_PENTAGON; conf.ramsize = 128;  // molodcov_alex
   hdrZ80 *hdr = (hdrZ80*)snbuf;
   unsigned char *ptr = snbuf + 30;
   unsigned char model48k = (hdr->model < 3);
   reset((model48k|(hdr->p7FFD & 0x10)) ? RM_SOS : RM_128);
   if (hdr->flags == 0xFF)
       hdr->flags = 1;
   if (hdr->pc == 0)
   { // 2.01
      ptr += 2 + hdr->len;
      hdr->pc = hdr->newpc;
      memset(RAM_BASE_M, 0, PAGE*8); // clear 128k - first 8 pages

      while (ptr < snbuf+snapsize)
      {
         unsigned char *p48[] =
         {
                base_sos_rom, 0, 0, 0,
                RAM_BASE_M+2*PAGE, RAM_BASE_M+0*PAGE, 0, 0,
                RAM_BASE_M+5*PAGE, 0, 0, 0
         };
         unsigned char *p128[] =
         {
                base_sos_rom, base_dos_rom, base_128_rom, RAM_BASE_M+0*PAGE,
                RAM_BASE_M+1*PAGE, RAM_BASE_M+2*PAGE, RAM_BASE_M+3*PAGE, RAM_BASE_M+4*PAGE,
                RAM_BASE_M+5*PAGE, RAM_BASE_M+6*PAGE, RAM_BASE_M+7*PAGE, 0
         };
         unsigned len = *(unsigned short*)ptr;
         if (ptr[2] > 11)
             return 0;
         unsigned char *dstpage = model48k ? p48[ptr[2]] : p128[ptr[2]];
         if (!dstpage)
             return 0;
         ptr += 3;
         if (len == 0xFFFF)
             memcpy(dstpage, ptr, len = PAGE);
         else
             unpack_page(dstpage, PAGE, ptr, len);
         ptr += len;
      }
   }
   else
   {
      int len = snapsize - 30;
      unsigned char *mem48 = ptr;
      if (hdr->flags & 0x20)
         unpack_page(mem48 = snbuf + 4*PAGE, 3*PAGE, ptr, len);
      memcpy(memory + PAGE*5, mem48, PAGE);
      memcpy(memory + PAGE*2, mem48 + PAGE, PAGE);
      memcpy(memory + PAGE*0, mem48 + 2*PAGE, PAGE);
      model48k = 1;
   }
   cpu.a = hdr->a, cpu.f = hdr->f;
   cpu.bc = hdr->bc, cpu.de = hdr->de, cpu.hl = hdr->hl;
   cpu.alt.bc = hdr->bc1, cpu.alt.de = hdr->de1, cpu.alt.hl = hdr->hl1;
   cpu.alt.a = hdr->a1, cpu.alt.f = hdr->f1;
   cpu.pc = hdr->pc, cpu.sp = hdr->sp; cpu.ix = hdr->ix, cpu.iy = hdr->iy;
   cpu.i = hdr->i, cpu.r_low = hdr->r & 0x7F;
   cpu.r_hi = ((hdr->flags & 1) << 7);
   comp.pFE = (hdr->flags >> 1) & 7;
   comp.border_attr = comp.pFE;
   cpu.iff1 = hdr->iff1, cpu.iff2 = hdr->iff2; cpu.im = (hdr->im & 3);
   comp.p7FFD = (model48k) ? 0x30 : hdr->p7FFD;

   if(hdr->len == 55) // version 3.0 (with 1ffd)
       comp.p1FFD = hdr->p1FFD;

   if (model48k)
       comp.pEFF7 |= EFF7_LOCKMEM; //Alone Coder
   set_banks();

   return 1;
}

#define arctmp ((char*)rbuf)
char *arc_fname;
INT_PTR CALLBACK ArcDlg(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (msg == WM_INITDIALOG) {
      for (char *dst = arctmp; *dst; dst += strlen(dst)+1)
         SendDlgItemMessage(dlg, IDC_ARCLIST, LB_ADDSTRING, 0, (LPARAM)dst);
      SendDlgItemMessage(dlg, IDC_ARCLIST, LB_SETCURSEL, 0, 0);
      return 1;
   }
   if ((msg == WM_COMMAND && wp == IDCANCEL) ||
       (msg == WM_SYSCOMMAND && (wp & 0xFFF0) == SC_CLOSE)) EndDialog(dlg, 0);
   if (msg == WM_COMMAND && (LOWORD(wp) == IDOK || (HIWORD(wp)==LBN_DBLCLK && LOWORD(wp) == IDC_ARCLIST)))
   {
      int n = SendDlgItemMessage(dlg, IDC_ARCLIST, LB_GETCURSEL, 0, 0);
      char *dst = arctmp;
      for (int q = 0; q < n; q++) dst += strlen(dst)+1;
      arc_fname = dst;
      EndDialog(dlg, 0);
   }
   return 0;
}

bool filename_ok(char *fname)
{
   for (char *wc = skiparc; *wc; wc += strlen(wc)+1)
      if (wcmatch(fname, wc)) return 0;
   return 1;
}

int load_arc(char *fname)
{
   char *ext = strrchr(fname, '.'); if (!ext) return 0;
   ext++;
   char *cmdtmp, done = 0;
   for (char *x = arcbuffer; *x; x = cmdtmp + strlen(cmdtmp)+1) {
      cmdtmp = x + strlen(x)+1;
      if (stricmp(ext, x)) continue;

      char dir[0x200]; GetCurrentDirectory(sizeof dir, dir);
      char tmp[0x200]; GetTempPath(sizeof tmp, tmp);
      char d1[0x20]; sprintf(d1, "us%08lX", GetTickCount());
      SetCurrentDirectory(tmp);
      CreateDirectory(d1, 0);
      SetCurrentDirectory(d1);

      color();
      char cmdln[0x200]; sprintf(cmdln, cmdtmp, fname);
      STARTUPINFO si = { sizeof si };
      si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
      PROCESS_INFORMATION pi;
      unsigned flags = CREATE_NEW_CONSOLE;
      HANDLE hc = CreateFile("CONOUT$", GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
      if (hc != INVALID_HANDLE_VALUE) CloseHandle(hc), flags = 0;
      if (CreateProcess(0, cmdln, 0, 0, 0, flags, 0, 0, &si, &pi)) {
         WaitForSingleObject(pi.hProcess, INFINITE);
         DWORD code; GetExitCodeProcess(pi.hProcess, &code);
         CloseHandle(pi.hThread);
         CloseHandle(pi.hProcess);
         if (!code || MessageBox(GetForegroundWindow(), "Broken archive", 0, MB_ICONERROR | MB_OKCANCEL) == IDOK) {
            WIN32_FIND_DATA fd; HANDLE h;
            char *dst = arctmp; unsigned nfiles = 0;
            if ((h = FindFirstFile("*.*", &fd)) != INVALID_HANDLE_VALUE) {
               do {
                  if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && filename_ok(fd.cFileName)) {
                     strcpy(dst, fd.cFileName); dst += strlen(dst)+1;
                     nfiles++;
                  }
               } while (FindNextFile(h, &fd));
               FindClose(h);
            }
            *dst = 0; arc_fname = 0;
            if (nfiles == 1) arc_fname = arctmp;
            if (nfiles > 1)
                DialogBox(hIn, MAKEINTRESOURCE(IDD_ARC), GetForegroundWindow(), ArcDlg);
            if (!nfiles) MessageBox(GetForegroundWindow(), "Empty archive!", 0, MB_ICONERROR | MB_OK);
            char buf[0x200]; strcpy(buf, tmp); strcat(buf, "\\");
            strcat(buf, d1); strcat(buf, "\\"); if (arc_fname) strcat(buf, arc_fname), arc_fname = buf;
            if (arc_fname && !(done = loadsnap(arc_fname))) MessageBox(GetForegroundWindow(), "loading error", arc_fname, MB_ICONERROR);
            if (!done) done = -1;
         }
         // delete junk
         SetCurrentDirectory(tmp);
         SetCurrentDirectory(d1);
         WIN32_FIND_DATA fd; HANDLE h;
         if ((h = FindFirstFile("*.*", &fd)) != INVALID_HANDLE_VALUE) {
            do { DeleteFile(fd.cFileName); } while (FindNextFile(h, &fd));
            FindClose(h);
         }
      }
      SetCurrentDirectory(tmp);
      RemoveDirectory(d1);
      SetCurrentDirectory(dir);
      eat(); if (done) return done > 0 ? 1 : 0;
   }
   eat(); return 0;
}

void opensnap(int index)
{
   char mask[0x200]; *mask = 0;
   for (char *x = arcbuffer; *x; )
      strcat(mask, ";*."), strcat(mask, x),
      x += strlen(x)+1, x += strlen(x)+1;

   char fline[0x400];
   const char *src = "all (sna,z80,sp,tap,tzx,csw,trd,scl,fdi,td0,udi,isd,pro,hobeta)\0*.sna;*.z80;*.sp;*.tap;*.tzx;*.csw;*.trd;*.scl;*.td0;*.udi;*.fdi;*.isd;*.pro;*.$?;*.!?<\0"
               "Disk B (trd,scl,fdi,td0,udi,isd,pro,hobeta)\0*.trd;*.scl;*.fdi;*.udi;*.td0;*.isd;*.pro;*.$?<\0"
               "Disk C (trd,scl,fdi,td0,udi,isd,pro,hobeta)\0*.trd;*.scl;*.fdi;*.udi;*.td0;*.isd;*.pro;*.$?<\0"
               "Disk D (trd,scl,fdi,td0,udi,isd,pro,hobeta)\0*.trd;*.scl;*.fdi;*.udi;*.td0;*.isd;*.pro;*.$?<\0\0>";
   if (!conf.trdos_present)
      src = "ZX files (sna,z80,tap,tzx,csw)\0*.sna;*.z80;*.tap;*.tzx;*.csw<\0\0>";
   for (char *dst = fline; *src != '>'; src++)
      if (*src == '<') strcpy(dst, mask), dst += strlen(dst);
      else *dst++ = *src;

   OPENFILENAME ofn = { 0 };
   char fname[0x200]; *fname = 0;
   char dir[0x200]; GetCurrentDirectory(sizeof dir, dir);

   ofn.lStructSize = (WinVerMajor < 5) ? OPENFILENAME_SIZE_VERSION_400 : sizeof(OPENFILENAME);
   ofn.hwndOwner = GetForegroundWindow();
   ofn.lpstrFilter = fline;
   ofn.lpstrFile = fname; ofn.nMaxFile = sizeof fname;
   ofn.lpstrTitle = "Load Snapshot / Disk / Tape";
   ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
   ofn.nFilterIndex = index;
   ofn.lpstrInitialDir = dir;
//   __debugbreak();
   if (GetSnapshotFileName(&ofn, 0))
   {
      trd_toload = ofn.nFilterIndex-1;
      if (!loadsnap(fname))
          MessageBox(GetForegroundWindow(), fname, "loading error", MB_ICONERROR);
   }
   eat();
}

const int mx_typs = (1+4*6);
unsigned char snaps[mx_typs]; unsigned exts[mx_typs], drvs[mx_typs]; int snp;
static void addref(LPSTR &ptr, unsigned char sntype, const char *ref, unsigned drv, unsigned ext)
{
   strcpy(ptr, ref); ptr += strlen(ptr)+1;
   strcpy(ptr, ref+strlen(ref)+1); ptr += strlen(ptr)+1;
   drvs[snp] = drv; exts[snp] = ext; snaps[snp++] = sntype;
}

void savesnap(int diskindex)
{
again:
   OPENFILENAME ofn = { 0 };
   char fname[0x200]; *fname = 0;
   if (diskindex >= 0) {
      strcpy(fname, comp.wd.fdd[diskindex].name);
      int ln = strlen(fname);
      if (ln > 4 && (*(unsigned*)(fname+ln-4) | WORD4(0,0x20,0x20,0x20)) == WORD4('.','s','c','l'))
         *(unsigned*)(fname+ln-4) = WORD4('.','t','r','d');
   }

   snp = 1; char types[600], *ptr = types;

   if (diskindex < 0)
   {
      exts[snp] = WORD4('s','n','a',0); snaps[snp] = snSNA_128; // default
      addref(ptr, snSNA_128, "ZX-Spectrum 128K snapshot (SNA)\0*.sna", -1, WORD4('s','n','a',0));
   }

   ofn.lStructSize = (WinVerMajor < 5) ? OPENFILENAME_SIZE_VERSION_400 : sizeof(OPENFILENAME);
   ofn.nFilterIndex = 1;

   if (conf.trdos_present)
   {
      static char mask[] = "Disk A (TRD)\0*.trd";
      static const char ex[][3] = { {'T','R','D'}, {'F','D','I'},{'T','D','0'},{'U','D','I'},{'I','S','D'},{'P','R','O'}};
      static const unsigned ex2[] = { snTRD, snFDI, snTD0, snUDI, snISD, snPRO };

      for (int n = 0; n < 4; n++)
      {
         if (!comp.wd.fdd[n].rawdata)
             continue;
         if (diskindex >= 0 && diskindex != n)
             continue;
         mask[5] = 'A'+n;

         for (int i = 0; i < sizeof ex/sizeof(ex[0]); i++)
         {
            if (diskindex == n && ex2[i] == comp.wd.fdd[n].snaptype)
                ofn.nFilterIndex = snp;
            memcpy(mask+8, ex[i], 3);
            memcpy(mask+15, ex[i], 3);
            addref(ptr, ex2[i], mask, n, (*(unsigned*)ex[i] & 0xFFFFFF) | 0x202020);
         }
      }
   }
   ofn.lpstrFilter = types; *ptr = 0;
   ofn.lpstrFile = fname; ofn.nMaxFile = sizeof fname;
   ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
   ofn.hwndOwner = GetForegroundWindow();
   char *path = strrchr(fname, '\\');
   if (path)
   { // check if directory exists (for files opened from archive)
      char x = *path; *path = 0;
      unsigned atr = GetFileAttributes(fname); *path = x;
      if (atr == -1 || !(atr & FILE_ATTRIBUTE_DIRECTORY)) *fname = 0;
   } else path = fname;
   path = strrchr(path, '.'); if (path) *path = 0; // delete extension

   if (GetSnapshotFileName(&ofn, 1))
   {
      char *fn = strrchr(ofn.lpstrFile, '\\');
      fn = fn? fn+1 : ofn.lpstrFile;
      char *extpos = strrchr(fn, '.');
      if (!extpos || stricmp(extpos+1, (char*)&exts[ofn.nFilterIndex]))
      {
         char *dst = fn + strlen(fn); *dst++ = '.';
         *(unsigned*)dst = exts[ofn.nFilterIndex];
      }
      if (GetFileAttributes(ofn.lpstrFile) != -1 &&
            IDNO == MessageBox(GetForegroundWindow(), "File exists. Overwrite ?", "Save", MB_ICONQUESTION | MB_YESNO))
         goto again;

      FILE *ff = fopen(ofn.lpstrFile, "wb");
      if (ff)
      {
         int res = 0;
         FDD *saveto = comp.wd.fdd + drvs[ofn.nFilterIndex];
         switch (snaps[ofn.nFilterIndex])
         {
            case snSNA_128: res = writeSNA(ff); break;
            case snTRD: res = saveto->write_trd(ff); break;
            case snUDI: res = saveto->write_udi(ff); break;
            case snFDI: res = saveto->write_fdi(ff); break;
            case snTD0: res = saveto->write_td0(ff); break;
            case snISD: res = saveto->write_isd(ff); break;
            case snPRO: res = saveto->write_pro(ff); break;
         }
         fclose(ff);
         if (!res)
             MessageBox(GetForegroundWindow(), "write error", "Save", MB_ICONERROR);
         else if (drvs[ofn.nFilterIndex]!=-1)
         {
             comp.wd.fdd[drvs[ofn.nFilterIndex]].optype=0;
             strcpy(comp.wd.fdd[drvs[ofn.nFilterIndex]].name, ofn.lpstrFile);

             //---------Alone Coder
             char *name = ofn.lpstrFile;
             for (char *x = name; *x; x++)
             {
                 if (*x == '\\')
                     name = x+1;
             }
             char wintitle[0x200];
             strcpy(wintitle,name);
             strcat(wintitle," - UnrealSpeccy");
             SetWindowText(wnd, wintitle);
             //~---------Alone Coder
         }
      }
      else
          MessageBox(GetForegroundWindow(), "Can't open file for writing", "Save", MB_ICONERROR);
   }
   eat();
}

void ConvPal8ToBgr24(u8 *dst, u8 *scrbuf, int dx)
{
    u8 *ds = dst;
    for(unsigned i = 0; i < temp.oy; i++) // convert to BGR24 format
    {
       unsigned char *src = scrbuf + i*dx;
       for (unsigned y = 0; y < temp.ox; y++)
       {
          ds[0] = pal0[src[y]].peBlue;
          ds[1] = pal0[src[y]].peGreen;
          ds[2] = pal0[src[y]].peRed;
          ds += 3;
       }
       ds = (PBYTE)(ULONG_PTR(ds + 3) & ~ULONG_PTR(3)); // ������ ������ ��������� �� 4
    }
}

void ConvRgb15ToBgr24(u8 *dst, u8 *scrbuf, int dx)
{
    u8 *ds = dst;
    for(unsigned i = 0; i < temp.oy; i++) // convert to BGR24 format
    {
       unsigned char *src = scrbuf + i*dx;
       for (unsigned y = 0; y < temp.ox; y++)
       {
          unsigned xx;
          xx = *(unsigned*)(src + y*2);

          ds[0] = (xx & 0x1F)<<3;
          ds[1] = (xx & 0x03E0)>>2;
          ds[2] = (xx & 0x7C00)>>7;
          ds += 3;
       }
       ds = (PBYTE)(ULONG_PTR(ds + 3) & ~ULONG_PTR(3)); // ������ ������ ��������� �� 4
    }
}

void ConvRgb16ToBgr24(u8 *dst, u8 *scrbuf, int dx)
{
    u8 *ds = dst;
    for(unsigned i = 0; i < temp.oy; i++) // convert to BGR24 format
    {
       unsigned char *src = scrbuf + i*dx;
       for (unsigned y = 0; y < temp.ox; y++)
       {
          unsigned xx;
          xx = *(unsigned*)(src + y*2);

          ds[0] = (xx&0x1F)<<3;
          ds[1] = (xx&0x07E0)>>3;
          ds[2] = (xx&0xF800)>>8;
          ds += 3;
       }
       ds = (PBYTE)(ULONG_PTR(ds + 3) & ~ULONG_PTR(3)); // ������ ������ ��������� �� 4
    }
}

void ConvYuy2ToBgr24(u8 *dst, u8 *scrbuf, int dx)
{
    u8 *ds = dst;
    for(unsigned i = 0; i < temp.oy; i++) // convert to BGR24 format
    {
        unsigned char *src = scrbuf + i*dx;
        for (unsigned y = 0; y < temp.ox; y++)
        {
            unsigned xx;
            xx = *(unsigned*)(src + y*2);

            int u = src[y/2*4+1], v = src[y/2*4+3], Y = src[y*2];
            int r = (int)(.4732927654e-2*u-255.3076403+1.989858012*v+.9803921569*Y);
            int g = (int)(-.9756592292*v+186.0716700-.4780256930*u+.9803921569*Y);
            int b = (int)(.9803921569*Y+2.004732928*u-255.3076403-.1014198783e-1*v); // mapple rulez!
            if (r < 0) r = 0; if (r > 255) r = 255;
            if (g < 0) g = 0; if (g > 255) g = 255;
            if (b < 0) b = 0; if (b > 255) b = 255;

            ds[0] = b;
            ds[1] = g;
            ds[2] = r;
            ds += 3;
        }
        ds = (PBYTE)(ULONG_PTR(ds + 3) & ~ULONG_PTR(3)); // ������ ������ ��������� �� 4
    }
}

void ConvBgr32ToBgr24(u8 *dst, u8 *scrbuf, int dx)
{
    u8 *ds = dst;
    for(unsigned i = 0; i < temp.oy; i++) // convert to BGR24 format
    {
       unsigned char *src = scrbuf + i*dx;
       for (unsigned x = 0; x < temp.ox; x++)
       {
          ds[0] = src[0];
          ds[1] = src[1];
          ds[2] = src[2];
          src += 4;
          ds += 3;
       }
       ds = (PBYTE)(ULONG_PTR(ds + 3) & ~ULONG_PTR(3)); // ������ ������ ��������� �� 4
    }
}


TColorConverter ConvBgr24 = 0;

void SaveBmp(FILE *File, u8 *ds)
{
     static u8 bmpheader32[]=
     {
            // BITMAPFILEHEADER
            0x42,0x4d,           // Type
            0x36,0x10,0x0e,0x00, // Size
            0x00,0x00,           // Reserved1
            0x00,0x00,           // Reserved2
            0x36,0x00,0x00,0x00, // OffBits
            // BITMAPINFOHEADER
            0x28,0x00,0x00,0x00, // Size
            0x80,0x02,0x00,0x00, // Width
            0xe0,0x01,0x00,0x00, // Height
            0x01,0x00,           // Planes
            0x18,0x00,           // BitCount
            0x00,0x00,0x00,0x00, // Compression
            0x00,0x10,0x0e,0x00, // SizeImage
            0x00,0x00,0x00,0x00, // XPixelsPerMeter
            0x00,0x00,0x00,0x00, // YPixelsPerMeter
            0x00,0x00,0x00,0x00, // ClrUsed
            0x00,0x00,0x00,0x00  // ClrImportant
     };

    *(unsigned*)(bmpheader32 + 2) = temp.ox * temp.oy * 3 + sizeof(bmpheader32); // filesize
    *(unsigned*)(bmpheader32 + 0x12) = temp.ox;
    *(unsigned*)(bmpheader32 + 0x16) = temp.oy;
    fwrite(bmpheader32, 1, sizeof(bmpheader32), File);

    for(int y = temp.oy - 1; y >= 0 ; y--)
    {
        fwrite(ds + ((y * temp.ox * 3 + 3) & ~3), 1, temp.ox * 3, File);
    }
}

void SavePng(FILE *File, u8 *ds)
{
    static png_color bkgColor = {127, 127, 127};

    if(!temp.PngSupport)
        return;

    PngSaveImage(File, ds, temp.ox, temp.oy, bkgColor);
}

#define MAKE_RGBQUAD(r,g,b) (ULONG) (u32(b) | (u32(g)<<8) | (u32(r)<<16))

static void SaveBmp16c(FILE *File, const u8 *ds)
{
     static u8 bmpheader32[]=
     {
            // BITMAPFILEHEADER
            0x42,0x4d,           // Type
            0x36,0x10,0x0e,0x00, // Size = 320*200/2 + sizeof(bmpheader32)
            0x00,0x00,           // Reserved1
            0x00,0x00,           // Reserved2
            0x76,0x00,0x00,0x00, // OffBits
            // BITMAPINFOHEADER
            0x28,0x00,0x00,0x00, // Size
            0x40,0x01,0x00,0x00, // Width = 320
            0xc8,0x00,0x00,0x00, // Height = 200
            0x01,0x00,           // Planes
            0x04,0x00,           // BitCount = 4
            0x00,0x00,0x00,0x00, // Compression = BI_RGB
            0x00,0x10,0x0e,0x00, // SizeImage
            0x00,0x00,0x00,0x00, // XPixelsPerMeter
            0x00,0x00,0x00,0x00, // YPixelsPerMeter
            0x00,0x00,0x00,0x00, // ClrUsed
            0x00,0x00,0x00,0x00, // ClrImportant
            // PALETTE
            0,0,0,0,             //  0 bgra
            0,0,0,0,             //  1 bgra
            0,0,0,0,             //  2 bgra
            0,0,0,0,             //  3 bgra
            0,0,0,0,             //  4 bgra
            0,0,0,0,             //  5 bgra
            0,0,0,0,             //  6 bgra
            0,0,0,0,             //  7 bgra
            0,0,0,0,             //  8 bgra
            0,0,0,0,             //  9 bgra
            0,0,0,0,             // 10 bgra
            0,0,0,0,             // 11 bgra
            0,0,0,0,             // 12 bgra
            0,0,0,0,             // 13 bgra
            0,0,0,0,             // 14 bgra
            0,0,0,0              // 15 bgra
     };

    *(unsigned*)(bmpheader32 + 2) = 320 * 200 / 2 + sizeof(bmpheader32); // filesize
    for(unsigned i = 0; i < 16; i++)
    {
        // ������ ������� Gg0Rr0Bb
        u8 r = u32((comp.comp_pal[i] >> 3) & 3) * 255 / 3;
        u8 g = u32((comp.comp_pal[i] >> 6) & 3) * 255 / 3;
        u8 b = u32(comp.comp_pal[i] & 3) * 255 / 3;

        *(PULONG)(bmpheader32 + 54 + 4*i) = MAKE_RGBQUAD(r,g,b);
    }
    fwrite(bmpheader32, 1, sizeof(bmpheader32), File);
    fwrite(ds, 1, 320 * 200 / 2, File);
}

static void ConvPal16cAtm1(u8 *Buf)
{
    static const int ega0_ofs = -4*PAGE;
    static const int ega1_ofs = 0;
    static const int ega2_ofs = -4*PAGE+0x2000;
    static const int ega3_ofs = 0x2000;

   for (int y = 200 - 1; y >= 0; y--)
   {
      const u8 *src = temp.base + y*40;

      for(unsigned x = 0; x < 320; x += 8)
      {
          u8 v0 = src[ega0_ofs];
          u8 v1 = src[ega1_ofs];
          u8 v2 = src[ega2_ofs];
          u8 v3 = src[ega3_ofs];
          Buf[0] = ((((v0 >> 3) & 8) | (v0 & 0x7)) << 4) |
                   (((v0 & 0x80) | (v0 << 1) & 0x70) >> 4);
          Buf[1] = ((((v1 >> 3) & 8) | (v1 & 0x7)) << 4) |
                   (((v1 & 0x80) | (v1 << 1) & 0x70) >> 4);
          Buf[2] = ((((v2 >> 3) & 8) | (v2 & 0x7)) << 4) |
                   (((v2 & 0x80) | (v2 << 1) & 0x70) >> 4);
          Buf[3] = ((((v3 >> 3) & 8) | (v3 & 0x7)) << 4) |
                   (((v3 & 0x80) | (v3 << 1) & 0x70) >> 4);
          
          src++;
          Buf += 4;
      }
   }

}

#define ConvPal16cAtm2 ConvPal16cAtm1

typedef void (*TSaver)(FILE *File, u8 *ds);

void main_scrshot()
{
   char fname[FILENAME_MAX];
   static unsigned sshot = 0;
   static const char *Format[] = { "scr", "bmp", "png" };
   static const TSaver Saver[] = { SaveBmp, SavePng };

   const char *Ext = Format[conf.scrshot];

   if (conf.scrshot == 0 &&
       (
        ((conf.mem_model == MM_ATM710 || conf.mem_model == MM_ATM3) && ((comp.pFF77 & 7) == 0)) ||
        ((conf.mem_model == MM_ATM450) && (((comp.aFE >> 5) & 3) == 0))
       )
      )
   {
       Ext = Format[1];
   }

   snprintf(fname, _countof(fname), "%s\\%s_%06u.%s", conf.scrshot_dir,
       temp.LastSnapName[0] ? temp.LastSnapName : "sshot", sshot, Ext);
   fname[FILENAME_MAX-1] = 0;

   FILE *fileShot = fopen(fname, "wb");
   if (!fileShot)
      return;

   if (conf.scrshot == 0)
   {
      switch(conf.mem_model)
      {
      case MM_ATM710:
      case MM_ATM3:
          {
              switch(comp.pFF77 & 7)
              {
              case 0: // EGA 320x200 16c
                  {
                      u8 *Buf = (u8 *)malloc(320*200/2);
                      ConvPal16cAtm2(Buf);
                      SaveBmp16c(fileShot, Buf);
                      free(Buf);
                  }
              break;
              case 3:
                  goto standard_zx_mode;
              }
          }
      break;

      case MM_ATM450:
          switch((comp.aFE >> 5) & 3)
          {
          case 0: // EGA 320x200 16c
              {
                  u8 *Buf = (u8 *)malloc(320*200/2);
                  ConvPal16cAtm1(Buf);
                  SaveBmp16c(fileShot, Buf);
                  free(Buf);
              }
          break;

          case 3:
              goto standard_zx_mode;
          }
      break;

      default:
standard_zx_mode:;
          fwrite(temp.base, 1, 6912, fileShot);
      }
   }
   else
   {
      unsigned dx = temp.ox * temp.obpp / 8;
      unsigned char *scrbuf_unaligned = (unsigned char*)malloc(dx * temp.oy + CACHE_LINE);
      unsigned char *scrbuf = (unsigned char*)align_by(scrbuf_unaligned, CACHE_LINE);
      memset(scrbuf, 0, dx * temp.oy);
      renders[conf.render].func(scrbuf, dx); // render to memory buffer (PAL8, YUY2, RGB15, RGB16, RGB32)

      u8 *ds = (u8 *)malloc(((temp.ox * 3 + 3) & ~3) * temp.oy);
      ConvBgr24(ds, scrbuf, dx);

      Saver[conf.scrshot - 1](fileShot, ds);

      free(ds);
      free(scrbuf_unaligned);
   }
   fclose(fileShot);
   sprintf(statusline, "saving %s", strrchr(fname, '\\') + 1);
   statcnt = 30;
   sshot++;
}
/*
static void VideoFrameSaver()
{
   char fname[FILENAME_MAX];
   static unsigned FrameNum = 0;
   static const char *Format[] = { "scr", "bmp", "png" };
   static const TSaver Saver[] = { SaveBmp, SavePng };

   sprintf(fname, "video%06u.%s", FrameNum, Format[conf.scrshot]);
   addpath(fname);

   FILE *fileShot = fopen(fname, "wb");
   if (!fileShot)
      return;

   if (conf.scrshot == 0)
   {
      fwrite(temp.base, 1, 6912, fileShot);
   }
   else
   {
       unsigned dx = temp.ox * temp.obpp / 8;
       unsigned char *scrbuf_unaligned = (unsigned char*)malloc(dx * temp.oy + CACHE_LINE);
       unsigned char *scrbuf = (unsigned char*)align_by(scrbuf_unaligned, CACHE_LINE);
       memset(scrbuf, 0, dx * temp.oy);
       renders[conf.render].func(scrbuf, dx); // render to memory buffer (PAL8, YUY2, RGB15, RGB16, RGB32)

       u8 *ds = (u8 *)malloc(((temp.ox * 3 + 3) & ~3) * temp.oy);
       ConvBgr24(ds, scrbuf, dx);

       Saver[conf.scrshot - 1](fileShot, ds);

       free(ds);
       free(scrbuf_unaligned);
   }
   fclose(fileShot);
   FrameNum++;
}

static void VideoNullSaver()
{

}

TVideoSaver VideoSaver = VideoNullSaver;

void main_savevideo()
{
   static bool StartSave = false;

   if(!StartSave)
   {
       sprintf(statusline, "start saving video");
       StartSave = true;
       VideoSaver = VideoFrameSaver;
   }
   else
   {
       sprintf(statusline, "stop saving video");
       StartSave = false;
       VideoSaver = VideoNullSaver;
   }

   statcnt = 30;
}
*/

