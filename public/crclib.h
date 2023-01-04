/*
crclib.c - generate crc stuff
Copyright (C) 2007 Uncle Mike
Copyright (C) 2019 a1batross
Copyright (C) 2022 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#pragma once
#ifndef CRCLIB_H
#define CRCLIB_H

#include <stdint.h>

struct MD5Context_t
{
	uint32_t	buf[4];
	uint32_t	bits[2];
	uint32_t	in[16];
};

void CRC32_Init(uint32_t *pulCRC);
uint8_t CRC32_BlockSequence(uint8_t *base, int length, int sequence);
void CRC32_ProcessBuffer(uint32_t *pulCRC, const void *pBuffer, int nBuffer);
void CRC32_ProcessByte(uint32_t *pulCRC, uint8_t ch);
uint32_t CRC32_Final(uint32_t pulCRC);
void MD5Init(MD5Context_t *ctx);
void MD5Update(MD5Context_t *ctx, const uint8_t *buf, uint32_t len);
void MD5Final(uint8_t digest[16], MD5Context_t *ctx);
char *MD5_Print(uint8_t hash[16]);

#endif // CRCLIB_H
