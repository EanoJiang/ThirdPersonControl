#pragma once

#include "CoreMinimal.h"

class FWidgetRenderer;
class SWidget;
class UTextureRenderTarget2D;

// Synchronous capture, including resources first requested by this widget/scale.
UTextureRenderTarget2D* GraphShotRenderWidget(FWidgetRenderer& Renderer,
	const TSharedRef<SWidget>& Widget, FVector2D Size);
