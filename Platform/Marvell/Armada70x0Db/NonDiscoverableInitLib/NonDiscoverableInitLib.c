/**
*
*  Copyright (c) 2017, Linaro Ltd. All rights reserved.
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

#include <Uefi.h>

#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NonDiscoverableDeviceRegistrationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/MvGpio.h>
#include <Protocol/NonDiscoverableDevice.h>

#include "NonDiscoverableInitLib.h"

STATIC CONST MV_GPIO_PIN mXhciVbusPins[] = {
  {
    ARMADA_70x0_DB_IO_EXPANDER0,
    ARMADA_70x0_DB_VBUS0_PIN,
    TRUE,
  },
  {
    ARMADA_70x0_DB_IO_EXPANDER0,
    ARMADA_70x0_DB_VBUS0_LIMIT_PIN,
    TRUE,
  },
  {
    ARMADA_70x0_DB_IO_EXPANDER0,
    ARMADA_70x0_DB_VBUS1_PIN,
    TRUE,
  },
  {
    ARMADA_70x0_DB_IO_EXPANDER0,
    ARMADA_70x0_DB_VBUS1_LIMIT_PIN,
    TRUE,
  },
};

STATIC
EFI_STATUS
EFIAPI
XhciInit (
  IN  NON_DISCOVERABLE_DEVICE  *This
  )
{
  CONST MV_GPIO_PIN   *VbusPin;
  EMBEDDED_GPIO_MODE   Mode;
  EMBEDDED_GPIO_PIN    Gpio;
  EMBEDDED_GPIO       *GpioProtocol;
  EFI_STATUS           Status;
  UINTN                Index;

  Status = MvGpioGetProtocol (MV_GPIO_DRIVER_TYPE_PCA95XX, &GpioProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to find GPIO protocol\n", __FUNCTION__));
    return Status;
  }

  VbusPin = mXhciVbusPins;
  for (Index = 0; Index < ARRAY_SIZE (mXhciVbusPins); Index++) {
    Mode = VbusPin->ActiveHigh ? GPIO_MODE_OUTPUT_1 : GPIO_MODE_OUTPUT_0;
    Gpio = GPIO (VbusPin->ControllerId, VbusPin->PinNumber);
    GpioProtocol->Set (GpioProtocol, Gpio, Mode);
    VbusPin++;
  }

  return EFI_SUCCESS;
}

NON_DISCOVERABLE_DEVICE_INIT
EFIAPI
NonDiscoverableDeviceInitializerGet (
  IN  NON_DISCOVERABLE_DEVICE_TYPE  Type,
  IN  UINTN                         Index
  )
{
  if (Type == NonDiscoverableDeviceTypeXhci) {
        return XhciInit;
  }

  return NULL;
}
