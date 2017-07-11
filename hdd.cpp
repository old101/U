#include "std.h"

#include "emul.h"
#include "vars.h"

#include "util.h"

//#define DUMP_HDD_IO 1

const int MAX_DEVICES = MAX_PHYS_HD_DRIVES+2*MAX_PHYS_CD_DRIVES;

PHYS_DEVICE phys[MAX_DEVICES];
int n_phys = 0;

/*
// this function is untested
void ATA_DEVICE::exec_mode_select()
{
   intrq = 1;
   command_ok();

   struct {
      SCSI_PASS_THROUGH_DIRECT p;
      unsigned char sense[0x40];
   } srb = { 0 }, dst;

   srb.p.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
   *(CDB*)&srb.p.Cdb = cdb;
   srb.p.CdbLength = sizeof(CDB);
   srb.p.DataIn = SCSI_IOCTL_DATA_OUT;
   srb.p.TimeOutValue = 10;
   srb.p.DataBuffer = transbf;
   srb.p.DataTransferLength = transcount;
   srb.p.SenseInfoLength = sizeof(srb.sense);
   srb.p.SenseInfoOffset = sizeof(SCSI_PASS_THROUGH_DIRECT);

   DWORD outsize;
   int r = DeviceIoControl(hDevice, IOCTL_SCSI_PASS_THROUGH_DIRECT,
                           &srb.p, sizeof(srb.p),
                           &dst, sizeof(dst),
                           &outsize, 0);

   if (!r) return;
   if (senselen = dst.p.SenseInfoLength) memcpy(sense, dst.sense, senselen);
   return;
}
*/

void init_hdd_cd()
{
   memset(&phys, 0, sizeof phys);
   if (conf.ide_skip_real)
       return;

   n_phys = 0;
   n_phys = ATA_PASSER::identify(phys + n_phys, MAX_DEVICES - n_phys);
   n_phys += ATAPI_PASSER::identify(phys + n_phys, MAX_DEVICES - n_phys);

   if (!n_phys)
       errmsg("HDD/CD emulator can't access physical drives");
}

void delstr_spaces(char *dst, char *src)
{
   for (; *src; src++)
      if (*src != ' ') *dst++ = *src;
   *dst = 0;
}

unsigned find_hdd_device(char *name)
{
   char s2[512];
   delstr_spaces(s2, name);
//   if(temp.win9x)
   for (int drive = 0; drive < n_phys; drive++)
   {
      char s1[512];
      delstr_spaces(s1, phys[drive].viewname);
      if (!stricmp(s1,s2))
          return drive;
   }
   return -1;
}

void ATA_DEVICE::configure(IDE_CONFIG *cfg)
{
   atapi_p.close(); ata_p.close();

   c = cfg->c, h = cfg->h, s = cfg->s, lba = cfg->lba; readonly = cfg->readonly;

   memset(&reg, 0, sizeof(reg)); // Очищаем регистры
   command_ok(); // Сбрасываем состояние и позицию передачи данных

   phys_dev = -1;
   if (!*cfg->image)
       return;

   PHYS_DEVICE filedev, *dev;
   phys_dev = find_hdd_device(cfg->image);
   if (phys_dev == -1)
   {
      if (cfg->image[0] == '<')
      {
          errmsg("no physical device %s", cfg->image);
          *cfg->image = 0;
          return;
      }
      strcpy(filedev.filename, cfg->image);
      filedev.type = cfg->cd ? ATA_FILECD : ATA_FILEHDD;
      dev = &filedev;
   }
   else
   {
      dev = &phys[phys_dev];
      if (dev->type == ATA_NTHDD)
      {
         // read geometry from id sector
         c = *(unsigned short*)(phys[phys_dev].idsector+2);
         h = *(unsigned short*)(phys[phys_dev].idsector+6);
         s = *(unsigned short*)(phys[phys_dev].idsector+12);
         lba = *(unsigned*)(phys[phys_dev].idsector+0x78); // lba28
         if(*((u16*)(phys[phys_dev].idsector+83*2)) & (1<<10))
         {
             lba = *(u64*)(phys[phys_dev].idsector+100*2); // lba48
         }
         if (!lba)
             lba = c*h*s;
      }
   }
   DWORD errcode;
   if (dev->type == ATA_NTHDD || dev->type == ATA_FILEHDD)
   {
       dev->usage = ATA_OP_USE;
       errcode = ata_p.open(dev);
       atapi = 0;
   }

   if (dev->type == ATA_SPTI_CD || dev->type == ATA_ASPI_CD || dev->type == ATA_FILECD)
   {
       dev->usage = ATA_OP_USE;
       errcode = atapi_p.open(dev);
       atapi = 1;
   }

   if (errcode == NO_ERROR)
       return;
   errmsg("failed to open %s", cfg->image);
   err_win32(errcode);
   *cfg->image = 0;
}

