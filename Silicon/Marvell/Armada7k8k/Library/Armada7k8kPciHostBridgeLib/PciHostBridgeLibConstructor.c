/** @file
  PCI Host Bridge Library instance for Marvell 70x0/80x0

  Copyright (c) 2017, Linaro Ltd. All rights reserved.<BR>

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>
#include <IndustryStandard/Pci22.h>
#include <Library/ArmLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>

#define GPIO_BASE                 FixedPcdGet64 (PcdChip1MppBaseAddress) + 0x100
#define GPIO_DIR_OFFSET(n)        (((n) >> 5) * 0x40)
#define GPIO_ENABLE_OFFSET(n)     (((n) >> 5) * 0x40 + 0x4)

#define GPIO_PIN_MASK(n)          (1 << ((n) & 0x1f))

#define PCIE_SLOT_RESET_GPIO      52

#define MV_PCIE_BASE                                        0xF2600000

#define IATU_VIEWPORT_OFF                                   0x900
#define IATU_VIEWPORT_INBOUND                               BIT31
#define IATU_VIEWPORT_OUTBOUND                              0
#define IATU_VIEWPORT_REGION_INDEX(Idx)                     ((Idx) & 7)

#define IATU_REGION_CTRL_1_OFF_OUTBOUND_0                   0x904
#define IATU_REGION_CTRL_1_OFF_OUTBOUND_0_TYPE_MEM          0x0
#define IATU_REGION_CTRL_1_OFF_OUTBOUND_0_TYPE_IO           0x2
#define IATU_REGION_CTRL_1_OFF_OUTBOUND_0_TYPE_CFG0         0x4
#define IATU_REGION_CTRL_1_OFF_OUTBOUND_0_TYPE_CFG1         0x5

#define IATU_REGION_CTRL_2_OFF_OUTBOUND_0                   0x908
#define IATU_REGION_CTRL_2_OFF_OUTBOUND_0_REGION_EN         BIT31
#define IATU_REGION_CTRL_2_OFF_OUTBOUND_0_CFG_SHIFT_MODE    BIT28

#define IATU_LWR_BASE_ADDR_OFF_OUTBOUND_0                   0x90C
#define IATU_UPPER_BASE_ADDR_OFF_OUTBOUND_0                 0x910
#define IATU_LIMIT_ADDR_OFF_OUTBOUND_0                      0x914
#define IATU_LWR_TARGET_ADDR_OFF_OUTBOUND_0                 0x918
#define IATU_UPPER_TARGET_ADDR_OFF_OUTBOUND_0               0x91C

#define PORT_LINK_CTRL_OFF                                  0x710
#define PORT_LINK_CTRL_OFF_LINK_CAPABLE_x1                  (0x01 << 16)
#define PORT_LINK_CTRL_OFF_LINK_CAPABLE_x2                  (0x03 << 16)
#define PORT_LINK_CTRL_OFF_LINK_CAPABLE_x4                  (0x07 << 16)
#define PORT_LINK_CTRL_OFF_LINK_CAPABLE_x8                  (0x0f << 16)
#define PORT_LINK_CTRL_OFF_LINK_CAPABLE_x16                 (0x1f << 16)
#define PORT_LINK_CTRL_OFF_LINK_CAPABLE_MASK                (0x3f << 16)

#define GEN2_CTRL_OFF                                       0x80c
#define GEN2_CTRL_OFF_NUM_OF_LANES(n)                       (((n) & 0x1f) << 8)
#define GEN2_CTRL_OFF_NUM_OF_LANES_MASK                     (0x1f << 8)
#define GEN2_CTRL_OFF_DIRECT_SPEED_CHANGE                   BIT17

#define PCIE_GLOBAL_CTRL_OFFSET                             0x8000
#define PCIE_GLOBAL_APP_LTSSM_EN                            BIT2
#define PCIE_GLOBAL_CTRL_DEVICE_TYPE_RC                     (0x4 << 4)
#define PCIE_GLOBAL_CTRL_DEVICE_TYPE_MASK                   (0xF << 4)

#define PCIE_GLOBAL_STATUS_REG                              0x8008
#define PCIE_GLOBAL_STATUS_RDLH_LINK_UP                     BIT1
#define PCIE_GLOBAL_STATUS_PHY_LINK_UP                      BIT9

#define PCIE_PM_STATUS                                      0x8014
#define PCIE_PM_LTSSM_STAT_MASK                             (0x3f << 3)

#define PCIE_GLOBAL_INT_MASK1_REG                           0x8020
#define PCIE_INT_A_ASSERT_MASK                              BIT9
#define PCIE_INT_B_ASSERT_MASK                              BIT10
#define PCIE_INT_C_ASSERT_MASK                              BIT11
#define PCIE_INT_D_ASSERT_MASK                              BIT12

#define PCIE_ARCACHE_TRC_REG                                0x8050
#define PCIE_AWCACHE_TRC_REG                                0x8054
#define PCIE_ARUSER_REG                                     0x805C
#define PCIE_AWUSER_REG                                     0x8060

#define ARCACHE_DEFAULT_VALUE                               0x3511
#define AWCACHE_DEFAULT_VALUE                               0x5311

#define AX_USER_DOMAIN_INNER_SHAREABLE                      (0x1 << 4)
#define AX_USER_DOMAIN_OUTER_SHAREABLE                      (0x2 << 4)
#define AX_USER_DOMAIN_MASK                                 (0x3 << 4)

#define PCIE_LINK_CAPABILITY                                0x7C
#define PCIE_LINK_CTL_2                                     0xA0
#define TARGET_LINK_SPEED_MASK                              0xF
#define LINK_SPEED_GEN_1                                    0x1
#define LINK_SPEED_GEN_2                                    0x2
#define LINK_SPEED_GEN_3                                    0x3

#define PCIE_GEN3_EQU_CTRL                                  0x8A8
#define GEN3_EQU_EVAL_2MS_DISABLE                           BIT5

#define PCIE_LINK_UP_TIMEOUT_US                             (40000)

STATIC
VOID
ConfigureWindow (
  UINTN     Index,
  UINT64    CpuBase,
  UINT64    PciBase,
  UINT64    Size,
  UINTN     Type,
  UINTN     EnableFlags
  )
{
  ArmDataMemoryBarrier ();

  MmioWrite32 (MV_PCIE_BASE + IATU_VIEWPORT_OFF,
               IATU_VIEWPORT_OUTBOUND | IATU_VIEWPORT_REGION_INDEX (Index));

  ArmDataMemoryBarrier ();

  MmioWrite32 (MV_PCIE_BASE + IATU_LWR_BASE_ADDR_OFF_OUTBOUND_0,
               (UINT32)(CpuBase & 0xFFFFFFFF));
  MmioWrite32 (MV_PCIE_BASE + IATU_UPPER_BASE_ADDR_OFF_OUTBOUND_0,
               (UINT32)(CpuBase >> 32));
  MmioWrite32 (MV_PCIE_BASE + IATU_LIMIT_ADDR_OFF_OUTBOUND_0,
               (UINT32)(CpuBase + Size - 1));
  MmioWrite32 (MV_PCIE_BASE + IATU_LWR_TARGET_ADDR_OFF_OUTBOUND_0,
               (UINT32)(PciBase & 0xFFFFFFFF));
  MmioWrite32 (MV_PCIE_BASE + IATU_UPPER_TARGET_ADDR_OFF_OUTBOUND_0,
               (UINT32)(PciBase >> 32));
  MmioWrite32 (MV_PCIE_BASE + IATU_REGION_CTRL_1_OFF_OUTBOUND_0,
               Type);
  MmioWrite32 (MV_PCIE_BASE + IATU_REGION_CTRL_2_OFF_OUTBOUND_0,
               IATU_REGION_CTRL_2_OFF_OUTBOUND_0_REGION_EN | EnableFlags);
}

STATIC
VOID
WaitForLink (
  VOID
  )
{
  UINT32 Mask;
  UINT32 Status;
  UINT32 Timeout;

  if (!(MmioRead32 (MV_PCIE_BASE + PCIE_PM_STATUS) & PCIE_PM_LTSSM_STAT_MASK)) {
    DEBUG ((DEBUG_INIT, "%a: no PCIe device detected\n", __FUNCTION__));
    return;
  }

  //
  // Wait for the link to establish itself
  //
  DEBUG ((DEBUG_INIT, "%a: waiting for PCIe link\n", __FUNCTION__));

  Mask = PCIE_GLOBAL_STATUS_RDLH_LINK_UP | PCIE_GLOBAL_STATUS_PHY_LINK_UP;
  Timeout = PCIE_LINK_UP_TIMEOUT_US / 10;
  do {
    Status = MmioRead32 (MV_PCIE_BASE + PCIE_GLOBAL_STATUS_REG);
    if ((Status & Mask) == Mask) {
      break;
    }
    gBS->Stall (10);
  } while (Timeout--);
}

EFI_STATUS
EFIAPI
Armada7k8kPciHostBridgeLibConstructor (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  ASSERT (FixedPcdGet32 (PcdPciBusMin) == 0);
  ASSERT (FixedPcdGet64 (PcdPciExpressBaseAddress) % SIZE_256MB == 0);

  //
  // Reset the slot
  //
  MmioOr32 (GPIO_BASE + GPIO_DIR_OFFSET (PCIE_SLOT_RESET_GPIO),
            GPIO_PIN_MASK (PCIE_SLOT_RESET_GPIO));
  ArmDataMemoryBarrier ();
  gBS->Stall (10 * 1000);

  MmioAnd32 (GPIO_BASE + GPIO_ENABLE_OFFSET (PCIE_SLOT_RESET_GPIO),
             ~GPIO_PIN_MASK (PCIE_SLOT_RESET_GPIO));
  ArmDataMemoryBarrier ();
  gBS->Stall (20 * 1000);

  MmioAndThenOr32 (MV_PCIE_BASE + PORT_LINK_CTRL_OFF,
                   ~PORT_LINK_CTRL_OFF_LINK_CAPABLE_MASK,
                   PORT_LINK_CTRL_OFF_LINK_CAPABLE_x4);

  MmioAndThenOr32 (MV_PCIE_BASE + GEN2_CTRL_OFF,
                   ~GEN2_CTRL_OFF_NUM_OF_LANES_MASK,
                   GEN2_CTRL_OFF_NUM_OF_LANES(4) |
                   GEN2_CTRL_OFF_DIRECT_SPEED_CHANGE);

  MmioAndThenOr32 (MV_PCIE_BASE + PCIE_GLOBAL_CTRL_OFFSET,
                   ~(PCIE_GLOBAL_CTRL_DEVICE_TYPE_MASK |
                     PCIE_GLOBAL_APP_LTSSM_EN),
                   PCIE_GLOBAL_CTRL_DEVICE_TYPE_RC);

  MmioWrite32 (MV_PCIE_BASE + PCIE_ARCACHE_TRC_REG,
               ARCACHE_DEFAULT_VALUE);

  MmioWrite32 (MV_PCIE_BASE + PCIE_AWCACHE_TRC_REG,
               AWCACHE_DEFAULT_VALUE);

  MmioAndThenOr32 (MV_PCIE_BASE + PCIE_ARUSER_REG,
                   ~AX_USER_DOMAIN_MASK,
                   AX_USER_DOMAIN_OUTER_SHAREABLE);

  MmioAndThenOr32 (MV_PCIE_BASE + PCIE_AWUSER_REG,
                   ~AX_USER_DOMAIN_MASK,
                   AX_USER_DOMAIN_OUTER_SHAREABLE);

  MmioAndThenOr32 (MV_PCIE_BASE + PCIE_LINK_CTL_2,
                   ~TARGET_LINK_SPEED_MASK,
                   LINK_SPEED_GEN_3);

  MmioAndThenOr32 (MV_PCIE_BASE + PCIE_LINK_CAPABILITY,
                   ~TARGET_LINK_SPEED_MASK,
                   LINK_SPEED_GEN_3);

  MmioOr32 (MV_PCIE_BASE + PCIE_GEN3_EQU_CTRL,
            GEN3_EQU_EVAL_2MS_DISABLE);

  MmioOr32 (MV_PCIE_BASE + PCIE_GLOBAL_CTRL_OFFSET,
            PCIE_GLOBAL_APP_LTSSM_EN);

  // Region 0: MMIO32 range
  ConfigureWindow (0,
                   FixedPcdGet32 (PcdPciMmio32Base),
                   FixedPcdGet32 (PcdPciMmio32Base),
                   FixedPcdGet32 (PcdPciMmio32Size),
                   IATU_REGION_CTRL_1_OFF_OUTBOUND_0_TYPE_MEM,
                   0);

  // Region 1: Type 0 config space
  ConfigureWindow (1,
                   FixedPcdGet64 (PcdPciExpressBaseAddress),
                   0x0,
                   SIZE_64KB,
                   IATU_REGION_CTRL_1_OFF_OUTBOUND_0_TYPE_CFG0,
                   IATU_REGION_CTRL_2_OFF_OUTBOUND_0_CFG_SHIFT_MODE);

  // Region 2: Type 1 config space
  ConfigureWindow (2,
                   FixedPcdGet64 (PcdPciExpressBaseAddress) + SIZE_64KB,
                   0x0,
                   FixedPcdGet32 (PcdPciBusMax) * SIZE_1MB,
                   IATU_REGION_CTRL_1_OFF_OUTBOUND_0_TYPE_CFG1,
                   IATU_REGION_CTRL_2_OFF_OUTBOUND_0_CFG_SHIFT_MODE);

  // Region 3: port I/O range
  ConfigureWindow (3,
                   FixedPcdGet64 (PcdPciIoTranslation),
                   FixedPcdGet32 (PcdPciIoBase),
                   FixedPcdGet32 (PcdPciIoSize),
                   IATU_REGION_CTRL_1_OFF_OUTBOUND_0_TYPE_IO,
                   0);

  // Region 4: MMIO64 range
  ConfigureWindow (4,
                   FixedPcdGet64 (PcdPciMmio64Base),
                   FixedPcdGet64 (PcdPciMmio64Base),
                   FixedPcdGet64 (PcdPciMmio64Size),
                   IATU_REGION_CTRL_1_OFF_OUTBOUND_0_TYPE_MEM,
                   0);

  MmioOr32 (MV_PCIE_BASE + PCIE_GLOBAL_INT_MASK1_REG,
            PCIE_INT_A_ASSERT_MASK |
            PCIE_INT_B_ASSERT_MASK |
            PCIE_INT_C_ASSERT_MASK |
            PCIE_INT_D_ASSERT_MASK);

  WaitForLink ();

  //
  // Enable the RC
  //
  MmioOr32 (MV_PCIE_BASE + PCI_COMMAND_OFFSET,
            EFI_PCI_COMMAND_IO_SPACE |
            EFI_PCI_COMMAND_MEMORY_SPACE |
            EFI_PCI_COMMAND_BUS_MASTER);

  return EFI_SUCCESS;
}
