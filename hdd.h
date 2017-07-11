#pragma once
struct ATA_DEVICE
{
   // Геометрия диска   
   unsigned c,h,s;
   u64 lba;

   // Текущая позиция на диске
   u64 lba_cur; // Текущий сектор в lba режиме
   unsigned c_cur; // Текущий цилиндр
   unsigned h_cur; // Текущая головка
   unsigned s_cur; // Текущий сектор
   unsigned n_cur; // Число секторов

#pragma pack(push, 1)
   // Регистры (control block)
   // Чтение (CPU<-HDD)          Запись (CPU->HDD)         
   // 110   | alternate status | device control
   // 111   | drive address    | not used

   // Регистры (command block)
   // Чтение (CPU<-HDD)        Запись (CPU->HDD)
   // 000   | data           | data
   // 001   | error          | features
   // 010   | sector count   | sector count
   // 011   | sector number  | sector number
   //       | lba [7:0]      | lba [7:0]
   // 100   | cylinder low   | cylinder low 
   //       | lba [15:8]     | lba [15:8]
   // 101   | cylinder high  | cylinder high
   //       | lba [23:16]    | lba [23:16]
   // 110   | drive/head     | drive/head
   //       | lba [27:24]    | lba [27:24]
   // 111   | status         | command

   struct
   {
      u8 data;
      u8 err;             // for write, features
      union
      {
         u8 count;
         u8 intreason;
      };

      struct
      {
          union
          {
              u8 sec;
              u8 lba0;
          };
          union
          {
             u16 cyl;
             u16 atapi_count;
             struct
             {
                u8 cyl_l;
                u8 cyl_h;
             };
             struct
             {
                 u8 lba1;
                 u8 lba2;
             };
             u16 lba12;
          };
          union
          {
              u8 devhead;
              u8 lba3; // Только для lba28
          };
      };

      u8 status;          // for write, cmd
      /*                  */
      u8 control;         // reg8 - control (CS1,DA=6)
      u8 feat;
      u8 cmd;
      u8 reserved;        // reserved

      // только для lba48
      u8 lba4;
      u8 lba5;
      u8 lba6;
      u8 count1;
   } reg;
#pragma pack(pop)

   u8 *regs_w[2][9]; // Регистры для записи
   u8 *regs_r[2][9]; // Регистры для чтения
   unsigned regs_sel; // Текущий блок регистров для lba48 (0/1)

   unsigned char intrq;
   unsigned char readonly;
   unsigned char device_id;             // 0x00 - master, 0x10 - slave
   unsigned char atapi;                 // flag for CD-ROM device

   ATA_DEVICE()
   {
       regs_w[0][0] = &reg.data;
       regs_w[0][1] = &reg.feat;
       regs_w[0][2] = &reg.count;
       regs_w[0][3] = &reg.lba0;
       regs_w[0][4] = &reg.lba1;
       regs_w[0][5] = &reg.lba2;
       regs_w[0][6] = &reg.devhead;
       regs_w[0][7] = &reg.cmd;
       regs_w[0][8] = &reg.control;

       regs_w[1][0] = &reg.data;
       regs_w[1][1] = &reg.feat;
       regs_w[1][2] = &reg.count1;
       regs_w[1][3] = &reg.lba4;
       regs_w[1][4] = &reg.lba5;
       regs_w[1][5] = &reg.lba6;
       regs_w[1][6] = &reg.devhead;
       regs_w[1][7] = &reg.cmd;
       regs_w[1][8] = &reg.control;

       regs_r[0][0] = &reg.data;
       regs_r[0][1] = &reg.err;
       regs_r[0][2] = &reg.count;
       regs_r[0][3] = &reg.lba0;
       regs_r[0][4] = &reg.lba1;
       regs_r[0][5] = &reg.lba2;
       regs_r[0][6] = &reg.devhead;
       regs_r[0][7] = &reg.status;
       regs_r[0][8] = &reg.status;

       regs_r[1][0] = &reg.data;
       regs_r[1][1] = &reg.err;
       regs_r[1][2] = &reg.count1;
       regs_r[1][3] = &reg.lba4;
       regs_r[1][4] = &reg.lba5;
       regs_r[1][5] = &reg.lba6;
       regs_r[1][6] = &reg.devhead;
       regs_r[1][7] = &reg.status;
       regs_r[1][8] = &reg.status;

       regs_sel = 0;
   }