void ATA_PORT::reset()
{
   dev[0].reset(ATA_DEVICE::RESET_HARD);
   dev[1].reset(ATA_DEVICE::RESET_HARD);
}

unsigned char ATA_PORT::read(unsigned n_reg)
{
   u8 val1 = dev[0].read(n_reg);
   u8 val2 = dev[1].read(n_reg);

   unsigned devs = 0;
   devs |= (dev[0].loaded() ? 1 : 0);
   devs |= (dev[1].loaded() ? 2 : 0);

   u8 val = 0xFF;
   switch(devs)
   {
   case 1: val = val1; break;
   case 2: val = val2; break;
   case 3: val = dev[0].selected() ? val1 : val2; break;
   }

#ifdef DUMP_HDD_IO
   printf("R%X:%02X ", n_reg, val);
#endif
   return val;
}

unsigned ATA_PORT::read_data()
{
#ifdef DUMP_HDD_IO
   unsigned val = dev[0].read_data() & dev[1].read_data();
   printf("r%04X ", val & 0xFFFF);
   return val;
#endif
   return dev[0].read_data() & dev[1].read_data();
}

void ATA_PORT::write(unsigned n_reg, unsigned char data)
{
#ifdef DUMP_HDD_IO
   printf("R%X=%02X ", n_reg, data);
#endif
   dev[0].write(n_reg, data);
   dev[1].write(n_reg, data);
}

void ATA_PORT::write_data(unsigned data)
{
#ifdef DUMP_HDD_IO
   printf("w%04X ", data & 0xFFFF);
#endif
   dev[0].write_data(data);
   dev[1].write_data(data);
}

unsigned char ATA_PORT::read_intrq()
{
#ifdef DUMP_HDD_IO
unsigned char i = dev[0].read_intrq() & dev[1].read_intrq(); printf("i%d ", !!i); return i;
#endif
   return dev[0].read_intrq() & dev[1].read_intrq();
}

void ATA_DEVICE::reset_signature(RESET_TYPE mode)
{
   reg.count = reg.sec = 1;
   reg.err = 1;
   reg.cyl = atapi ? 0xEB14 : 0;
   reg.devhead |= 0x50;
   reg.devhead &= (atapi && mode == RESET_SOFT) ? 0x10 : 0;
   reg.status = (mode == RESET_SOFT || !atapi) ? STATUS_DRDY | STATUS_DSC : 0;
}

void ATA_DEVICE::reset(RESET_TYPE mode)
{
   reg.control = 0; // clear SRST
   intrq = 0;
   regs_sel = 0;

   command_ok();
   reset_signature(mode);
}

void ATA_DEVICE::command_ok()
{
   state = S_IDLE;
   transptr = -1;
   reg.err = 0;
   reg.status = STATUS_DRDY | STATUS_DSC;
}

unsigned char ATA_DEVICE::read_intrq()
{
   if (!loaded() || ((reg.devhead ^ device_id) & 0x10) || (reg.control & CONTROL_nIEN)) return 0xFF;
   return intrq? 0xFF : 0x00;
}

