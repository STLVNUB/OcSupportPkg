#!/bin/bash

for i in /Volumes/EFI/EFI/CLOVER/kexts/Other/*.kext; do
	./pack-kext.sh "$i"
done
