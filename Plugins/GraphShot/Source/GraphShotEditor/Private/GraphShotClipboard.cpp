// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphShotClipboard.h"

#include "HAL/PlatformProcess.h"
#include "Misc/FeedbackContext.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsHWrapper.h"

// Note: FColor is laid out in memory as { B, G, R, A }, which is exactly the byte order a
// 32bpp BI_RGB CF_DIB expects (the 4th byte is reserved and ignored by GDI but preserved),
// so the pixel data needs no swizzle. ReadPixels produces rows top-down; a CF_DIB with a
// positive biHeight is bottom-up, so rows are copied in reverse order.
bool GraphShotCopyPixelsToClipboard(const TArray<FColor>& Pixels, int32 Width, int32 Height)
{
	if (Width <= 0 || Height <= 0 || Pixels.Num() != Width * Height)
	{
		UE_LOG(LogTemp, Warning, TEXT("GraphShot: invalid pixel buffer (%d x %d, %d pixels)"), Width, Height, Pixels.Num());
		return false;
	}

	const int32 BytesPerRow = Width * 4;
	const int32 ImageBytes = BytesPerRow * Height;
	const SIZE_T TotalBytes = sizeof(BITMAPINFOHEADER) + (SIZE_T)ImageBytes;

	HGLOBAL Mem = GlobalAlloc(GMEM_MOVEABLE, TotalBytes);
	if (!Mem)
	{
		UE_LOG(LogTemp, Warning, TEXT("GraphShot: GlobalAlloc failed"));
		return false;
	}

	uint8* Dst = reinterpret_cast<uint8*>(GlobalLock(Mem));
	if (!Dst)
	{
		GlobalFree(Mem);
		return false;
	}

	BITMAPINFOHEADER BIH;
	FMemory::Memzero(&BIH, sizeof(BIH));
	BIH.biSize = sizeof(BITMAPINFOHEADER); // 40
	BIH.biWidth = Width;
	BIH.biHeight = Height;               // positive => bottom-up DIB
	BIH.biPlanes = 1;
	BIH.biBitCount = 32;
	BIH.biCompression = BI_RGB;          // 0
	BIH.biSizeImage = ImageBytes;
	FMemory::Memcpy(Dst, &BIH, sizeof(BITMAPINFOHEADER));

	uint8* DstPix = Dst + sizeof(BITMAPINFOHEADER);
	const FColor* Src = Pixels.GetData();
	// ReadPixels is top-down (row 0 = top). Write bottom-up so the pasted image is upright.
	for (int32 Y = Height - 1; Y >= 0; --Y)
	{
		const FColor* SrcRow = Src + Y * Width;
		FMemory::Memcpy(DstPix, SrcRow, BytesPerRow);
		DstPix += BytesPerRow;
	}
	GlobalUnlock(Mem);

	// OpenClipboard can fail transiently when another app holds it; retry briefly.
	bool bSuccess = false;
	for (int32 Try = 0; Try < 10; ++Try)
	{
		if (OpenClipboard(NULL))
		{
			EmptyClipboard();
			if (SetClipboardData(CF_DIB, Mem) != NULL)
			{
				bSuccess = true;
				CloseClipboard();
				break; // Windows now owns Mem; do not free it.
			}
			CloseClipboard();
			break;
		}
		FPlatformProcess::Sleep(0.01f);
	}

	if (!bSuccess)
	{
		GlobalFree(Mem);
		UE_LOG(LogTemp, Warning, TEXT("GraphShot: OpenClipboard/SetClipboardData failed (err=%u)"), (uint32)GetLastError());
	}
	return bSuccess;
}

#include "Windows/HideWindowsPlatformTypes.h"

#else // !PLATFORM_WINDOWS

bool GraphShotCopyPixelsToClipboard(const TArray<FColor>& Pixels, int32 Width, int32 Height)
{
	UE_LOG(LogTemp, Warning, TEXT("GraphShot: clipboard image output is only supported on Windows."));
	return false;
}

#endif // PLATFORM_WINDOWS
