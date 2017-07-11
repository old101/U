#pragma once

const int Z80FQ = 3500000; // todo: #define as (conf.frame*conf.intfq)
const int FDD_RPS = 5; // rotation speed

const int MAX_TRACK_LEN = 6250;
const int MAX_CYLS = 86;            // don't load images with so many tracks
const int MAX_PHYS_CYL = 86;        // don't seek over it
const int MAX_SEC = 256;

struct SECHDR
{
   unsigned char c,s,n,l;
   unsigned short crc; // CRC заголовка сектора

   // флаги crc зоны адреса и данных:
   // При форматировании:
   //   0 - генерируется правильное значение crc
   //   1 - запись crc из crc(для адреса)/crcd(для данных)
   //   2 - ошибочный crc (генерируется инверсией правильного crc))
   // При чтении (функция seek устанавливает поля c1 и c2):
   //   0 - рассчитанное crc не совпадает с указанным в заголовке (возвращается crc error)
   //   1 - рассчитанное crc совпадает с указанным в заголовке
   unsigned char c1, c2;
   u8 *data; // Указатель на данные сектора внутри трэка
   u8 *id; // Указатель на заголовок сектора внутри трэка
   u8 *wp; // Указатель на битовую карту сбойных байтов сектора внутри трэка (используется только при загрузке)
   unsigned wp_start; // Номер первого бита в карте сбойных байтов (относительно начала трэка) для данного сектора
   unsigned datlen; // Размер сектора в байтах
   unsigned crcd;        // used to load specific CRC from FDI-file
};

enum SEEK_MODE { JUST_SEEK = 0, LOAD_SECTORS = 1 };

static inline bool test_bit(const u8 *data, unsigned bit)
{
    return (data[bit >> 3] & (1U << (bit & 7))) != 0;
}

static inline void set_bit(u8 *data, unsigned bit)
{
    data[bit >> 3] |= (1U << (bit & 7));
}

static inline void clr_bit(u8 *data, unsigned bit)
{
    data[bit >> 3] &= ~(1U << (bit & 7));
}


struct TRKCACHE
{
   // cached track position
   struct FDD *drive;
   unsigned cyl, side;

   // generic track data
   unsigned trklen;
    // pointer to data inside UDI
   u8 *trkd; // данные
   u8 *trki; // битовая карта синхроимпульсов
   u8 *trkwp; // битовая карта сбойных байтов
   unsigned ts_byte;                 // cpu.t per byte
   SEEK_MODE sf;                     // flag: is sectors filled
   unsigned s;                       // no. of sectors

   // sectors on track
   SECHDR hdr[MAX_SEC];

   void set_i(unsigned pos) { set_bit(trki, pos); }
   void clr_i(unsigned pos) { clr_bit(trki, pos); }
   bool test_i(unsigned pos) { return test_bit(trki, pos); }

   void set_wp(unsigned pos) { set_bit(trkwp, pos); }
   bool test_wp(unsigned pos) { return test_bit(trkwp, pos); }

   void write(unsigned pos, unsigned char byte, char index)
   {
       if(!trkd)
           return;

       trkd[pos] = byte;
       if (index)
           set_i(pos);
       else
           clr_i(pos);
   }

   void seek(FDD *d, unsigned cyl, unsigned side, SEEK_MODE fs);
   void format(); // before use, call seek(d,c,s,JUST_SEEK), set s and hdr[]
   int write_sector(unsigned sec, unsigned char *data); // call seek(d,c,s,LOAD_SECTORS)
   const SECHDR *get_sector(unsigned sec) const; // before use, call fill(d,c,s,LOAD_SECTORS)

   void dump();
   void clear()
   {
       drive = 0;
       trkd = 0;
       ts_byte = Z80FQ/(MAX_TRACK_LEN * FDD_RPS);
   }
   TRKCACHE() { clear(); }
};


struct FDD
{
   u8 Id;
   // drive data

   __int64 motor;       // 0 - not spinning, >0 - time when it'll stop
   unsigned char track; // head position

   // disk data

   unsigned char *rawdata;              // used in VirtualAlloc/VirtualFree
   unsigned rawsize;
   unsigned cyls, sides;
   unsigned trklen[MAX_CYLS][2];
   u8 *trkd[MAX_CYLS][2]; // данные
   u8 *trki[MAX_CYLS][2]; // битовые карты синхроимпульсов
   u8 *trkwp[MAX_CYLS][2]; // битовые карты сбойных байтов
   unsigned char optype; // bits: 0-not modified, 1-write sector, 2-format track
   unsigned char snaptype;

