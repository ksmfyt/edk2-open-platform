/**
*
*  Copyright (c) 2018, Marvell International Ltd. All rights reserved.
*
*  This program and the accompanying materials are licensed and made available
*  under the terms and conditions of the BSD License which accompanies this
*  distribution. The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/
#ifndef __MV_GPIO_H__
#define __MV_GPIO_H__


#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/BoardDesc.h>
#include <Protocol/MvGpio.h>

#include <Uefi/UefiBaseType.h>

#define GPIO_SIGNATURE                   SIGNATURE_32 ('G', 'P', 'I', 'O')

#ifndef BIT
#define BIT(nr)                          (1 << (nr))
#endif

// Marvell GPIO Controller Registers
#define GPIO_DATA_OUT_REG                (0x0)
#define GPIO_OUT_EN_REG                  (0x4)
#define GPIO_BLINK_EN_REG                (0x8)
#define GPIO_DATA_IN_POL_REG             (0xc)
#define GPIO_DATA_IN_REG                 (0x10)

typedef struct {
  MARVELL_GPIO_PROTOCOL   GpioProtocol;
  MV_BOARD_GPIO_DESC      *Desc;
  UINTN                   Signature;
  EFI_HANDLE              Handle;
} MV_GPIO;

#endif // __MV_GPIO_H__
