#!/bin/bash

KEXT_NAME=`basename "$1"`
rm -Rf "$KEXT_NAME"
mkdir "$KEXT_NAME"
cp InjectKext.* "$KEXT_NAME"

echo "#include <Protocol/OcKextContainer.h>" > "$KEXT_NAME"/KextData.c
echo "STATIC CHAR8 KextPath[] = \"/Library/Extensions/$KEXT_NAME\";" >> "$KEXT_NAME"/KextData.c


echo "STATIC UINT8 KextInfoPlistData[] = {" >> "$KEXT_NAME"/KextData.c
xxd -i < "$1/Contents/Info.plist" >> "$KEXT_NAME"/KextData.c
echo "};" >> "$KEXT_NAME"/KextData.c

for i in "$1/Contents/MacOS"/*; do
  echo "STATIC CHAR8 KextBinaryPath[] = \"Contents/MacOS/$(basename $i)\";" >> "$KEXT_NAME"/KextData.c
  echo "STATIC UINT8 KextData[] = {" >> "$KEXT_NAME"/KextData.c
  xxd -i < "$1/Contents/MacOS"/* >> "$KEXT_NAME"/KextData.c
  echo "};" >> "$KEXT_NAME"/KextData.c

done

echo "OC_KEXT_DESCRIPTOR Kext = {"  >> "$KEXT_NAME"/KextData.c
echo "    .Executable = KextData,"  >> "$KEXT_NAME"/KextData.c
echo "    .ExecutableSize = sizeof(KextData),"  >> "$KEXT_NAME"/KextData.c
echo "    .InfoPlist = KextInfoPlistData,"  >> "$KEXT_NAME"/KextData.c
echo "    .InfoPlistSize = sizeof(KextInfoPlistData),"  >> "$KEXT_NAME"/KextData.c
echo "    .KextPath = KextPath,"  >> "$KEXT_NAME"/KextData.c
echo "    .ExecutablePath = KextBinaryPath"  >> "$KEXT_NAME"/KextData.c
echo "};"  >> "$KEXT_NAME"/KextData.c


echo "[Sources]" >> "$KEXT_NAME"/InjectKext.inf
echo "KextData.c" >> "$KEXT_NAME"/InjectKext.inf

echo "[Defines]" >> "$KEXT_NAME"/InjectKext.inf
echo "BASE_NAME = $KEXT_NAME" >> "$KEXT_NAME"/InjectKext.inf
echo "FILE_GUID = $(uuidgen)" >> "$KEXT_NAME"/InjectKext.inf

cp ../../OcSupportPkg.dsc ../../OcSupportPkg.dsc.bak
echo "[Components]" >> ../../OcSupportPkg.dsc
echo "OcSupportPkg/Tests/KextPacker/$KEXT_NAME/InjectKext.inf" >> ../../OcSupportPkg.dsc
build -a X64 -b RELEASE -t XCODE5 -p OcSupportPkg/OcSupportPkg.dsc -m OcSupportPkg/Tests/KextPacker/"$KEXT_NAME"/InjectKext.inf || true
mv ../../OcSupportPkg.dsc.bak ../../OcSupportPkg.dsc

