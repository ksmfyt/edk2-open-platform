/*******************************************************************************
Copyright (C) 2018 Marvell International Ltd.

Marvell BSD License Option

If you received this File from Marvell, you may opt to use, redistribute and/or
modify this File under the following licensing terms.
Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of Marvell nor the names of its contributors may be
  used to endorse or promote products derived from this software without
  specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#include "XenonSdMmcOverride.h"

STATIC EFI_HANDLE              mXenonSdMmcOverrideHandle;
STATIC EDKII_SD_MMC_OVERRIDE  *mSdMmcOverride;

STATIC
EFI_STATUS
EFIAPI
GetSdMmcDesc (
  IN      EFI_HANDLE              ControllerHandle,
  IN  OUT MV_BOARD_SDMMC_DESC     *SdMmcDesc
  )
{
  EFI_STATUS                      Status;
  MV_BOARD_SDMMC_DESC             *SdMmcDescs;
  NON_DISCOVERABLE_DEVICE         *Device;
  MARVELL_BOARD_DESC_PROTOCOL     *BoardDescProtocol;
  UINTN                           Index;

  Device = NULL;
  Status = gBS->OpenProtocol (ControllerHandle,
                  &gEdkiiNonDiscoverableDeviceProtocolGuid,
                  (VOID **) &Device,
                  mXenonSdMmcOverrideHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  BoardDescProtocol = NULL;
  Status = gBS->LocateProtocol (&gMarvellBoardDescProtocolGuid,
                  NULL,
                  (VOID **) &BoardDescProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = BoardDescProtocol->BoardDescSdMmcGet (BoardDescProtocol, &SdMmcDescs);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < SdMmcDescs->SdMmcDevCount; Index++) {
    if (SdMmcDescs[Index].SoC->SdMmcBaseAddress ==
        Device->Resources[0].AddrRangeMin) {
      *SdMmcDesc = SdMmcDescs[Index];
      break;
    }
  }

  if (Index == SdMmcDescs->SdMmcDevCount) {
    BoardDescProtocol->BoardDescFree (SdMmcDescs);
    return EFI_INVALID_PARAMETER;
  }

  BoardDescProtocol->BoardDescFree (SdMmcDescs);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetPciIo (
  IN      EFI_HANDLE              ControllerHandle,
  IN  OUT EFI_PCI_IO_PROTOCOL     **PciIo
  )
{
  EFI_STATUS Status;

  *PciIo  = NULL;
  Status = gBS->OpenProtocol (ControllerHandle,
                  &gEfiPciIoProtocolGuid,
                  (VOID **) PciIo,
                  mXenonSdMmcOverrideHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  return Status;
}

/**
  Set SD Host Controler control 2 registry according to selected speed.

  @param[in] ControllerHandle The EFI_HANDLE of the controller.
  @param[in] Slot             The slot number of the SD card to send the command to.
  @param[in] Timing           The timing to select.

  @retval EFI_SUCCESS         The override function completed successfully.
  @retval EFI_NOT_FOUND       The specified controller or slot does not exist.
**/
STATIC
EFI_STATUS
XenonSdMmcHcUhsSignaling (
  IN EFI_HANDLE             ControllerHandle,
  IN UINT8                  Slot,
  IN SD_MMC_UHS_TIMING      Timing
  )
{
  EFI_PCI_IO_PROTOCOL      *PciIo;
  EFI_STATUS                Status;
  UINT8                     HostCtrl2;

  if (Slot != 0) {
    return EFI_NOT_FOUND;
  }

  Status = GetPciIo (ControllerHandle, &PciIo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  HostCtrl2 = (UINT8)~SD_MMC_HC_CTRL_UHS_MASK;
  Status = XenonHcAndMmio (PciIo,
             Slot,
             SD_MMC_HC_HOST_CTRL2,
             sizeof (HostCtrl2),
             &HostCtrl2);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  switch (Timing) {
    case SdMmcUhsSdr12:
      HostCtrl2 = SD_MMC_HC_CTRL_UHS_SDR12;
      break;
    case SdMmcUhsSdr25:
      HostCtrl2 = SD_MMC_HC_CTRL_UHS_SDR25;
      break;
    case SdMmcUhsSdr50:
      HostCtrl2 = SD_MMC_HC_CTRL_UHS_SDR50;
      break;
    case SdMmcUhsSdr104:
      HostCtrl2 = SD_MMC_HC_CTRL_UHS_SDR104;
      break;
    case SdMmcUhsDdr50:
      HostCtrl2 = SD_MMC_HC_CTRL_UHS_DDR50;
      break;
    case SdMmcMmcDdr52:
      HostCtrl2 = SD_MMC_HC_CTRL_MMC_DDR52;
      break;
    case SdMmcMmcSdr50:
      HostCtrl2 = SD_MMC_HC_CTRL_MMC_SDR50;
      break;
    case SdMmcMmcSdr25:
      HostCtrl2 = SD_MMC_HC_CTRL_MMC_SDR25;
      break;
    case SdMmcMmcSdr12:
      HostCtrl2 = SD_MMC_HC_CTRL_MMC_SDR12;
      break;
    case SdMmcMmcHs200:
      HostCtrl2 = XENON_SD_MMC_HC_CTRL_HS200;
      break;
    case SdMmcMmcHs400:
      HostCtrl2 = XENON_SD_MMC_HC_CTRL_HS400;
      break;
    default:
     HostCtrl2 = 0;
     break;
  }
  Status = XenonHcOrMmio (PciIo,
             Slot,
             SD_MMC_HC_HOST_CTRL2,
             sizeof (HostCtrl2),
             &HostCtrl2);

  return Status;
}

/**

  Override function for SDHCI capability bits

  @param[in]      ControllerHandle      The EFI_HANDLE of the controller.
  @param[in]      Slot                  The 0 based slot index.
  @param[in,out]  SdMmcHcSlotCapability The SDHCI capability structure.

  @retval EFI_SUCCESS           The override function completed successfully.
  @retval EFI_NOT_FOUND         The specified controller or slot does not exist.
  @retval EFI_INVALID_PARAMETER SdMmcHcSlotCapability is NULL

**/
STATIC
EFI_STATUS
EFIAPI
XenonSdMmcCapability (
  IN      EFI_HANDLE                      ControllerHandle,
  IN      UINT8                           Slot,
  IN  OUT VOID                            *SdMmcHcSlotCapability
  )
{
  EFI_STATUS                      Status;
  MV_BOARD_SDMMC_DESC             SdMmcDesc;
  SD_MMC_HC_SLOT_CAP              *Capability;

  if (SdMmcHcSlotCapability == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  if (Slot != 0) {
    return EFI_NOT_FOUND;
  }
  Capability = (SD_MMC_HC_SLOT_CAP *)SdMmcHcSlotCapability;

  Status = GetSdMmcDesc (ControllerHandle, &SdMmcDesc);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Override capabilities structure according to board configuration.
  //
  if (SdMmcDesc.Xenon1v8Enabled) {
    Capability->Voltage33 = 0;
    Capability->Voltage30 = 0;
  } else {
    Capability->Voltage18 = 0;
    Capability->Sdr104 = 0;
    Capability->Ddr50 = 0;
    Capability->Sdr50 = 0;
  }

  if (!SdMmcDesc.Xenon8BitBusEnabled) {
    Capability->BusWidth8 = 0;
  }

  if (SdMmcDesc.XenonSlowModeEnabled) {
    Capability->Sdr104 = 0;
    Capability->Ddr50 = 0;
  }

  Capability->SlotType = SdMmcDesc.SlotType;

  return EFI_SUCCESS;
}

/**

  Override function for SDHCI controller operations

  @param[in]      ControllerHandle      The EFI_HANDLE of the controller.
  @param[in]      Slot                  The 0 based slot index.
  @param[in]      PhaseType             The type of operation and whether the
                                        hook is invoked right before (pre) or
                                        right after (post)
  @param[in]      Data                  A pointer to the Data which is specific
                                        for the PhaseType

  @retval EFI_SUCCESS           The override function completed successfully.
  @retval EFI_NOT_FOUND         The specified controller or slot does not exist.
  @retval EFI_UNSUPPORTED       Nothing has been done in connection of PhaseType
  @retval EFI_INVALID_PARAMETER PhaseType is invalid

**/
STATIC
EFI_STATUS
EFIAPI
XenonSdMmcNotifyPhase (
  IN      EFI_HANDLE                      ControllerHandle,
  IN      UINT8                           Slot,
  IN      EDKII_SD_MMC_PHASE_TYPE         PhaseType
  )
{
  EFI_STATUS                      Status;
  MV_BOARD_SDMMC_DESC             SdMmcDesc;
  EFI_PCI_IO_PROTOCOL             *PciIo;

  if (Slot != 0) {
    return EFI_NOT_FOUND;
  }

  switch (PhaseType) {
    case EdkiiSdMmcInitHostPre:
      Status = GetPciIo (ControllerHandle, &PciIo);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Status = GetSdMmcDesc (ControllerHandle, &SdMmcDesc);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Status = XenonInit (PciIo,
                 SdMmcDesc.Xenon1v8Enabled,
                 SdMmcDesc.XenonSlowModeEnabled,
                 SdMmcDesc.XenonTuningStepDivisor);
      return Status;
    default:
      return EFI_SUCCESS;
  }

  return EFI_SUCCESS;
}

/**

  Additional operations specific for host controller

  @param[in]      ControllerHandle      The EFI_HANDLE of the controller.
  @param[in]      Slot                  The 0 based slot index.
  @param[in]      Timing                The timing which should be set by
                                        host controller.

  @retval EFI_SUCCESS           The override function completed successfully.
  @retval EFI_NOT_FOUND         The specified controller or slot does not exist.

**/
EFI_STATUS
EFIAPI
XenonSwitchClockFreqPost (
  IN      EFI_HANDLE                      ControllerHandle,
  IN      UINT8                           Slot,
  IN      SD_MMC_UHS_TIMING               Timing
  )
{
  EFI_STATUS                      Status;
  MV_BOARD_SDMMC_DESC             SdMmcDesc;
  EFI_PCI_IO_PROTOCOL             *PciIo;

  if (Slot != 0) {
    return EFI_NOT_FOUND;
  }

  Status = GetPciIo (ControllerHandle, &PciIo);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Status = GetSdMmcDesc (ControllerHandle, &SdMmcDesc);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = XenonSetPhy (PciIo,
             SdMmcDesc.XenonSlowModeEnabled,
             SdMmcDesc.XenonTuningStepDivisor,
             Timing);

  return Status;
}

/**

  Callback that allow to override base clock frequency with value
  higher then 255

  @param[in]      ControllerHandle      The EFI_HANDLE of the controller.
  @param[in]      Slot                  The 0 based slot index.
  @param[in,out]  BaseClkFreq           The base clock frequency that can be
                                        overriden with greater value then 255.

  @retval EFI_SUCCESS           The override function completed successfully.
  @retval EFI_NOT_FOUND         The specified controller or slot does not exist.

**/
EFI_STATUS
EFIAPI
XenonBaseClkFreq (
  IN      EFI_HANDLE                      ControllerHandle,
  IN      UINT8                           Slot,
  IN  OUT UINT32                          *BaseClkFreq
  )
{
  if (Slot != 0) {
    return EFI_NOT_FOUND;
  }
  //
  // Override inappropriate base clock frequency from Capabilities Register 1.
  // Actual clock speed of Xenon controller is 400MHz.
  //
  *BaseClkFreq = XENON_MMC_MAX_CLK / 1000 / 1000;

  return EFI_SUCCESS;
}

/**
  The entry point for Xenon driver, used to install SdMMcOverrideProtocol
  on the ImageHandle.

  @param[in]  ImageHandle   The firmware allocated handle for this driver image.
  @param[in]  SystemTable   Pointer to the EFI system table.

  @retval EFI_SUCCESS   Driver loaded.
  @retval other         Driver not loaded.

**/
EFI_STATUS
EFIAPI
InitializeXenonDxe (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;

  mSdMmcOverride = AllocateZeroPool (sizeof (EDKII_SD_MMC_OVERRIDE));
  if (mSdMmcOverride == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Cannot allocate memory\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  mSdMmcOverride->Version = EDKII_SD_MMC_OVERRIDE_PROTOCOL_VERSION;
  mSdMmcOverride->Capability = XenonSdMmcCapability;
  mSdMmcOverride->NotifyPhase = XenonSdMmcNotifyPhase;
  mSdMmcOverride->UhsSignaling = XenonSdMmcHcUhsSignaling;
  mSdMmcOverride->SwitchClockFreqPost = XenonSwitchClockFreqPost;
  mSdMmcOverride->BaseClockFreq = XenonBaseClkFreq;

  Status = gBS->InstallProtocolInterface (&ImageHandle,
                  &gEdkiiSdMmcOverrideProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  mSdMmcOverride);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR,
      "%a: Filed to install SdMmcOverride protocol\n",
      __FUNCTION__));
    return Status;
  }

  mXenonSdMmcOverrideHandle = ImageHandle;

  return Status;
}
