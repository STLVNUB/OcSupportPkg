/** @file
  Copyright (C) 2019, vit9696. All rights reserved.

  All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef OC_DEBUG_LOG_LIB_H
#define OC_DEBUG_LOG_LIB_H

#include <Library/DebugLib.h>
#include <Protocol/OcLog.h>

/**
  Install or update the OcLog protocol with specified options.

  @param[in] Options       Logging options.
  @param[in] Delay         Delay in microseconds after each log entry.
  @param[in] DisplayLevel  Console visible error level.
  @param[in] HaltLevel     Error level causing CPU halt.

  @retval EFI_SUCCESS  The entry point is executed successfully.
**/
EFI_STATUS
OcConfigureLogProtocol (
  IN OC_LOG_OPTIONS      Options,
  IN UINT32              Delay,
  IN UINTN               DisplayLevel,
  IN UINTN               HaltLevel
  );

#endif // OC_DEBUG_LOG_LIB_H
