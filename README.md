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
  - LZMA  (Repacking might not work on low ram devices)
  - GZIP  
  - LZ4

## Important Note

This project officially supports only GKI boot images (introduced with Android 11+), but it may also work with boot images from devices launched with Android 9 (Pie) or newer. Additionally, no extra device requirements or root permissions are needed to run ABIK.