   unsigned char read(unsigned n_reg);
   void write(unsigned n_reg, unsigned char data);
   unsigned read_data();
   void write_data(unsigned data);
   unsigned char read_intrq();

   void update_regs(); // Обновление регистров в соответствии с текущими внутренними значениями chs/lba (при чтении регистров)
   void update_cur(); // Обновление текущих внутренних значений chs/lba в соответствии с данными в регистрах (при записи в регистры)

   char exec_ata_cmd(unsigned char cmd);
   char exec_atapi_cmd(unsigned char cmd);

   enum RESET_TYPE { RESET_HARD, RESET_SOFT, RESET_SRST };
   void reset_signature(RESET_TYPE mode = RESET_SOFT);

   void reset(RESET_TYPE mode);
   char seek();
   void recalibrate();
   void configure(IDE_CONFIG *cfg);
   void prepare_id();
   void command_ok();
   void next_sector();
   void read_sectors();
   void verify_sectors();
   void write_sectors();
   void format_track();

   enum ATAPI_INT_REASON
   {
      INT_COD       = 0x01,
      INT_IO        = 0x02,
      INT_RELEASE   = 0x04
   };

   enum HD_STATUS
   {
      STATUS_BSY   = 0x80,
      STATUS_DRDY  = 0x40,
      STATUS_DF    = 0x20,
      STATUS_DSC   = 0x10,
      STATUS_DRQ   = 0x08,
      STATUS_CORR  = 0x04,
      STATUS_IDX   = 0x02,
      STATUS_ERR   = 0x01
   };

   enum HD_ERROR
   {
      ERR_BBK   = 0x80,
      ERR_UNC   = 0x40,
      ERR_MC    = 0x20,
      ERR_IDNF  = 0x10,
      ERR_MCR   = 0x08,
      ERR_ABRT  = 0x04,
      ERR_TK0NF = 0x02,
      ERR_AMNF  = 0x01
   };

   enum HD_CONTROL
   {
      CONTROL_HOB  = 0x80, // ATA-6
      CONTROL_SRST = 0x04,
      CONTROL_nIEN = 0x02
   };

   enum HD_STATE
   {
      S_IDLE = 0, S_READ_ID,
      S_READ_SECTORS, S_VERIFY_SECTORS, S_WRITE_SECTORS, S_FORMAT_TRACK,
      S_RECV_PACKET, S_READ_ATAPI,
      S_MODE_SELECT
   };

   HD_STATE state;
   unsigned transptr, transcount;
   unsigned phys_dev;
   unsigned char transbf[0xFFFF]; // ATAPI is able to tranfer 0xFFFF bytes. passing more leads to error

   void handle_atapi_packet();
   void handle_atapi_packet_emulate();
   void exec_mode_select();

   ATA_PASSER ata_p;
   ATAPI_PASSER atapi_p;
   bool loaded() { return ata_p.loaded() || atapi_p.loaded(); } //was crashed at atapi_p.loaded() if no master or slave device!!! see fix in ATAPI_PASSER //Alone Coder
   bool selected() { return !((reg.devhead ^ device_id) & 0x10); }
};

struct ATA_PORT
{
   ATA_DEVICE dev[2];
   unsigned char read_high, write_high;

   ATA_PORT()
   {
       dev[0].device_id = 0;
       dev[1].device_id = 0x10;
       reset();
   }

   unsigned char read(unsigned n_reg);
   void write(unsigned n_reg, unsigned char data);
   unsigned read_data();
   void write_data(unsigned data);
   unsigned char read_intrq();

   void reset();
};

extern PHYS_DEVICE phys[];
extern int n_phys;

unsigned find_hdd_device(char *name);
void init_hdd_cd();
