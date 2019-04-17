/** @file
  Copyright (C) 2019, Goldfish64. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/OcAppleChunklistLib.h>
#include <Library/OcAppleDiskImageLib.h>
#include <Library/OcCompressionLib.h>
#include <Library/OcGuardLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "OcAppleDiskImageLibInternal.h"

VOID *
OcAppleDiskImageAllocateBuffer (
  IN UINTN  BufferSize
  )
{
  EFI_STATUS           Status;
  EFI_PHYSICAL_ADDRESS BufferAddress;

  ASSERT (BufferSize > 0);

  Status = gBS->AllocatePages (
                  AllocateAnyPages,
                  EfiACPIMemoryNVS,
                  EFI_SIZE_TO_PAGES (BufferSize),
                  &BufferAddress
                  );
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  return (VOID *)(UINTN)BufferAddress;
}

VOID
OcAppleDiskImageFreeBuffer (
  IN VOID   *Buffer,
  IN UINTN  BufferSize
  )
{
  ASSERT (Buffer != NULL);
  ASSERT (BufferSize > 0);

  gBS->FreePages (
         (EFI_PHYSICAL_ADDRESS)(UINTN)Buffer,
         EFI_SIZE_TO_PAGES (BufferSize)
         );
}

/**
  Allocates and fills in the Page Directory and Page Table Entries to
  establish a 1:1 Virtual to Physical mapping.

  @return The address of 4 level page map.

**/
UINTN
CreateIdentityMappingPageTables (
  IN UINT64                          RoStart,
  IN UINT64                          RoSize
  );