unsigned char ATA_DEVICE::read(unsigned n_reg)
{
   if (!loaded())
       return 0xFF;

/*
   if ((reg.devhead ^ device_id) & 0x10)
   {
       return 0xFF;
   }
*/

   if (n_reg == 7)
       intrq = 0;
   if (n_reg == 8)
       n_reg = 7; // read alt.status -> read status

   if ((n_reg == 7) && ((reg.devhead ^ device_id) & 0x10))
   {
       return 0;
   }

   if (n_reg == 7 || (reg.status & STATUS_BSY))
   {
//	   printf("state=%d\n",state); //Alone Coder
	   return reg.status;
   } // BSY=1 or read status
   // BSY = 0
   //// if (reg.status & STATUS_DRQ) return 0xFF;    // DRQ.  ATA-5: registers should not be queried while DRQ=1, but programs do this!

   update_regs();
   // DRQ = 0
   unsigned sel = regs_sel;
   if(lba > 0xFFFFFFFULL)
   { // lba48
       sel ^= (reg.control & CONTROL_HOB) ? 1 : 0;
   }

   return *regs_r[sel][n_reg];
}

unsigned ATA_DEVICE::read_data()
{
   if (!loaded())
       return 0xFFFFFFFF;
   if ((reg.devhead ^ device_id) & 0x10)
       return 0xFFFFFFFF;
   if (/* (reg.status & (STATUS_DRQ | STATUS_BSY)) != STATUS_DRQ ||*/ transptr >= transcount)
       return 0xFFFFFFFF;

   // DRQ=1, BSY=0, data present
   unsigned result = *(unsigned*)(transbf + transptr*2);
   transptr++;
//   printf(__FUNCTION__" data=0x%04X\n", result & 0xFFFF);

   if (transptr < transcount)
       return result;
   // look to state, prepare next block
   if (state == S_READ_ID || state == S_READ_ATAPI)
       command_ok();
   if (state == S_READ_SECTORS)
   {
//       __debugbreak();
//       printf("dev=%d, cnt=%d\n", device_id, reg.count);
       if(!--reg.count)
           command_ok();
       else
       {
           next_sector();
           read_sectors();
       }
   }

   return result;
}

