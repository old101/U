#pragma once

unsigned wd93_crc(unsigned char *ptr, unsigned size);
unsigned short crc16(unsigned char *buf, unsigned size);
void udi_buggy_crc(int &crc, unsigned char *buf, unsigned len);