   TRKCACHE t; // used in read/write image
   char name[0x200];
   char dsc[0x200];

   char test();
   void free();
   int index();

   void format_trd(unsigned CylCnt); // Используется только для wldr_trd
   void emptydisk(unsigned FreeSecCnt); // Используется только для wldr_trd
   int addfile(unsigned char *hdr, unsigned char *data); // Используется только для wldr_trd
   void addboot(); // Используется только для wldr_trd

   void newdisk(unsigned cyls, unsigned sides);

   int read(unsigned char snType);

   int read_scl();
   int read_hob();
   int read_trd();
   int write_trd(FILE *ff);
   int read_fdi();
   int write_fdi(FILE *ff);
   int read_td0();
   int write_td0(FILE *ff);
   int read_udi();
   int write_udi(FILE *ff);

   void format_isd();
   int read_isd();
   int write_isd(FILE *ff);

   void format_pro();
   int read_pro();
   int write_pro(FILE *ff);

   ~FDD() { free(); }
};


struct WD1793
{
   enum WDSTATE
   {
      S_IDLE = 0,
      S_WAIT,

      S_DELAY_BEFORE_CMD,
      S_CMD_RW,
      S_FOUND_NEXT_ID,
      S_RDSEC,
      S_READ,
      S_WRSEC,
      S_WRITE,
      S_WRTRACK,
      S_WR_TRACK_DATA,

      S_TYPE1_CMD,
      S_STEP,
      S_SEEKSTART,
      S_RESTORE,
      S_SEEK,
      S_VERIFY,
      S_VERIFY2,

      S_WAIT_HLT,
      S_WAIT_HLT_RW,

      S_RESET
   };

   __int64 next, time;
   __int64 idx_tmo;

   FDD *seldrive;
   unsigned tshift;

   WDSTATE state, state2;

   unsigned char cmd;
   unsigned char data, track, sector;
   unsigned char rqs, status;
   u8 sign_status; // Внешние сигналы (пока только HLD)

   unsigned drive, side;                // update this with changing 'system'

   signed char stepdirection;
   unsigned char system;                // beta128 system register

   unsigned idx_cnt; // idx counter

   // read/write sector(s) data
   __int64 end_waiting_am;
   unsigned foundid;                    // index in trkcache.hdr for next encountered ID and bytes before this ID
   unsigned rwptr, rwlen;

   // format track data
   unsigned start_crc;

   enum CMDBITS
   {
      CMD_SEEK_RATE     = 0x03,
      CMD_SEEK_VERIFY   = 0x04,
      CMD_SEEK_HEADLOAD = 0x08,
      CMD_SEEK_TRKUPD   = 0x10,
      CMD_SEEK_DIR      = 0x20,

      CMD_WRITE_DEL     = 0x01,
      CMD_SIDE_CMP_FLAG = 0x02,
      CMD_DELAY         = 0x04,
      CMD_SIDE          = 0x08,
      CMD_SIDE_SHIFT    = 3,
      CMD_MULTIPLE      = 0x10
   };

   enum BETA_STATUS
   {
      DRQ   = 0x40,
      INTRQ = 0x80
   };

   enum WD_STATUS
   {
      WDS_BUSY      = 0x01,
      WDS_INDEX     = 0x02,
      WDS_DRQ       = 0x02,
      WDS_TRK00     = 0x04,
      WDS_LOST      = 0x04,
      WDS_CRCERR    = 0x08,
      WDS_NOTFOUND  = 0x10,
      WDS_SEEKERR   = 0x10,
      WDS_RECORDT   = 0x20,
      WDS_HEADL     = 0x20,
      WDS_WRFAULT   = 0x20,
      WDS_WRITEP    = 0x40,
      WDS_NOTRDY    = 0x80
   };

   enum WD_SYS
   {
      SYS_HLT       = 0x08
   };

   enum WD_SIG
   {
       SIG_HLD      = 0x01
   };

   unsigned char in(unsigned char port);
   void out(unsigned char port, unsigned char val);
   u8 RdStatus();

   void process();
   void find_marker();
   char notready();
   void load();
   void getindex();
   void trdos_traps();

//   TRKCACHE trkcache;
   FDD fdd[4];

   WD1793()
   {
       for(unsigned i = 0; i < 4; i++) // [vv] Для удобства отладки
           fdd[i].Id = i;
       seldrive = &fdd[0];
       idx_cnt = 0;
       idx_tmo = LLONG_MAX;
       sign_status = 0;
   }
};