char ATA_DEVICE::exec_ata_cmd(unsigned char cmd)
{
//   printf(__FUNCTION__" cmd=%02X\n", cmd);
   // EXECUTE DEVICE DIAGNOSTIC for both ATA and ATAPI
   if (cmd == 0x90)
   {
       reset_signature(RESET_SOFT);
       return 1;
   }

   if (atapi)
       return 0;

   // INITIALIZE DEVICE PARAMETERS
   if (cmd == 0x91)
   {
     // pos = (reg.cyl * h + (reg.devhead & 0x0F)) * s + reg.sec - 1;
     h = (reg.devhead & 0xF) + 1;
     s = reg.count;
     if(s == 0)
     {
          reg.status = STATUS_DRDY | STATUS_DF | STATUS_DSC | STATUS_ERR;
          return 1;
     }

     c = lba / s / h;

     reg.status = STATUS_DRDY | STATUS_DSC;
     return 1;
   }

   if ((cmd & 0xFE) == 0x20) // ATA-3 (mandatory), read sectors (20-w-retr/21-wo-retr)
   { // cmd #21 obsolette, rqd for is-dos
//       printf(__FUNCTION__" sec_cnt=%d\n", reg.count);
//       __debugbreak();
       read_sectors();
       return 1;
   }

   if ((cmd == 0x24) && (lba > 0xFFFFFFFULL)) // ATA-6 read sectors ext (lba48)
   {
       read_sectors();
       return 1;
   }


   if((cmd & 0xFE) == 0x40) // ATA-3 (mandatory),  verify sectors
   { //rqd for is-dos
       verify_sectors();
       return 1;
   }

   if ((cmd == 0x42) && (lba > 0xFFFFFFFULL)) // ATA-6 verify sectors ext (lba48)
   {
       verify_sectors();
       return 1;
   }

   if ((cmd & 0xFE) == 0x30 && !readonly) // ATA-3 (mandatory), write sectors (30-w-retr,31-wo-retr)
   {
      if (seek())
      {
          state = S_WRITE_SECTORS;
          reg.status = STATUS_DRQ | STATUS_DSC;
          transptr = 0;
          transcount = 0x100;
      }
      return 1;
   }

   if ((cmd == 0x34) && (lba > 0xFFFFFFFULL) && !readonly) // ATA-6 write sectors ext (lba48)
   {
      if (seek())
      {
          state = S_WRITE_SECTORS;
          reg.status = STATUS_DRQ | STATUS_DSC;
          transptr = 0;
          transcount = 0x100;
      }
      return 1;
   }

   if(cmd == 0x50) // format track (данная реализация - ничего не делает)
   {
      reg.sec = 1;
      if (seek())
      {
          state = S_FORMAT_TRACK;
          reg.status = STATUS_DRQ | STATUS_DSC;
          transptr = 0;
          transcount = 0x100;
      }
      return 1;
   }

   if (cmd == 0xEC)
   {
       prepare_id();
       return 1;
   }

   if (cmd == 0xE7)
   { // FLUSH CACHE
      if (ata_p.flush())
      {
          command_ok();
          intrq = 1;
      }
      else
          reg.status = STATUS_DRDY | STATUS_DF | STATUS_DSC | STATUS_ERR; // 0x71
      return 1;
   }

   if (cmd == 0x10)
   {
      recalibrate();
      command_ok();
      intrq = 1;
      return 1;
   }

   if (cmd == 0x70)
   { // seek
      if (!seek())
          return 1;
      command_ok();
      intrq = 1;
      return 1;
   }

   printf("*** unknown ata cmd %02X ***\n", cmd);

   return 0;
}

char ATA_DEVICE::exec_atapi_cmd(unsigned char cmd)
{
   if (!atapi)
       return 0;

   // soft reset
   if (cmd == 0x08)
   {
       reset(RESET_SOFT);
       return 1;
   }
   if (cmd == 0xA1) // IDENTIFY PACKET DEVICE
   {
       prepare_id();
       return 1;
   }

   if (cmd == 0xA0)
   { // packet
      state = S_RECV_PACKET;
      reg.status = STATUS_DRQ;
      reg.intreason = INT_COD;
      transptr = 0;
      transcount = 6;
      return 1;
   }

   if (cmd == 0xEC)
   {
       reg.count = 1;
       reg.sec = 1;
       reg.cyl = 0xEB14;

       reg.status = STATUS_DSC | STATUS_DRDY | STATUS_ERR;
       reg.err = ERR_ABRT;
       state = S_IDLE;
       intrq = 1;
       return 1;
   }

   printf("*** unknown atapi cmd %02X ***\n", cmd);
   // "command aborted" with ATAPI signature
   reg.count = 1;
   reg.sec = 1;
   reg.cyl = 0xEB14;
   return 0;
}

void ATA_DEVICE::write(unsigned n_reg, unsigned char data)
{
//   printf("dev=%d, reg=%d, data=%02X\n", device_id, n_reg, data);
   if (!loaded())
       return;

   reg.control &= ~CONTROL_HOB;

   if (n_reg == 1)
   {
       reg.feat = data;
       return;
   }

   if (n_reg != 7) // Не регистр команд
   {
      *regs_w[regs_sel][n_reg] = data;
      regs_sel ^= (lba > 0xFFFFFFFULL) ? 1 : 0;

      update_cur();

      if (reg.control & CONTROL_SRST)
      {
//          printf("dev=%d, reset\n", device_id);
          reset(RESET_SRST);
      }
      return;
   }

   // execute command!
   if (((reg.devhead ^ device_id) & 0x10) && data != 0x90)
       return;
   if (!(reg.status & STATUS_DRDY) && !atapi)
   {
       printf("warning: hdd not ready cmd = %02X (ignored)\n", data);
       return;
   }

   reg.err = 0; intrq = 0;

//{printf(" [");for (int q=1;q<9;q++) printf("-%02X",regs[q]);printf("]\n");}
   if (exec_atapi_cmd(data))
       return;
   if (exec_ata_cmd(data))
       return;
   reg.status = STATUS_DSC | STATUS_DRDY | STATUS_ERR;
   reg.err = ERR_ABRT;
   state = S_IDLE; intrq = 1;
}

