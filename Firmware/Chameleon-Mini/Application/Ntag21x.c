/*
 * Ntag21x.c
 *
 *  Created on: 20.03.2017
 *      Author: vdwel
 *		Based on MifareUltralight from author: skuser
 */

#include "Ntag21x.h"
#include "ISO14443-3A.h"
#include "../Codec/ISO14443-2A.h"
#include "../Memory.h"
#include "../LEDHook.h"


#define ATQA_VALUE            0x0044
#define SAK_CL1_VALUE        ISO14443A_SAK_INCOMPLETE
#define SAK_CL2_VALUE        ISO14443A_SAK_COMPLETE_NOT_COMPLIANT

#define NTAG213_VERSION_STORAGE_SIZE 0x0F
#define NTAG213_PWD_ADDRESS			0xAC
#define NTAG213_PACK_ADDRESS		0xB0
#define NTAG213_PAGE_READ_MAX        0x2C
#define NTAG213_PAGE_WRITE_MAX        0x2C

#define NTAG215_VERSION_STORAGE_SIZE 0x11
#define NTAG215_PWD_ADDRESS			0x214
#define NTAG215_PACK_ADDRESS		0x218
#define NTAG215_PAGE_READ_MAX        0x86
#define NTAG215_PAGE_WRITE_MAX        0x86

#define NTAG216_VERSION_STORAGE_SIZE 0x13
#define NTAG216_PWD_ADDRESS			0x394
#define NTAG216_PACK_ADDRESS		0x398
#define NTAG216_PAGE_READ_MAX        0xE6
#define NTAG216_PAGE_WRITE_MAX        0xE6

#define NTAG_VERSION_SIZE	0x08
#define PWD_SIZE			0x04
#define PACK_SIZE  	    	0x02
#define PACK_FRAME_SIZE		0x10
#define ACK_VALUE            0x0A
#define ACK_FRAME_SIZE        4 /* Bits */
#define NAK_INVALID_ARG        0x00
#define NAK_CRC_ERROR        0x01
#define NAK_EEPROM_ERROR    0x05
#define NAK_OTHER_ERROR        0x06
#define NAK_AUTH				0x04
#define NAK_FRAME_SIZE        4

#define CMD_READ                0x30
#define CMD_FAST_READ           0x3A
#define CMD_READ_FRAME_SIZE        2 /* without CRC bytes */
#define CMD_FAST_READ_FRAME_SIZE   3 /* without CRC bytes */
#define CMD_WRITE                0xA2
#define CMD_WRITE_FRAME_SIZE    6 /* without CRC bytes */
#define CMD_COMPAT_WRITE        0xA0
#define CMD_COMPAT_WRITE_FRAME_SIZE 2
#define CMD_HALT                0x50
#define CMD_GET_VERSION			0x60
#define CMD_PWD_AUTH			0x1B
#define CMD_PWD_AUTH_FRAME_SIZE	0x05


#define UID_CL1_ADDRESS        0x00    /* In Card Memory */
#define UID_CL1_SIZE        3        /* In Bytes */
#define UID_BCC1_ADDRESS    0x03
#define UID_CL2_ADDRESS        0x04
#define UID_CL2_SIZE        4
#define UID_BCC2_ADDRESS    0x08

#define BYTES_PER_PAGE        4
#define PAGE_ADDRESS_MASK    0x0F

#define BYTES_PER_READ        16
#define PAGE_READ_MIN        0x00

#define BYTES_PER_WRITE        4
#define PAGE_WRITE_MIN        0x02

#define BYTES_PER_COMPAT_WRITE 16

static enum {
    STATE_HALT,
    STATE_IDLE,
    STATE_READY1,
    STATE_READY2,
    STATE_ACTIVE,
    STATE_COMPAT_WRITE
} State;

static uint8_t CompatWritePageAddress;
static bool FromHalt = false;
static uint8_t VersionStorageSize;
static uint16_t PwdAddress;
static uint16_t PackAddress;
static uint16_t PageReadMax;
static uint16_t PageWriteMax;

void Ntag213AppInit(void)
{
    State = STATE_IDLE;
    FromHalt = false;
    VersionStorageSize = NTAG213_VERSION_STORAGE_SIZE;
    PwdAddress = NTAG213_PWD_ADDRESS;
    PackAddress = NTAG213_PACK_ADDRESS;
    PageReadMax = NTAG213_PAGE_READ_MAX;
    PageWriteMax = NTAG213_PAGE_WRITE_MAX;    
}

