// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * Copy an FColor (BGRA) image to the system clipboard as a packed DIB (CF_DIB, 32bpp BI_RGB).
 *
 * @param Pixels  Top-down rows of FColor (B,G,R,A). Must contain Width*Height entries.
 * @param Width   Image width in pixels.
 * @param Height  Image height in pixels.
 * @return True on success.
 *
 * Windows only; on other platforms this is a no-op that returns false.
 */
bool GraphShotCopyPixelsToClipboard(const TArray<FColor>& Pixels, int32 Width, int32 Height);
