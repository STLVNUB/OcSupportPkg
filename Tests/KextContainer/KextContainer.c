/** @file
  Test kernel patch support.

Copyright (c) 2018, vit9696. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Uefi.h>
#include <PiDxe.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/OcDevicePropertyLib.h>
#include <Library/DevicePathLib.h>
#include <Library/OcMiscLib.h>
#include <Library/OcProtocolLib.h>
#include <Library/OcAppleBootPolicyLib.h>
#include <Library/OcSmbiosLib.h>
#include <Library/OcCpuLib.h>
#include <Library/OcStringLib.h>
#include <Library/OcVirtualFsLib.h>
#include <Library/OcAppleKernelLib.h>
#include <Library/OcFileLib.h>
#include <Library/OcMachoLib.h>
#include <Library/OcXmlLib.h>

#include <Protocol/AppleBootPolicy.h>
#include <Protocol/DevicePathPropertyDatabase.h>

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleTextInEx.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/OcKextContainer.h>

STATIC OC_KEXT_DESCRIPTOR KextDescriptors[32];
STATIC UINTN KextsCount = 0;

EFI_STATUS
EFIAPI AddKext (
  IN OC_KEXT_CONTAINER_PROTOCOL  *This,
  IN OC_KEXT_DESCRIPTOR *KextDescriptor
  ) {
  if (KextsCount >= ARRAY_SIZE(KextDescriptors)) {
    return EFI_BUFFER_TOO_SMALL;
  }

  KextDescriptors[KextsCount++] = *KextDescriptor;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI GetKexts (
  IN OC_KEXT_CONTAINER_PROTOCOL  *This,
  OUT OC_KEXT_DESCRIPTOR **Descriptors,
  OUT UINTN *Count
  ) {
  *Descriptors = KextDescriptors;
  *Count = KextsCount;
  return EFI_SUCCESS;
}

STATIC OC_KEXT_CONTAINER_PROTOCOL ProtocolInstance = {
  .AddKext = AddKext,
  .GetKexts = GetKexts
};

EFI_STATUS
EFIAPI
InstallProtocol (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_HANDLE Handle = NULL;
  return gBS->InstallProtocolInterface(
    &Handle,
    &gOcKextContainerProtocolGuid,
    EFI_NATIVE_INTERFACE,
    &ProtocolInstance
    );
}