void Ntag215AppInit(void)
{
    State = STATE_IDLE;
    FromHalt = false;
    VersionStorageSize = NTAG215_VERSION_STORAGE_SIZE;
    PwdAddress = NTAG215_PWD_ADDRESS;
    PackAddress = NTAG215_PACK_ADDRESS;
    PageReadMax = NTAG215_PAGE_READ_MAX;
    PageWriteMax = NTAG215_PAGE_WRITE_MAX;    
}

void Ntag216AppInit(void)
{
    State = STATE_IDLE;
    FromHalt = false;
    VersionStorageSize = NTAG216_VERSION_STORAGE_SIZE;
    PwdAddress = NTAG216_PWD_ADDRESS;
    PackAddress = NTAG216_PACK_ADDRESS;
    PageReadMax = NTAG216_PAGE_READ_MAX;
    PageWriteMax = NTAG216_PAGE_WRITE_MAX;    
}

void Ntag21xAppReset(void)
{
    State = STATE_IDLE;
}

void Ntag21xAppTask(void)
{

}


uint16_t Ntag21xAppProcess(uint8_t* Buffer, uint16_t BitCount)
{
    uint8_t Cmd = Buffer[0];

    switch(State) {
    case STATE_IDLE:
    case STATE_HALT:
    	FromHalt = State == STATE_HALT;
        if (ISO14443AWakeUp(Buffer, &BitCount, ATQA_VALUE, FromHalt)) {
            /* We received a REQA or WUPA command, so wake up. */
            State = STATE_READY1;
            return BitCount;
        }
        break;

    case STATE_READY1:
        if (ISO14443AWakeUp(Buffer, &BitCount, ATQA_VALUE, FromHalt)) {
            State = FromHalt ? STATE_HALT : STATE_IDLE;
            return ISO14443A_APP_NO_RESPONSE;
        } else if (Cmd == ISO14443A_CMD_SELECT_CL1) {
            /* Load UID CL1 and perform anticollision. Since
            * MF Ntag21x use a double-sized UID, the first byte
            * of CL1 has to be the cascade-tag byte. */
            uint8_t UidCL1[ISO14443A_CL_UID_SIZE] = { [0] = ISO14443A_UID0_CT };

            MemoryReadBlock(&UidCL1[1], UID_CL1_ADDRESS, UID_CL1_SIZE);

            if (ISO14443ASelect(Buffer, &BitCount, UidCL1, SAK_CL1_VALUE)) {
                /* CL1 stage has ended successfully */
                State = STATE_READY2;
            }

            return BitCount;
        } else {
            /* Unknown command. Enter halt state */
            State = STATE_IDLE;
        }
        break;

    case STATE_READY2:
        if (ISO14443AWakeUp(Buffer, &BitCount, ATQA_VALUE, FromHalt)) {
            State = FromHalt ? STATE_HALT : STATE_IDLE;
            return ISO14443A_APP_NO_RESPONSE;
        } else if (Cmd == ISO14443A_CMD_SELECT_CL2) {
            /* Load UID CL2 and perform anticollision */
            uint8_t UidCL2[ISO14443A_CL_UID_SIZE];

            MemoryReadBlock(UidCL2, UID_CL2_ADDRESS, UID_CL2_SIZE);

            if (ISO14443ASelect(Buffer, &BitCount, UidCL2, SAK_CL2_VALUE)) {
                /* CL2 stage has ended successfully. This means
                * our complete UID has been sent to the reader. */
                State = STATE_ACTIVE;
            }

            return BitCount;
        } else {
            /* Unknown command. Enter halt state */
            State = STATE_IDLE;
        }
        break;

    case STATE_ACTIVE:
        if (ISO14443AWakeUp(Buffer, &BitCount, ATQA_VALUE, FromHalt)) {
            State = FromHalt ? STATE_HALT : STATE_IDLE;
            return ISO14443A_APP_NO_RESPONSE;
        } else if (Cmd == CMD_READ) {
            uint8_t PageAddress = Buffer[1];

            if (ISO14443ACheckCRCA(Buffer, CMD_READ_FRAME_SIZE)) {
                if (   (PageAddress >= PAGE_READ_MIN)
                    && (PageAddress <= PageReadMax) ) {
                    /* TODO: Missing address wrap around behaviour.
                    * Implement using a for-loop copying 4 bytes each iteration
                    * and mask pageaddress */
                    MemoryReadBlock(Buffer, PageAddress * BYTES_PER_PAGE, BYTES_PER_READ);
                    ISO14443AAppendCRCA(Buffer, BYTES_PER_READ);
                    return (BYTES_PER_READ + ISO14443A_CRCA_SIZE) * 8;
                } else {
                    Buffer[0] = NAK_INVALID_ARG;
                    return NAK_FRAME_SIZE;
                }
            } else {
                Buffer[0] = NAK_CRC_ERROR;
                return NAK_FRAME_SIZE;
            }
        } else if (Cmd == CMD_FAST_READ) {
            uint8_t PageAddressStart = Buffer[1];
            uint8_t PageAddressEnd = Buffer[2];

            if (ISO14443ACheckCRCA(Buffer, CMD_FAST_READ_FRAME_SIZE)) {
                if (   (PageAddressStart >= PAGE_READ_MIN)
                    && (PageAddressStart <= PageReadMax) ) {
                    /* TODO: Missing address wrap around behaviour.
                    * Implement using a for-loop copying 4 bytes each iteration
                    * and mask pageaddress */
 
                    MemoryReadBlock(Buffer, PageAddressStart * BYTES_PER_PAGE, (PageAddressEnd - PageAddressStart + 1) * BYTES_PER_PAGE);
                    ISO14443AAppendCRCA(Buffer, (PageAddressEnd - PageAddressStart + 1) * BYTES_PER_PAGE);
                    return ((PageAddressEnd - PageAddressStart + 1) * BYTES_PER_PAGE + ISO14443A_CRCA_SIZE) * 8;
                } else {
                    Buffer[0] = NAK_INVALID_ARG;
                    return NAK_FRAME_SIZE;
                }
            } else {
                Buffer[0] = NAK_CRC_ERROR;
                return NAK_FRAME_SIZE;
            }
        } else if (Cmd == CMD_WRITE) {
            /* This is a write command containing 4 bytes of data that
            * should be written to the given page address. */
            uint8_t PageAddress = Buffer[1];
            if (ISO14443ACheckCRCA(Buffer, CMD_WRITE_FRAME_SIZE)) {
                /* CRC check passed */
                if (   (PageAddress >= PAGE_WRITE_MIN)
                    && (PageAddress <= PageWriteMax) ) {
                    /* PageAddress is within bounds. */

                    if (!ActiveConfiguration.ReadOnly) {
                        MemoryWriteBlock(&Buffer[2], PageAddress * BYTES_PER_PAGE, BYTES_PER_WRITE);
                    } else {
                        /* If the chameleon is in read only mode, it silently
                        * ignores any attempt to write data. */
                    }

                    Buffer[0] = ACK_VALUE;
                    return ACK_FRAME_SIZE;

                } else {
                    Buffer[0] = NAK_INVALID_ARG;
                    return NAK_FRAME_SIZE;
                }
            } else {
                Buffer[0] = NAK_CRC_ERROR;
                return NAK_FRAME_SIZE;
            }
        } else if (Cmd == CMD_COMPAT_WRITE) {
            /* The Mifare compatbility write command is a 2-frame command.
            * The first frame contains the page-address and the second frame
            * holds the data. */
            uint8_t PageAddress = Buffer[1];

            if (ISO14443ACheckCRCA(Buffer, CMD_COMPAT_WRITE_FRAME_SIZE)) {
                if (   (PageAddress >= PAGE_WRITE_MIN)
                    && (PageAddress <= PageWriteMax) ) {
                    /* CRC check passed and page-address is within bounds.
                    * Store address and proceed to receiving the data. */
                    CompatWritePageAddress = PageAddress;
                    State = STATE_COMPAT_WRITE;

                    Buffer[0] = ACK_VALUE;
                    return ACK_FRAME_SIZE;
                } else {
                    Buffer[0] = NAK_INVALID_ARG;
                    return NAK_FRAME_SIZE;
                }
            } else {
                Buffer[0] = NAK_CRC_ERROR;
                return NAK_FRAME_SIZE;
            }
        } else if (Cmd == CMD_HALT) {
            /* Halts the tag. According to the ISO14443, the second
            * byte is supposed to be 0. */
            if (Buffer[1] == 0) {
                if (ISO14443ACheckCRCA(Buffer, 2)) {
                    /* According to ISO14443, we must not send anything
                    * in order to acknowledge the HALT command. */
                    State = STATE_HALT;
                    return ISO14443A_APP_NO_RESPONSE;
                } else {
                    Buffer[0] = NAK_CRC_ERROR;
                    return NAK_FRAME_SIZE;
                }
            } else {
                Buffer[0] = NAK_INVALID_ARG;
                return NAK_FRAME_SIZE;
            }
        } else if (Cmd == CMD_GET_VERSION) {
            if (ISO14443ACheckCRCA(Buffer, 1)) {
                    Buffer[0] = 0x00;
                    Buffer[1] = 0x04;
                    Buffer[2] = 0x04;
                    Buffer[3] = 0x02;
                    Buffer[4] = 0x01;
                    Buffer[5] = 0x00;
                    Buffer[6] = VersionStorageSize;
                    Buffer[7] = 0x03;
                    ISO14443AAppendCRCA(Buffer, NTAG_VERSION_SIZE);
                    return (NTAG_VERSION_SIZE + ISO14443A_CRCA_SIZE) * 8;
                } else {
                    Buffer[0] = NAK_INVALID_ARG;
                    return NAK_FRAME_SIZE;
                }
        } else if (Cmd == CMD_PWD_AUTH) {
            if (ISO14443ACheckCRCA(Buffer, CMD_PWD_AUTH_FRAME_SIZE)) {
                /* CRC check passed */
				uint8_t pwBuffer[4];
				/* Uncomment the following line to always have positive authentication
				*  and store/steal the password at the password address*/
				//MemoryWriteBlock(&Buffer[1], PwdAddress, PWD_SIZE);
				MemoryReadBlock(&pwBuffer[0], PwdAddress, PWD_SIZE);
				int pwdOK = 1;
				for (int i=0; i<PWD_SIZE; i++) {
					if (Buffer[1+i] != pwBuffer[i]) {
						pwdOK = 0;
					}
				}
				if (pwdOK == 1){
					MemoryReadBlock(&Buffer[0], PackAddress, PACK_SIZE);
					/* 0x8080 response to positive auth: write 0x8080 in the PACK bytes 
					*  Uncomment the following lines to always get 0x8080 as the response */
					//Buffer[0] = 0x80;
					//Buffer[1] = 0x80;
					//MemoryWriteBlock(Buffer, PackAddress, PACK_SIZE);	
					/* End 0x8080 response stuff */				
					ISO14443AAppendCRCA(Buffer, PACK_SIZE);
                    return (PACK_SIZE + ISO14443A_CRCA_SIZE) * 8;
				} else {
					Buffer[0] = NAK_AUTH;
					return NAK_FRAME_SIZE;
				}
            } else {
                Buffer[0] = NAK_CRC_ERROR;
                return NAK_FRAME_SIZE;
            }

        } else {
            /* Unknown command. Enter halt state */
            State = STATE_IDLE;
        }
    break;

    case STATE_COMPAT_WRITE:
        /* Compatibility write. Receiving 16 bytes of data of which 4 bytes are valid. */
        if (ISO14443ACheckCRCA(Buffer, BYTES_PER_COMPAT_WRITE)) {
            /* We don't perform any checks here. You will be able to program the
            * whole memory. Also there is no OTP behaviour. */
            if (!ActiveConfiguration.ReadOnly) {
                MemoryWriteBlock(Buffer, CompatWritePageAddress * BYTES_PER_PAGE, BYTES_PER_WRITE);
            } else {
                /* If we are told to be read only, we silently ignore the write command
                * and pretend to have written data. */
            }

            State = STATE_ACTIVE;
            Buffer[0] = ACK_VALUE;
            return ACK_FRAME_SIZE;
        } else {
            State = STATE_ACTIVE;
            Buffer[0] = NAK_CRC_ERROR;
            return NAK_FRAME_SIZE;
        }

    default:
        /* Unknown state? Should never happen. */
        break;
    }

    /* No response has been sent, when we reach here */
    return ISO14443A_APP_NO_RESPONSE;
}

void Ntag21xGetUid(ConfigurationUidType Uid)
{
    /* Read UID from memory */
    MemoryReadBlock(&Uid[0], UID_CL1_ADDRESS, UID_CL1_SIZE);
    MemoryReadBlock(&Uid[UID_CL1_SIZE], UID_CL2_ADDRESS, UID_CL2_SIZE);
}

void Ntag21xSetUid(ConfigurationUidType Uid)
{
    /* Calculate check bytes and write everything into memory */
    uint8_t BCC1 = ISO14443A_UID0_CT ^ Uid[0] ^ Uid[1] ^ Uid[2];
    uint8_t BCC2 = Uid[3] ^ Uid[4] ^ Uid[5] ^ Uid[6];

    MemoryWriteBlock(&Uid[0], UID_CL1_ADDRESS, UID_CL1_SIZE);
    MemoryWriteBlock(&BCC1, UID_BCC1_ADDRESS, ISO14443A_CL_BCC_SIZE);
    MemoryWriteBlock(&Uid[UID_CL1_SIZE], UID_CL2_ADDRESS, UID_CL2_SIZE);
    MemoryWriteBlock(&BCC2, UID_BCC2_ADDRESS, ISO14443A_CL_BCC_SIZE);
}