void ATA_DEVICE::write_data(unsigned data)
{
   if (!loaded()) return;
   if ((reg.devhead ^ device_id) & 0x10)
       return;
   if (/* (reg.status & (STATUS_DRQ | STATUS_BSY)) != STATUS_DRQ ||*/ transptr >= transcount)
       return;
   *(unsigned short*)(transbf + transptr*2) = (unsigned short)data; transptr++;
   if (transptr < transcount)
       return;
   // look to state, prepare next block
   if (state == S_WRITE_SECTORS)
   {
       write_sectors();
       return;
   }

   if (state == S_FORMAT_TRACK)
   {
       format_track();
       return;
   }

   if (state == S_RECV_PACKET)
   {
       handle_atapi_packet();
       return;
   }
/*   if (state == S_MODE_SELECT) { exec_mode_select(); return; } */
}

char ATA_DEVICE::seek()
{
   u64 pos;
   if (reg.devhead & 0x40)
   {
      pos = lba_cur;
      if (lba_cur >= lba)
      {
//          printf("seek error: lba %I64u:%I64u\n", lba, pos);

          seek_err:
          reg.status = STATUS_DRDY | STATUS_DF | STATUS_ERR;
          reg.err = ERR_IDNF | ERR_ABRT;
          intrq = 1;
          return 0;
      }
//      printf("lba %I64u:%I64u\n", lba, pos);
   }
   else
   {
      if (c_cur >= c || h_cur >= h || s_cur > s || s_cur == 0)
      {
//          printf("seek error: chs %4d/%02d/%02d\n", c_cur,  h_cur, s_cur);
          goto seek_err;
      }
      pos = (c_cur * h + h_cur) * s + s_cur - 1;
//      printf("chs %4d/%02d/%02d: %I64u\n", c_cur,  h_cur, s_cur, pos);
   }
//printf("[seek %I64u]", pos << 9);
   if (!ata_p.seek(pos))
   {
      reg.status = STATUS_DRDY | STATUS_DF | STATUS_ERR;
      reg.err = ERR_IDNF | ERR_ABRT;
      intrq = 1;
      return 0;
   }
   return 1;
}

void ATA_DEVICE::format_track()
{
   intrq = 1;
   if(!seek())
       return;

   command_ok();
   return;
}

void ATA_DEVICE::write_sectors()
{
   intrq = 1;
//printf(" [write] ");
   if(!seek())
       return;

   if (!ata_p.write_sector(transbf))
   {
      reg.status = STATUS_DRDY | STATUS_DSC | STATUS_ERR;
      reg.err = ERR_UNC;
      state = S_IDLE;
      return;
   }

   if (!--reg.count)
   {
       command_ok();
       return;
   }
   next_sector();

   transptr = 0, transcount = 0x100;
   state = S_WRITE_SECTORS;
   reg.err = 0;
   reg.status = STATUS_DRQ | STATUS_DSC;
}

void ATA_DEVICE::read_sectors()
{
//   __debugbreak();
   intrq = 1;
   if (!seek())
      return;

   if (!ata_p.read_sector(transbf))
   {
      reg.status = STATUS_DRDY | STATUS_DSC | STATUS_ERR;
      reg.err = ERR_UNC | ERR_IDNF;
      state = S_IDLE;
      return;
   }
   transptr = 0;
   transcount = 0x100;
   state = S_READ_SECTORS;
   reg.err = 0;
   reg.status = STATUS_DRDY | STATUS_DRQ | STATUS_DSC;

/*
   if(reg.devhead & 0x40)
       printf("dev=%d lba=%d\n", device_id, *(unsigned*)(regs+3) & 0x0FFFFFFF);
   else
       printf("dev=%d c/h/s=%d/%d/%d\n", device_id, reg.cyl, (reg.devhead & 0xF), reg.sec);
*/
}

