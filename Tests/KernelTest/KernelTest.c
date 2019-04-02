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
#include <Library/OcCompressionLib.h>
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

#if 0

STATIC
UINT8
DisableAppleHDAPatchReplace[] = {
  0x31, 0xC0, 0xC3 // xor eax, eax ; ret
};

STATIC
PATCHER_GENERIC_PATCH
DisableAppleHDAPatch = {
  .Base    = "__ZN20AppleHDACodecGeneric5probeEP9IOServicePi",
  .Find    = NULL,
  .Mask    = NULL,
  .Replace = DisableAppleHDAPatchReplace,
  .ReplaceMask = NULL,
  .Size    = sizeof (DisableAppleHDAPatchReplace),
  .Count   = 1,
  .Skip    = 0
};

STATIC
UINT8
DisableKernelLog[] = {
  0xC3
};

STATIC
PATCHER_GENERIC_PATCH
KernelPatch = {
  .Base    = "_IOLog",
  .Find    = NULL,
  .Mask    = NULL,
  .Replace = DisableKernelLog,
  .Size    = sizeof (DisableKernelLog),
  .Count   = 1,
  .Skip    = 0
};

#endif

STATIC
VOID
ApplyKextPatches (
  PRELINKED_CONTEXT  *Context
  )
{
  PatchAppleIntelCPUPowerManagement (Context);

  PatchUsbXhciPortLimit (Context);

  PatchThirdPartySsdTrim (Context);

  PatchForceInternalDiskIcons (Context);

#if 0

  EFI_STATUS       Status;
  PATCHER_CONTEXT  Patcher;

  Status = PatcherInitContextFromPrelinked (
    &Patcher,
    Context,
    "com.apple.driver.AppleHDA"
    );

  if (!EFI_ERROR (Status)) {
    Status = PatcherApplyGenericPatch (&Patcher, &DisableAppleHDAPatch);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "Failed to apply patch com.apple.driver.AppleHDA - %r\n", Status));
    } else {
      DEBUG ((DEBUG_WARN, "Patch success com.apple.driver.AppleHDA\n"));
    }
  } else {
    DEBUG ((DEBUG_WARN, "Failed to find com.apple.driver.AppleHDA - %r\n", Status));
  }

  Status = PatcherInitContextFromPrelinked (
    &Patcher,
    Context,
    "com.apple.driver.AppleHDAController"
    );

  if (!EFI_ERROR (Status)) {
    Status = PatcherBlockKext (&Patcher);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "Failed to block com.apple.driver.AppleHDAController - %r\n", Status));
    } else {
      DEBUG ((DEBUG_WARN, "Block success com.apple.driver.AppleHDAController\n"));
    }
  } else {
    DEBUG ((DEBUG_WARN, "Failed to find com.apple.driver.AppleHDAController - %r\n", Status));
  }

#endif
}

STATIC
VOID
ApplyKernelPatches (
  IN OUT UINT8   *Kernel,
  IN     UINT32  Size
  )
{
#if 0
  EFI_STATUS       Status;
  PATCHER_CONTEXT  Patcher;

  Status = PatcherInitContextFromBuffer (
    &Patcher,
    Kernel,
    Size
    );

  if (!EFI_ERROR (Status)) {
    Status = PatcherApplyGenericPatch (&Patcher, &KernelPatch);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "Failed to apply patch kernel - %r\n", Status));
    } else {
      DEBUG ((DEBUG_WARN, "Patch success kernel\n"));
    }
  } else {
    DEBUG ((DEBUG_WARN, "Failed to find kernel - %r\n", Status));
  }
#endif
}

extern UINT8 LiluKextData[];
extern UINT32 LiluKextDataSize;
extern CHAR8 LiluKextInfoPlistData[];
extern UINT32 LiluKextInfoPlistDataSize;
extern UINT8 VsmcKextData[];
extern UINT32 VsmcKextDataSize;
extern CHAR8 VsmcKextInfoPlistData[];
extern UINT32 VsmcKextInfoPlistDataSize;

