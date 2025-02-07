# ABIK

ABIK is a powerful Android application that lets you easily unpack and repack Android boot images.

## Features

- **Unpack & Repack Boot Images:**  
  Easily extract and rebuild Android boot images.
  
- **Supported Boot Image Types:**  
  - `init_boot`  
  - `boot`  
  - `recovery`  
  - `vendor_boot` (v4 is also supported)
  
- **Ramdisk Decompression:**  
  Automatically decompress ramdisks compressed with:  
  - LZMA  
  - GZIP  
  - LZ4

## Important Note

The boot image **must** be from a device that was launched with **Android 9 (Pie) or newer**. There are no additional device requirements or root permissions needed to run ABIK.
