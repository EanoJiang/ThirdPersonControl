#include "Modules/ModuleManager.h"

#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "LevelEditorViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "ScopedTransaction.h"
#include "SRootMotionAnalyzerPanel.h"
#include "ToolMenus.h"
#include "XSJArtToolsCore.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "FXSJAssetToolsEditorModule"

namespace XSJAssetToolsEditor
{
	const TCHAR* CubeMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
}

class FXSJAssetToolsEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FXSJAssetToolsEditorModule::RegisterMenus)
		);
	}

	virtual void ShutdownModule() override
	{
		if (RootMotionAnalyzerWindow.IsValid())
		{
			RootMotionAnalyzerWindow->RequestDestroyWindow();
			RootMotionAnalyzerWindow.Reset();
		}

		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}

private:
	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);

		UToolMenu* AssetToolsMenu = UToolMenus::Get()->ExtendMenu(FXSJArtToolsMenuNames::AssetTools);
		if (AssetToolsMenu == nullptr)
		{
			return;
		}

		FToolMenuSection& Section = AssetToolsMenu->FindOrAddSection(TEXT("XSJAssetToolsActions"));
		Section.AddMenuEntry(
			TEXT("RootMotionAnalyzer"),
			LOCTEXT("RootMotionAnalyzerLabel", "Root Motion 动画分析器"),
			LOCTEXT("RootMotionAnalyzerTooltip", "批量分析 AnimSequence 的帧数、时长、Root Motion 位移和平均速度"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FXSJAssetToolsEditorModule::OpenRootMotionAnalyzer))
		);

		Section.AddMenuEntry(
			TEXT("CreateTestCube"),
			LOCTEXT("CreateTestCubeLabel", "test"),
			LOCTEXT("CreateTestCubeTooltip", "Create a Cube at the active viewport focus location"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateRaw(this, &FXSJAssetToolsEditorModule::CreateTestCube),
				FCanExecuteAction::CreateRaw(this, &FXSJAssetToolsEditorModule::CanCreateTestCube)
			)
		);
	}

	void OpenRootMotionAnalyzer()
	{
		if (RootMotionAnalyzerWindow.IsValid())
		{
			RootMotionAnalyzerWindow->BringToFront(true);
			return;
		}

		RootMotionAnalyzerWindow = SNew(SWindow)
			.Title(LOCTEXT("RootMotionAnalyzerWindowTitle", "Root Motion 动画分析器"))
			.ClientSize(FVector2D(980.0f, 520.0f))
			.SupportsMaximize(true)
			.SupportsMinimize(false)
			[
				SNew(SRootMotionAnalyzerPanel)
			];

		RootMotionAnalyzerWindow->SetOnWindowClosed(
			FOnWindowClosed::CreateRaw(this, &FXSJAssetToolsEditorModule::OnRootMotionAnalyzerClosed)
		);

		FSlateApplication::Get().AddWindow(RootMotionAnalyzerWindow.ToSharedRef());
	}

	void OnRootMotionAnalyzerClosed(const TSharedRef<SWindow>& ClosedWindow)
	{
		(void)ClosedWindow;
		RootMotionAnalyzerWindow.Reset();
	}

	bool CanCreateTestCube() const
	{
		if (GEditor == nullptr || GCurrentLevelEditingViewportClient == nullptr)
		{
			return false;
		}

		const UWorld* EditorWorld = GCurrentLevelEditingViewportClient->GetWorld();
		return IsValid(EditorWorld)
			&& EditorWorld->WorldType == EWorldType::Editor
			&& EditorWorld->GetCurrentLevel() != nullptr;
	}

	void CreateTestCube()
	{
		if (!CanCreateTestCube())
		{
			UE_LOG(LogTemp, Error, TEXT("XSJAssetToolsEditor: No valid active Level Editor viewport."));
			return;
		}

		UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, XSJAssetToolsEditor::CubeMeshPath);
		if (!IsValid(CubeMesh))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("XSJAssetToolsEditor: Failed to load Cube mesh '%s'."),
				XSJAssetToolsEditor::CubeMeshPath
			);
			return;
		}

		UWorld* EditorWorld = GCurrentLevelEditingViewportClient->GetWorld();
		const FVector SpawnLocation = GCurrentLevelEditingViewportClient->GetLookAtLocation();
		FScopedTransaction Transaction(LOCTEXT("CreateTestCubeTransaction", "Create XSJArtTools Test Cube"));

		AStaticMeshActor* CubeActor = Cast<AStaticMeshActor>(
			GEditor->AddActor(
				EditorWorld->GetCurrentLevel(),
				AStaticMeshActor::StaticClass(),
				FTransform(FRotator::ZeroRotator, SpawnLocation, FVector::OneVector),
				false,
				RF_Transactional,
				true
			)
		);

		if (!IsValid(CubeActor))
		{
			Transaction.Cancel();
			UE_LOG(LogTemp, Error, TEXT("XSJAssetToolsEditor: Failed to create Cube actor."));
			return;
		}

		UStaticMeshComponent* MeshComponent = CubeActor->GetStaticMeshComponent();
		if (!IsValid(MeshComponent))
		{
			EditorWorld->EditorDestroyActor(CubeActor, true);
			Transaction.Cancel();
			UE_LOG(LogTemp, Error, TEXT("XSJAssetToolsEditor: Created actor has no StaticMeshComponent."));
			return;
		}

		CubeActor->Modify();
		MeshComponent->Modify();

		if (!MeshComponent->SetStaticMesh(CubeMesh))
		{
			EditorWorld->EditorDestroyActor(CubeActor, true);
			Transaction.Cancel();
			UE_LOG(LogTemp, Error, TEXT("XSJAssetToolsEditor: Failed to assign the Cube mesh."));
			return;
		}

		CubeActor->SetActorLabel(TEXT("XSJArtTools_TestCube"), true);
		CubeActor->SetActorScale3D(FVector::OneVector);
		CubeActor->PostEditChange();
		CubeActor->MarkPackageDirty();
		EditorWorld->MarkPackageDirty();

		UE_LOG(
			LogTemp,
			Display,
			TEXT("XSJAssetToolsEditor: Created '%s' at %s."),
			*CubeActor->GetActorLabel(),
			*SpawnLocation.ToCompactString()
		);
	}

	TSharedPtr<SWindow> RootMotionAnalyzerWindow;
};

IMPLEMENT_MODULE(FXSJAssetToolsEditorModule, XSJAssetToolsEditor)

#undef LOCTEXT_NAMESPACE