void ATA_DEVICE::verify_sectors()
{
   intrq = 1;
//   __debugbreak();

   do
   {
       --n_cur;
/*
       if(reg.devhead & 0x40)
           printf("lba=%d\n", *(unsigned*)(regs+3) & 0x0FFFFFFF);
       else
           printf("c/h/s=%d/%d/%d\n", reg.cyl, (reg.devhead & 0xF), reg.sec);
*/
       if (!seek())
           return;
/*
       u8 Buf[512];
       if (!ata_p.read_sector(Buf))
       {
          reg.status = STATUS_DRDY | STATUS_DF | STATUS_CORR | STATUS_DSC | STATUS_ERR;
          reg.err = ERR_UNC | ERR_IDNF | ERR_ABRT | ERR_AMNF;
          state = S_IDLE;
          return;
       }
*/
       if(n_cur)
           next_sector();
   }while(n_cur);
   command_ok();
}

void ATA_DEVICE::next_sector()
{
   if (reg.devhead & 0x40)
   { // LBA
      lba_cur++;
      return;
   }
   // need to recalc CHS for every sector, coz ATA registers
   // should contain current position on failure
   if (s_cur < s)
   {
       s_cur++;
       return;
   }
   s_cur = 1;

   if (++h_cur < h)
   {
       return;
   }
   h_cur = 0;
   c_cur++;
}

void ATA_DEVICE::recalibrate()
{
   lba_cur = 0;
   c_cur = 0;
   h_cur = 0;
   s_cur = 1;

   reg.cyl = 0;
   reg.devhead &= 0xF0;

   if (reg.devhead & 0x40) // LBA
   {
      reg.sec = 0;
      return;
   }

   reg.sec = 1;
}

#define TOC_DATA_TRACK          0x04

// [vv] Работа с файлом - образом диска напрямую
void ATA_DEVICE::handle_atapi_packet_emulate()
{
//    printf("%s\n", __FUNCTION__);
    memcpy(&atapi_p.cdb, transbf, 12);

    switch(atapi_p.cdb.CDB12.OperationCode)
    {
    case SCSIOP_TEST_UNIT_READY:; // 6
          command_ok();
          return;

    case SCSIOP_READ:; // 10
    {
      unsigned cnt = (u32(atapi_p.cdb.CDB10.TransferBlocksMsb) << 8) | atapi_p.cdb.CDB10.TransferBlocksLsb;
      unsigned pos = (u32(atapi_p.cdb.CDB10.LogicalBlockByte0) << 24) |
                     (u32(atapi_p.cdb.CDB10.LogicalBlockByte1) << 16) |
                     (u32(atapi_p.cdb.CDB10.LogicalBlockByte2) << 8) |
                     atapi_p.cdb.CDB10.LogicalBlockByte3;

      if(cnt * 2048 > sizeof(transbf))
      {
          reg.status = STATUS_DRDY | STATUS_DSC | STATUS_ERR;
          reg.err = ERR_UNC | ERR_IDNF;
          state = S_IDLE;
          return;
      }

      for(unsigned i = 0; i < cnt; i++, pos++)
      {
          if (!atapi_p.seek(pos))
          {
             reg.status = STATUS_DRDY | STATUS_DSC | STATUS_ERR;
             reg.err = ERR_UNC | ERR_IDNF;
             state = S_IDLE;
             return;
          }

          if (!atapi_p.read_sector(transbf + i * 2048))
          {
             reg.status = STATUS_DRDY | STATUS_DSC | STATUS_ERR;
             reg.err = ERR_UNC | ERR_IDNF;
             state = S_IDLE;
             return;
          }
      }
      intrq = 1;
      reg.atapi_count = cnt * 2048;
      reg.intreason = INT_IO;
      reg.status = STATUS_DRQ;
      transcount = (cnt * 2048)/2;
      transptr = 0;
      state = S_READ_ATAPI;
      return;
    }

    case SCSIOP_READ_TOC:; // 10
    {
      u8 TOC_DATA[] =
      {
        0, 4+8*2 - 2, 1, 0xAA,
        0, TOC_DATA_TRACK, 1, 0, 0, 0, 0, 0,
        0, TOC_DATA_TRACK, 0xAA, 0, 0, 0, 0, 0,
      };
      unsigned len = sizeof(TOC_DATA);
      memcpy(transbf, TOC_DATA, len);
      reg.atapi_count = len;
      reg.intreason = INT_IO;
      reg.status = STATUS_DRQ;
      transcount = (len + 1)/2;
      transptr = 0;
      state = S_READ_ATAPI;
      return;
    }
    case SCSIOP_START_STOP_UNIT:; // 10
          command_ok();
          return;

    case SCSIOP_SET_CD_SPEED:; // 12
          command_ok();
          return;
    }

    printf("*** unknown scsi cmd %02X ***\n", atapi_p.cdb.CDB12.OperationCode);

    reg.err = 0;
    state = S_IDLE;
    reg.status = STATUS_DSC | STATUS_ERR | STATUS_DRDY;
}

