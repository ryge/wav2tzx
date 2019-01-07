/*
 * -------------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE":
 * <andreasryge@gmail.com> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.           Andreas Ryge
 * --------------------------------------------------------------------------------
 */

#include <stdint.h>

struct TZXheader 
{
	char signature[8];
	unsigned char major;
	unsigned char minor;
};

struct ID32 {
	uint16_t size;
	uint8_t number;
	char ** strings;
};

struct ID10 {
	uint16_t pause;
	uint16_t size;
	uint8_t data[65536];
};