STATIC
UINT32
CalculateReserveSize (
  VOID
  )
{
  UINT32  ReserveSize;
  EFI_STATUS Status;

  ReserveSize = PRELINK_INFO_RESERVE_SIZE;

  OC_KEXT_CONTAINER_PROTOCOL *kextContainer;

  Status = gBS->LocateProtocol(
    &gOcKextContainerProtocolGuid,
    NULL,
    (VOID**)&kextContainer
    );

  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_WARN, "Failed to locate OC_KEXT_CONTAINER_PROTOCOL", Status));
  }
  else {

    OC_KEXT_DESCRIPTOR *KextDescriptors;
    UINTN KextCount;
    Status = kextContainer->GetKexts(kextContainer, &KextDescriptors, &KextCount);

    if (EFI_ERROR(Status)) {
      DEBUG ((DEBUG_WARN, "Failed to get kexts", Status));
    }
    else {
      for (UINTN i = 0; i < KextCount; i++) {
        OC_KEXT_DESCRIPTOR *Kext = &KextDescriptors[i];
        PrelinkedReserveKextSize (&ReserveSize, Kext->InfoPlistSize, Kext->Executable, Kext->ExecutableSize);
      }
    }
  }

  DEBUG ((DEBUG_INFO, "Kext reservation size %u\n", ReserveSize));

  return ReserveSize;
}

