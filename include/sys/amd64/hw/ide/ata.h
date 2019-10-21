#pragma once

#define ATA_CMD_IDENTIFY        0xEC
#define ATA_CMD_READ_DMA_EX     0x25

#define ATA_SR_BUSY             (1 << 7)
#define ATA_SR_DRQ              (1 << 3)

#define ATA_IDENT_DEVICE_TYPE   0x00
#define ATA_IDENT_CYLINDERS     0x02
#define ATA_IDENT_HEADS         0x06
#define ATA_IDENT_SECTORS       0x0C
#define ATA_IDENT_SERIAL        0x14
#define ATA_IDENT_MODEL         0x36
#define ATA_IDENT_CAPS          0x62
#define ATA_IDENT_MAX_LBA       0x78
#define ATA_IDENT_CMD_SETS      0xA4
#define ATA_IDENT_MAX_LBAEXT    0xC8