void ATA_DEVICE::handle_atapi_packet()
{
#if defined(DUMP_HDD_IO)
   {
       printf(" [packet");
       for (int i = 0; i < 12; i++)
           printf("-%02X", transbf[i]);
       printf("]\n");
   }
#endif
   if(phys_dev == -1)
       return handle_atapi_packet_emulate();

   memcpy(&atapi_p.cdb, transbf, 12);

   intrq = 1;

   if (atapi_p.cdb.MODE_SELECT10.OperationCode == 0x55)
   { // MODE SELECT requires additional data from host

      state = S_MODE_SELECT;
      reg.status = STATUS_DRQ;
      reg.intreason = 0;
      transptr = 0;
      transcount = atapi_p.cdb.MODE_SELECT10.ParameterListLength[0]*0x100 + atapi_p.cdb.MODE_SELECT10.ParameterListLength[1];
      return;
   }

   if (atapi_p.cdb.CDB6READWRITE.OperationCode == 0x03 && atapi_p.senselen)
   { // REQ.SENSE - read cached
      memcpy(transbf, atapi_p.sense, atapi_p.senselen);
      atapi_p.passed_length = atapi_p.senselen; atapi_p.senselen = 0; // next time read from device
      goto ok;
   }

   if (atapi_p.pass_through(transbf, sizeof transbf))
   {
      if (atapi_p.senselen)
      {
          reg.err = atapi_p.sense[2] << 4;
          goto err;
      } // err = sense key //win9x hangs on drq after atapi packet when emulator does goto err (see walkaround in SEND_ASPI_CMD)
    ok:
      if (!atapi_p.cdb.CDB6READWRITE.OperationCode)
          atapi_p.passed_length = 0; // bugfix in cdrom driver: TEST UNIT READY has no data
      if (!atapi_p.passed_length /* || atapi_p.passed_length == sizeof transbf */ )
      {
          command_ok();
          return;
      }
      reg.atapi_count = atapi_p.passed_length;
      reg.intreason = INT_IO;
      reg.status = STATUS_DRQ;
      transcount = (atapi_p.passed_length+1)/2;
	  //printf("transcount=%d\n",transcount); //32768 in win9x
      transptr = 0;
      state = S_READ_ATAPI;
   }
   else
   { // bus error
      reg.err = 0;
    err:
      state = S_IDLE;
      reg.status = STATUS_DSC | STATUS_ERR | STATUS_DRDY;
   }
}

