#include "GraphShotWidgetRendering.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Brushes/SlateImageBrush.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Images/SImage.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGraphShotColdSvgTest,
	"GraphShot.Rendering.ColdSvgCapture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGraphShotColdSvgTest::RunTest(const FString& Parameters)
{
	// A unique resource name prevents an earlier editor frame/test from warming it.
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/GraphShot");
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString Path = FPaths::ConvertRelativePathToFull(Directory /
		(FGuid::NewGuid().ToString() + TEXT(".svg")));
	if (!FFileHelper::SaveStringToFile(TEXT("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"64\" height=\"64\"><circle cx=\"32\" cy=\"32\" r=\"20\" fill=\"#00ff00\"/></svg>"), *Path))
	{
		AddError(TEXT("Could not create cold-cache SVG fixture"));
		return false;
	}

	FWidgetRenderer Renderer(true, true);
	FSlateVectorImageBrush Brush(Path, FVector2f(64.0f, 64.0f));
	const TSharedRef<SWidget> Widget = SNew(SImage).Image(&Brush);
	// Changing the size also requires a new atlas entry, even for a cached SVG file.
	for (int32 Size : {64, 93})
	{
		UTextureRenderTarget2D* Target = GraphShotRenderWidget(Renderer, Widget, FVector2D(Size, Size));
		if (!TestNotNull(TEXT("Capture target"), Target))
		{
			break;
		}
		TArray<FColor> Pixels;
		const bool bRead = Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
		if (TestTrue(TEXT("Read capture pixels"), bRead) && TestEqual(TEXT("Pixel count"), Pixels.Num(), Size * Size))
		{
			const FColor Center = Pixels[(Size / 2) * Size + Size / 2];
			const FColor Corner = Pixels[2 * Size + 2];
			AddInfo(FString::Printf(TEXT("Size %d: center=%s corner=%s"), Size, *Center.ToString(), *Corner.ToString()));
			TestTrue(TEXT("SVG center is green, not a white missing-resource rectangle"), Center.G > 200 && Center.R < 20 && Center.B < 20);
			TestTrue(TEXT("Outside the circle stays transparent"), Corner.A < 10);
		}
		Target->ConditionalBeginDestroy();
	}
	IFileManager::Get().Delete(*Path);
	return !HasAnyErrors();
}
#endif