BOOLEAN
OcAppleDiskImageInitializeContext (
  OUT OC_APPLE_DISK_IMAGE_CONTEXT  *Context,
  IN  VOID                         *Buffer,
  IN  UINTN                        BufferSize,
  IN  BOOLEAN                      VerifyChecksum
  )
{
  BOOLEAN                     Result;
  UINTN                       TrailerOffset;
  UINT8                       *BufferBytes;
  UINT8                       *BufferBytesCurrent;
  APPLE_DISK_IMAGE_TRAILER    *Trailer;
  UINT32                      DmgBlockCount;
  APPLE_DISK_IMAGE_BLOCK_DATA **DmgBlocks;
  UINT32                      Crc32;
  UINT32                      SwappedSig;
  UINT64                      OffsetTop;

  UINT32                      HeaderSize;
  UINT64                      DataForkOffset;
  UINT64                      DataForkLength;
  UINT32                      SegmentCount;
  APPLE_DISK_IMAGE_CHECKSUM   DataForkChecksum;
  UINT64                      XmlOffset;
  UINT64                      XmlLength;
  UINT64                      SectorCount;

  ASSERT (Context != NULL);
  ASSERT (Buffer != NULL);
  ASSERT (BufferSize > 0);

  if (BufferSize <= sizeof (*Trailer)) {
    return FALSE;
  }

  SwappedSig = SwapBytes32 (APPLE_DISK_IMAGE_MAGIC);

  BufferBytes = (UINT8 *)Buffer;

  Trailer       = NULL;
  TrailerOffset = 0;

  for (
    BufferBytesCurrent = (BufferBytes + (BufferSize - sizeof (*Trailer)));
    BufferBytesCurrent >= BufferBytes;
    --BufferBytesCurrent
    ) {
    Trailer = (APPLE_DISK_IMAGE_TRAILER *)BufferBytesCurrent;
    if (Trailer->Signature == SwappedSig) {
      TrailerOffset = (BufferBytesCurrent - BufferBytes);
      break;
    }
  }

  if (TrailerOffset == 0) {
    return FALSE;
  }

  HeaderSize            = SwapBytes32 (Trailer->HeaderSize);
  DataForkOffset        = SwapBytes64 (Trailer->DataForkOffset);
  DataForkLength        = SwapBytes64 (Trailer->DataForkLength);
  SegmentCount          = SwapBytes32 (Trailer->SegmentCount);
  XmlOffset             = SwapBytes64 (Trailer->XmlOffset);
  XmlLength             = SwapBytes64 (Trailer->XmlLength);
  SectorCount           = SwapBytes64 (Trailer->SectorCount);
  DataForkChecksum.Size = SwapBytes32 (Trailer->DataForkChecksum.Size);

  if ((HeaderSize != sizeof (*Trailer))
   || (XmlLength == 0)
   || (XmlLength > MAX_UINT32)
   || (DataForkChecksum.Size > (sizeof (DataForkChecksum.Data) * 8))) {
    return FALSE;
  }

  if ((SegmentCount != 0) && (SegmentCount != 1)) {
    DEBUG ((DEBUG_ERROR, "Multiple segments are unsupported.\n"));
    return FALSE;
  }

  Result = OcOverflowAddU64 (
             XmlOffset,
             XmlLength,
             &OffsetTop
             );
  if (Result || (OffsetTop > TrailerOffset)) {
    return FALSE;
  }

  Result = OcOverflowAddU64 (
             DataForkOffset,
             DataForkLength,
             &OffsetTop
             );
  if (Result || (OffsetTop > TrailerOffset)) {
    return FALSE;
  }

  if (VerifyChecksum) {
    DataForkChecksum.Type = SwapBytes32 (Trailer->DataForkChecksum.Type);

    if (DataForkChecksum.Type == APPLE_DISK_IMAGE_CHECKSUM_TYPE_CRC32) {
      if (DataForkChecksum.Size != 32) {
        return FALSE;
      }

      DataForkChecksum.Data[0] = SwapBytes32 (
                                   Trailer->DataForkChecksum.Data[0]
                                   );
      Crc32 = CalculateCrc32 (
                (BufferBytes + DataForkOffset),
                DataForkLength
                );
      if (Crc32 != DataForkChecksum.Data[0]) {
        return FALSE;
      }
    } else {
      DEBUG ((
        DEBUG_ERROR,
        "DMG checksum algorithm %x unsupported.\n",
        DataForkChecksum.Type
        ));
      return FALSE;
    }
  }

  Result = InternalParsePlist (
             ((CHAR8 *)Buffer + XmlOffset),
             (UINT32)XmlLength,
             DataForkOffset,
             DataForkLength,
             &DmgBlockCount,
             &DmgBlocks
             );
  if (!Result) {
    return FALSE;
  }

  Context->Buffer      = BufferBytes;
  Context->Length      = (TrailerOffset + sizeof (*Trailer));
  Context->BlockCount  = DmgBlockCount;
  Context->Blocks      = DmgBlocks;
  Context->SectorCount = SectorCount;

  AsmWriteCr3 (CreateIdentityMappingPageTables ((UINTN)Buffer, EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (BufferSize))));

  return TRUE;
}

BOOLEAN
OcAppleDiskImageVerifyData (
  IN OUT OC_APPLE_DISK_IMAGE_CONTEXT  *Context,
  IN OUT OC_APPLE_CHUNKLIST_CONTEXT   *ChunklistContext
  )
{
  ASSERT (Context != NULL);
  ASSERT (ChunklistContext != NULL);

  return OcAppleChunklistVerifyData (
           ChunklistContext,
           Context->Buffer,
           Context->Length
           );
}

VOID
OcAppleDiskImageFreeContext (
  IN OC_APPLE_DISK_IMAGE_CONTEXT  *Context
  )
{
  UINT32 Index;

  ASSERT (Context != NULL);

  for (Index = 0; Index < Context->BlockCount; ++Index) {
    FreePool (Context->Blocks[Index]);
  }

  FreePool (Context->Blocks);
}

VOID
OcAppleDiskImageFreeContextAndBuffer (
  IN OC_APPLE_DISK_IMAGE_CONTEXT  *Context
  )
{
  OcAppleDiskImageFreeBuffer (Context->Buffer, Context->Length);
  OcAppleDiskImageFreeContext (Context);
}

