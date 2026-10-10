#include "GraphShotWidgetRendering.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateDrawBuffer.h"
#include "Rendering/SlateRenderer.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"

UTextureRenderTarget2D* GraphShotRenderWidget(FWidgetRenderer& Renderer,
	const TSharedRef<SWidget>& Widget, FVector2D Size)
{
	// The first paint requests SVG rasterizations at the capture's actual scale.
	// Waiting on the render thread alone does not process those game-thread requests.
	UTextureRenderTarget2D* Target = Renderer.DrawWidget(Widget, Size);
	if (!Target)
	{
		return nullptr;
	}
	FlushRenderingCommands();

	// The desktop renderer owns the shared SVG atlas. An empty draw processes its
	// pending resource updates without ticking the editor or repainting live windows.
	FSlateRenderer& SlateRenderer = *FSlateApplication::Get().GetRenderer();
	{
		FSlateRenderer::FScopedAcquireDrawBuffer Buffer(SlateRenderer);
		Buffer.GetDrawBuffer().ClearBuffer();
		SlateRenderer.DrawWindows(Buffer.GetDrawBuffer());
	}
	FlushRenderingCommands();

	// Paint again so draw elements use the now-populated resource proxies/UVs.
	Renderer.DrawWidget(Target, Widget, Size, 0.0f);
	FlushRenderingCommands();
	return Target;
}
