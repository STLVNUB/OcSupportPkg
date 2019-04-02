/** @file
  Copyright (C) 2016, The HermitCrabs Lab. All rights reserved.

  All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef OC_KEXT_CONTAINER_PROTOCOL_H
#define OC_KEXT_CONTAINER_PROTOCOL_H

// OC_KEXT_CONTAINER_PROTOCOL_GUID
/// The GUID of the OC_KEXT_CONTAINER_PROTOCOL.
#define OC_KEXT_CONTAINER_PROTOCOL_GUID                                                        \
  {                                                                                 \
    0x62B30251, 0xC4F7, 0x41F9, { 0x8A, 0xF4, 0xEA, 0x13, 0x65, 0x88, 0xB6, 0x60 }  \
  }

typedef struct {
  UINT8* Executable;
  UINT32 ExecutableSize;
  UINT8* InfoPlist;
  UINT32 InfoPlistSize;
  CHAR8* KextPath;
  CHAR8* ExecutablePath;
} OC_KEXT_DESCRIPTOR;

// OC_KEXT_CONTAINER_PROTOCOL
/// The forward declaration for the protocol for the OC_KEXT_CONTAINER_PROTOCOL.
typedef struct OC_KEXT_CONTAINER_PROTOCOL_ OC_KEXT_CONTAINER_PROTOCOL;

// AddKext
/** Add a kext do the container

  @param[in] This              This protocol.
  @param[in] KextDescriptor    Kext descriptor.

  @retval EFI_SUCCESS  The kext was successfully added.
**/
typedef
EFI_STATUS
(EFIAPI *OC_KEXT_ADD) (
  IN OC_KEXT_CONTAINER_PROTOCOL  *This,
  IN OC_KEXT_DESCRIPTOR *KextDescriptor
  );


typedef
EFI_STATUS
(EFIAPI *OC_GET_KEXTS) (
  IN OC_KEXT_CONTAINER_PROTOCOL  *This,
  OUT OC_KEXT_DESCRIPTOR **Descriptors,
  OUT UINTN *Count
  );


/// The structure exposed by the OC_KEXT_CONTAINER_PROTOCOL.
struct OC_KEXT_CONTAINER_PROTOCOL_ {
  UINT32              Revision;     ///< The revision of the installed protocol.
  OC_KEXT_ADD         AddKext;      ///< A pointer to the AddEntry function.
  OC_GET_KEXTS        GetKexts;      ///< A pointer to the AddEntry function.
};

/// A global variable storing the GUID of the OC_LOG_PROTOCOL.
extern EFI_GUID gOcKextContainerProtocolGuid;

#endif // OC_KEXT_CONTAINER_PROTOCOL_H
