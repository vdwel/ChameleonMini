/*
 * MifareUltralight.h
 *
 *  Created on: 20.03.2017
 *      Author: svdwel
 */

#ifndef NTAG21x_H_
#define NTAG21x_H_

#include "Application.h"
#include "ISO14443-3A.h"

#define NTAG21x_UID_SIZE    ISO14443A_UID_SIZE_DOUBLE
#define NTAG213_MEM_SIZE    180
#define NTAG215_MEM_SIZE    540
#define NTAG216_MEM_SIZE    924

void Ntag213AppInit(void);
void Ntag215AppInit(void);
void Ntag216AppInit(void);
void Ntag21xAppReset(void);
void Ntag21xAppTask(void);

uint16_t Ntag21xAppProcess(uint8_t* Buffer, uint16_t BitCount);

void Ntag21xGetUid(ConfigurationUidType Uid);
void Ntag21xSetUid(ConfigurationUidType Uid);



#endif /* NTAG21x_H_ */
