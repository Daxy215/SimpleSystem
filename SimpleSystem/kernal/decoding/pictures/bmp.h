#ifndef BMP_H
#define BMP_H

#include "../../io.h"

#pragma pack(push, 1)  // Ensure no padding is added

// BMP File Header (14 bytes)
typedef struct {
	u16 bfType;      // File type: should be 'BM' (0x4D42)
	u32 bfSize;      // Size of the file in bytes
	u16 bfReserved1; // Reserved, must be 0
	u16 bfReserved2; // Reserved, must be 0
	u32 bfOffBits;   // Offset to start of pixel data
} BITMAPFILEHEADER;

// DIB Header (Bitmap Info Header - 40 bytes)
typedef struct {
	u32 biSize;          // Size of this header (40 bytes)
	i32  biWidth;         // Width of the bitmap in pixels
	i32  biHeight;        // Height of the bitmap in pixels
	u16 biPlanes;        // Number of color planes (must be 1)
	u16 biBitCount;      // Bits per pixel (e.g., 24 for RGB)
	u32 biCompression;   // Compression type (0 = uncompressed)
	u32 biSizeImage;     // Size of pixel data (can be 0 if uncompressed)
	i32  biXPelsPerMeter; // Horizontal resolution (pixels per meter)
	i32  biYPelsPerMeter; // Vertical resolution (pixels per meter)
	u32 biClrUsed;       // Number of colors in palette (0 = default)
	u32 biClrImportant;  // Important colors (0 = all)
} BITMAPINFOHEADER;

#pragma pack(pop)

#endif // BMP_H