STATIC
EFI_STATUS
TestInjectPrelinked (
  IN OUT UINT8   *Kernel,
  IN     UINT32  *KernelSize,
  IN     UINT32  AllocatedSize
  )
{
  EFI_STATUS         Status;
  PRELINKED_CONTEXT  Context;
  OC_KEXT_CONTAINER_PROTOCOL *kextContainer;
  OC_KEXT_DESCRIPTOR *KextDescriptors;
  UINTN KextCount;

  Status = PrelinkedContextInit (&Context, Kernel, *KernelSize, AllocatedSize);

  if (!EFI_ERROR (Status)) {

    ApplyKextPatches (&Context);

    Status = PrelinkedInjectPrepare (&Context);

    if (EFI_ERROR(Status)) {
      DEBUG ((DEBUG_WARN, "Failed to prepare prelinked kernel for injection", Status));
    }
    else {
      Status = gBS->LocateProtocol(
        &gOcKextContainerProtocolGuid,
        NULL,
        (VOID**)&kextContainer
        );

      if (EFI_ERROR(Status)) {
        DEBUG ((DEBUG_WARN, "Failed to locate OC_KEXT_CONTAINER_PROTOCOL", Status));
      }
      else {
        Status = kextContainer->GetKexts(kextContainer, &KextDescriptors, &KextCount);

        if (EFI_ERROR(Status)) {
          DEBUG ((DEBUG_WARN, "Failed to get kexts", Status));
        }
        else {
          for (UINTN i = 0; i < KextCount; i++) {
            OC_KEXT_DESCRIPTOR *Kext = &KextDescriptors[i];
            Status = PrelinkedInjectKext (
              &Context,
              Kext->KextPath,
              (const CHAR8*)Kext->InfoPlist,
              Kext->InfoPlistSize,
              Kext->ExecutablePath,
              Kext->Executable,
              Kext->ExecutableSize
              );

            DEBUG ((DEBUG_WARN, "kext injected - %r\n", Status));
          }
        }
      }

      Status = PrelinkedInjectComplete (&Context);
      if (EFI_ERROR(Status)) {
        DEBUG ((DEBUG_WARN, "Failed to finalize prelinked kernel", Status));
      }
    }

    *KernelSize = Context.PrelinkedSize;

    PrelinkedContextFree (&Context);
  }

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
TestFileOpen (
  IN  EFI_FILE_PROTOCOL       *This,
  OUT EFI_FILE_PROTOCOL       **NewHandle,
  IN  CHAR16                  *FileName,
  IN  UINT64                  OpenMode,
  IN  UINT64                  Attributes
  )
{
  EFI_STATUS         Status;
  UINT8              *Kernel;
  UINT32             KernelSize;
  UINT32             AllocatedSize;
  CHAR16             *FileNameCopy;
  EFI_FILE_PROTOCOL  *VirtualFileHandle;
  EFI_STATUS         PrelinkedStatus;
  EFI_TIME           ModificationTime;

  Status = This->Open (This, NewHandle, FileName, OpenMode, Attributes);

  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // boot.efi uses /S/L/K/kernel as is to determine valid filesystem.
  // Just skip it to speedup the boot process.
  // On 10.9 mach_kernel is loaded for manual linking aferwards, so we cannot skip it.
  //
  if (OpenMode == EFI_FILE_MODE_READ
    && StrStr (FileName, L"kernel") != NULL
    && StrCmp (FileName, L"System\\Library\\Kernels\\kernel") != 0) {

    Print (L"Trying XNU hook on %s\n", FileName);
    Status = ReadAppleKernel (
      *NewHandle,
      &Kernel,
      &KernelSize,
      &AllocatedSize,
      CalculateReserveSize ()
      );
    Print (L"Result of XNU hook on %s is %r\n", FileName, Status);

    //
    // This is not Apple kernel, just return the original file.
    //
    if (!EFI_ERROR (Status)) {
      //
      // TODO: patches, dropping, and injection here.
      //

      ApplyKernelPatches (Kernel, KernelSize);

      PrelinkedStatus = TestInjectPrelinked (Kernel, &KernelSize, AllocatedSize);

      DEBUG ((DEBUG_WARN, "Prelinked status - %r\n", PrelinkedStatus));

      ApplyPatch (
        (UINT8 *) "Darwin Kernel Version",
        NULL,
        L_STR_LEN ("Darwin Kernel Version"),
        (UINT8 *) "OpenCore Boot Version",
        NULL,
        Kernel,
        KernelSize,
        0, ///< At least on 10.14 we have BSD version string.
        0
        );

      Status = ReadFileModifcationTime (*NewHandle, &ModificationTime);
      if (EFI_ERROR (Status)) {
        ZeroMem (&ModificationTime, sizeof (ModificationTime));
      }

      (*NewHandle)->Close(*NewHandle);

      //
      // This was our file, yet firmware is dying.
      //
      FileNameCopy = AllocateCopyPool (StrSize (FileName), FileName);
      if (FileNameCopy == NULL) {
        DEBUG ((DEBUG_WARN, "Failed to allocate kernel name (%a) copy\n", FileName));
        FreePool (Kernel);
        return EFI_OUT_OF_RESOURCES;
      }

      Status = CreateVirtualFile (FileNameCopy, Kernel, KernelSize, &ModificationTime, &VirtualFileHandle);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_WARN, "Failed to virtualise kernel file (%a)\n", FileName));
        FreePool (Kernel);
        FreePool (FileNameCopy);
        return EFI_OUT_OF_RESOURCES;
      }

#if 0
      STATIC UINT32 DumpCounter;
      CHAR16 DumpName[64];
      UnicodeSPrint (DumpName, sizeof (DumpName), L"prel%u.bin", DumpCounter++);
      Print (
        L"Writing resulting kernel of %u bytes - %r\n",
        KernelSize,
        WriteFileData (NULL, DumpName, Kernel, KernelSize)
        );
#endif

      //
      // Return our handle.
      //
      *NewHandle = VirtualFileHandle;
      return EFI_SUCCESS;
    }
  }

  return CreateRealFile (*NewHandle, NULL, TRUE, NewHandle);
}

EFI_STATUS
EFIAPI
TestKernel (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = EnableVirtualFs (gBS, TestFileOpen);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Failed to enable vfs - %r\n", Status));
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  UINTN       Index;

  WaitForKeyPress (L"Press any key...");

  for (Index = 0; Index < SystemTable->NumberOfTableEntries; ++Index) {
    Print (L"Table %u is %g\n", (UINT32) Index, &SystemTable->ConfigurationTable[Index].VendorGuid);
  }

  Print (L"This is test app...\n");

  TestKernel (ImageHandle, SystemTable);

  Status = DisableVirtualFs (gBS);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Failed to disable vfs - %r\n", Status));
  }

  return EFI_SUCCESS;
}