BOOLEAN
OcAppleDiskImageRead (
  IN  OC_APPLE_DISK_IMAGE_CONTEXT  *Context,
  IN  UINT64                       Lba,
  IN  UINTN                        BufferSize,
  OUT VOID                         *Buffer
  )
{
  BOOLEAN                     Result;

  APPLE_DISK_IMAGE_BLOCK_DATA *BlockData;
  APPLE_DISK_IMAGE_CHUNK      *Chunk;
  UINT64                      ChunkTotalLength;
  UINT64                      ChunkLength;
  UINT64                      ChunkOffset;
  UINT8                       *ChunkData;
  UINT8                       *ChunkDataCurrent;

  UINT64                      LbaCurrent;
  UINT64                      LbaOffset;
  UINT64                      LbaLength;
  UINTN                       RemainingBufferSize;
  UINTN                       BufferChunkSize;
  UINT8                       *BufferCurrent;

  UINTN                       OutSize;

  ASSERT (Context != NULL);
  ASSERT (Buffer != NULL);
  ASSERT (Lba < Context->SectorCount);

  LbaCurrent          = Lba;
  RemainingBufferSize = BufferSize;
  BufferCurrent       = Buffer;

  while (RemainingBufferSize > 0) {
    Result = InternalGetBlockChunk (Context, LbaCurrent, &BlockData, &Chunk);
    if (!Result) {
      return FALSE;
    }

    LbaOffset = (LbaCurrent - DMG_SECTOR_START_ABS (BlockData, Chunk));
    LbaLength = (Chunk->SectorCount - LbaOffset);

    Result = OcOverflowMulU64 (
               LbaOffset,
               APPLE_DISK_IMAGE_SECTOR_SIZE,
               &ChunkOffset
               );
    if (Result) {
      return FALSE;
    }

    Result = OcOverflowMulU64 (
               Chunk->SectorCount,
               APPLE_DISK_IMAGE_SECTOR_SIZE,
               &ChunkTotalLength
               );
    if (Result) {
      return FALSE;
    }

    ChunkLength = (ChunkTotalLength - ChunkOffset);

    BufferChunkSize = (UINTN)MIN (RemainingBufferSize, ChunkLength);

    switch (Chunk->Type) {
      case APPLE_DISK_IMAGE_CHUNK_TYPE_ZERO:
      case APPLE_DISK_IMAGE_CHUNK_TYPE_IGNORE:
      {
        ZeroMem (BufferCurrent, BufferChunkSize);
        break;
      }

      case APPLE_DISK_IMAGE_CHUNK_TYPE_RAW:
      {
        ChunkData = (Context->Buffer + Chunk->CompressedOffset);
        ChunkDataCurrent = (ChunkData + ChunkOffset);

        CopyMem (BufferCurrent, ChunkDataCurrent, BufferChunkSize);
        break;
      }

      case APPLE_DISK_IMAGE_CHUNK_TYPE_ZLIB:
      {
        ChunkData = AllocatePool (ChunkTotalLength);
        if (ChunkData == NULL) {
          return FALSE;
        }

        ChunkDataCurrent = ChunkData + ChunkOffset;
        OutSize = DecompressZLIB (
                    ChunkData,
                    ChunkTotalLength,
                    (Context->Buffer + Chunk->CompressedOffset),
                    Chunk->CompressedLength
                    );
        if (OutSize != ChunkTotalLength) {
          FreePool (ChunkData);
          return FALSE;
        }

        CopyMem (BufferCurrent, ChunkDataCurrent, BufferChunkSize);
        FreePool (ChunkData);
        break;
      }

      default:
      {
        DEBUG ((
          DEBUG_ERROR,
          "Compression type %x unsupported.\n",
          Chunk->Type
          ));
        return FALSE;
      }
    }

    RemainingBufferSize -= BufferChunkSize;
    BufferCurrent       += BufferChunkSize;
    LbaCurrent          += LbaLength;
  }

  return TRUE;
}