void ATA_DEVICE::prepare_id()
{
   if (phys_dev == -1)
   {
      memset(transbf, 0, 512);
      make_ata_string(transbf+54, 20, "UNREAL SPECCY HARD DRIVE IMAGE");
      make_ata_string(transbf+20, 10, "0000");
      make_ata_string(transbf+46,  4, VERS_STRING);
      *(unsigned short*)transbf = 0x045A;
      ((unsigned short*)transbf)[1] = (unsigned short)c;
      ((unsigned short*)transbf)[3] = (unsigned short)h;
      ((unsigned short*)transbf)[6] = (unsigned short)s;
      *(unsigned*)(transbf+60*2) = (lba > 0xFFFFFFFULL) ? 0xFFFFFFF : lba; // lba28
      ((unsigned short*)transbf)[20] = 3; // a dual ported multi-sector buffer capable of simultaneous transfers with a read caching capability
      ((unsigned short*)transbf)[21] = 512; // cache size=256k
      ((unsigned short*)transbf)[22] = 4; // ECC bytes
      ((unsigned short*)transbf)[49] = 0x200; // LBA supported
      ((unsigned short*)transbf)[80] = 0x3E; // support specifications up to ATA-5
      ((unsigned short*)transbf)[81] = 0x13; // ATA/ATAPI-5 T13 1321D revision 3
      ((unsigned short*)transbf)[82] = 0x60; // supported look-ahead and write cache

      if(lba > 0xFFFFFFFULL)
      {
          ((unsigned short*)transbf)[83] = 0x400; // lba48 supported
          ((unsigned short*)transbf)[86] = 0x400; // lba48 supported
          *(u64*)(transbf+100*2) = lba; // lba48
      }

      // make checksum
      transbf[510] = 0xA5;
      unsigned char cs = 0;
      for (unsigned i = 0; i < 511; i++)
          cs += transbf[i];
      transbf[511] = 0-cs;
   }
   else
   { // copy as is...
      memcpy(transbf, phys[phys_dev].idsector, 512);
   }

   state = S_READ_ID;
   transptr = 0;
   transcount = 0x100;
   intrq = 1;
   reg.status = STATUS_DRDY | STATUS_DRQ | STATUS_DSC;
   reg.err = 0;
}

void ATA_DEVICE::update_regs()
{
   if(reg.devhead & 0x40)
   { // lba
       if(lba > 0xFFFFFFFULL)
       { // lba48
           reg.lba0 = lba_cur & 0xFF;
           reg.lba1 = (lba_cur >> 8) & 0xFF;
           reg.lba2 = (lba_cur >> 16) & 0xFF;
           reg.lba4 = (lba_cur >> 24) & 0xFF;
           reg.lba5 = (lba_cur >> 32) & 0xFF;
           reg.lba6 = (lba_cur >> 40) & 0xFF;
       }
       else
       { // lba28
           reg.lba0 = lba_cur & 0xFF;
           reg.lba1 = (lba_cur >> 8) & 0xFF;
           reg.lba2 = (lba_cur >> 16) & 0xFF;
           reg.lba3 &= ~0xF;
           reg.lba3 |= (lba_cur >> 24) & 0xF;
       }
   }
   else
   { // chs
       reg.cyl = c_cur;
       reg.devhead &= ~0xF;
       reg.devhead |= h_cur & 0xF;
       reg.sec = s_cur;
   }
}

void ATA_DEVICE::update_cur()
{
   n_cur = reg.count;
   if(lba > 0xFFFFFFFULL)
   { // lba48
       lba_cur = reg.lba0 | (u64(reg.lba1) << 8) | (u64(reg.lba2) << 16) | (u64(reg.lba4) << 24) | (u64(reg.lba5) << 32) | (u64(reg.lba6) << 40);
       n_cur |= (reg.count1 << 8);
   }
   else
   { // lba28
       lba_cur = reg.lba0 | (u64(reg.lba1) << 8) | (u64(reg.lba2) << 16) | (u64(reg.lba3 & 0xF) << 24);
   }

   c_cur = reg.cyl;
   h_cur = reg.devhead & 0xF;
   s_cur = reg.sec;
}
