#include "SkeletonRefPoseToJsonExporter.h"
#include "SSkeletonRefPoseToJsonDropPanel.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IDesktopPlatform.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ReferenceSkeleton.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SWindow.h"
#include "XSJArtToolsCore.h"

#define LOCTEXT_NAMESPACE "SkeletonRefPoseToJsonExporter"

namespace SkeletonRefPosePrivate
{
    static FString MakeSafeFileName(const FString& AssetPackagePath)
    {
        FString Safe = AssetPackagePath;
        Safe.ReplaceInline(TEXT("/"), TEXT("_"));
        Safe.ReplaceInline(TEXT("\\"), TEXT("_"));
        Safe.ReplaceInline(TEXT("."), TEXT("_"));
        Safe.ReplaceInline(TEXT(":"), TEXT("_"));
        Safe.ReplaceInline(TEXT(" "), TEXT("_"));
        return Safe + TEXT(".json");
    }

    static FString VecToString(const FVector& V)
    {
        return FString::Printf(TEXT("%.6f,%.6f,%.6f"), V.X, V.Y, V.Z);
    }

    static FString QuatToString(const FQuat& Q)
    {
        return FString::Printf(TEXT("%.6f,%.6f,%.6f,%.6f"), Q.X, Q.Y, Q.Z, Q.W);
    }

    // Angle (in degrees) between two rotations, robust to double-cover.
    // FQuat has no static DotProduct; the dot product is operator| (A | B).
    static double RotAngleDeg(const FQuat& A, const FQuat& B)
    {
        const double Dot = FMath::Clamp(FMath::Abs(A | B), 0.0, 1.0);
        return FMath::RadiansToDegrees(2.0 * FMath::Acos(Dot));
    }

    // -----------------------------------------------------------------
    // Left/right side detection from a bone name.
    // Returns the base name (side token stripped) and a side of -1 (none),
    // 0 (left) or 1 (right). Supports common suffix conventions:
    //   _l / _r , _L / _R , l / r (single char),
    //   _left / _right , left / right , Left / Right.
    // -----------------------------------------------------------------
    struct FSideResult { FString Base; int32 Side; };
    static FSideResult DetectSide(const FString& Name)
    {
        FSideResult Out{ Name, -1 };

        auto TrySuffix = [&](const FString& SfxL, const FString& SfxR) -> bool
        {
            if (Name.EndsWith(SfxL)) { Out.Base = Name.LeftChop(SfxL.Len()); Out.Side = 0; return true; }
            if (Name.EndsWith(SfxR)) { Out.Base = Name.LeftChop(SfxR.Len()); Out.Side = 1; return true; }
            return false;
        };
        auto TryPrefix = [&](const FString& PfxL, const FString& PfxR) -> bool
        {
            if (Name.StartsWith(PfxL)) { Out.Base = Name.RightChop(PfxL.Len()); Out.Side = 0; return true; }
            if (Name.StartsWith(PfxR)) { Out.Base = Name.RightChop(PfxR.Len()); Out.Side = 1; return true; }
            return false;
        };

        // Prefer the most explicit tokens first to avoid false matches.
        if (TrySuffix(TEXT("_left"), TEXT("_right"))) return Out;
        if (TrySuffix(TEXT("_Left"), TEXT("_Right"))) return Out;
        if (TrySuffix(TEXT("left"),  TEXT("right")))  return Out;
        if (TrySuffix(TEXT("Left"),  TEXT("Right")))  return Out;
        if (TrySuffix(TEXT("_l"), TEXT("_r"))) return Out;
        if (TrySuffix(TEXT("_L"), TEXT("_R"))) return Out;
        if (TryPrefix(TEXT("left_"), TEXT("right_"))) return Out;
        if (TryPrefix(TEXT("Left_"), TEXT("Right_"))) return Out;
        // Single trailing char l / r only when the base is non-trivial (len >= 2).
        if (Name.Len() >= 2)
        {
            const TCHAR Last = Name[Name.Len() - 1];
            if (Last == TEXT('l') || Last == TEXT('L')) { Out.Base = Name.LeftChop(1); Out.Side = 0; return Out; }
            if (Last == TEXT('r') || Last == TEXT('R')) { Out.Base = Name.LeftChop(1); Out.Side = 1; return Out; }
        }
        return Out;
    }
}

FSkeletonRefPoseToJsonExporter& FSkeletonRefPoseToJsonExporter::Get()
{
    static FSkeletonRefPoseToJsonExporter Instance;
    return Instance;
}

void FSkeletonRefPoseToJsonExporter::Register()
{
    if (bRegisteredMenu) { return; }
    if (!UToolMenus::Get()) { return; }
    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSkeletonRefPoseToJsonExporter::RegisterMenu));
}

void FSkeletonRefPoseToJsonExporter::Unregister()
{
    if (UToolMenus::Get() && bRegisteredMenu)
    {
        UToolMenus::UnRegisterStartupCallback(this);
        bRegisteredMenu = false;
    }
}

bool FSkeletonRefPoseToJsonExporter::IsSkeletonAssetClass(const FAssetData& InAsset)
{
    static const FTopLevelAssetPath SkeletonPath = USkeleton::StaticClass()->GetClassPathName();
    static const FTopLevelAssetPath SkeletalMeshPath = USkeletalMesh::StaticClass()->GetClassPathName();
    return InAsset.AssetClassPath == SkeletonPath || InAsset.AssetClassPath == SkeletalMeshPath;
}

void FSkeletonRefPoseToJsonExporter::RegisterMenu()
{
    if (bRegisteredMenu) { return; }

    static const FTopLevelAssetPath SkeletonPath = USkeleton::StaticClass()->GetClassPathName();
    static const FTopLevelAssetPath SkeletalMeshPath = USkeletalMesh::StaticClass()->GetClassPathName();

    if (UToolMenu* AssetMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu"))
    {
        FToolMenuSection& Section = AssetMenu->FindOrAddSection("SkeletonRefPoseToJson");
        Section.AddDynamicEntry("ExportSkeletonRefPoseToJson", FNewToolMenuSectionDelegate::CreateLambda(
            [](FToolMenuSection& InSection)
            {
                TArray<FAssetData> SelectedAssets;
                GEditor->GetContentBrowserSelections(SelectedAssets);

                bool bHasTarget = false;
                for (const FAssetData& Asset : SelectedAssets)
                {
                    // SkeletonPath / SkeletalMeshPath are function-static; reachable
                    // inside the lambda without capture.
                    if (Asset.AssetClassPath == SkeletonPath || Asset.AssetClassPath == SkeletalMeshPath)
                    {
                        bHasTarget = true;
                        break;
                    }
                }
                if (!bHasTarget) { return; }

                InSection.AddMenuEntry(
                    "ExportSkeletonRefPoseToJson",
                    LOCTEXT("ExportRefPose", "Export Skeleton Ref Pose to JSON..."),
                    LOCTEXT("ExportRefPoseTooltip", "Export this Skeleton / SkeletalMesh reference pose (per-bone local + world transforms, with left/right orientation comparison) to a JSON file."),
                    FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Skeleton"),
                    FUIAction(FExecuteAction::CreateRaw(&FSkeletonRefPoseToJsonExporter::Get(), &FSkeletonRefPoseToJsonExporter::OnExportClicked)));
            }));
    }

    // 2) XSJArtTools main menu: open the drag-and-drop export panel
    if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(FXSJArtToolsMenuNames::SkeletonRefPoseToJson))
    {
        FToolMenuSection& Section = Menu->FindOrAddSection("SkeletonRefPoseToJsonTools", LOCTEXT("SkeletonRefPoseToJsonSection", "Skeleton Ref Pose To JSON"));
        Section.AddMenuEntry(
            "SkeletonRefPoseToJson.OpenDropPanel",
            LOCTEXT("OpenSkeletonDropPanel", "Export Skeleton Ref Pose to JSON..."),
            LOCTEXT("OpenSkeletonDropPanelTooltip", "Open a panel where you can drag Skeleton / SkeletalMesh assets in and export their reference poses to JSON."),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Skeleton"),
            FUIAction(FExecuteAction::CreateRaw(this, &FSkeletonRefPoseToJsonExporter::OpenDropPanel)));
    }

    bRegisteredMenu = true;
}

void FSkeletonRefPoseToJsonExporter::OnExportClicked()
{
    TArray<FAssetData> SelectedAssets;
    GEditor->GetContentBrowserSelections(SelectedAssets);

    static const FTopLevelAssetPath SkeletonPath = USkeleton::StaticClass()->GetClassPathName();
    static const FTopLevelAssetPath SkeletalMeshPath = USkeletalMesh::StaticClass()->GetClassPathName();

    UObject* TargetAsset = nullptr;
    for (const FAssetData& Asset : SelectedAssets)
    {
        if (Asset.AssetClassPath == SkeletonPath || Asset.AssetClassPath == SkeletalMeshPath)
        {
            TargetAsset = Asset.GetAsset();
            if (TargetAsset) { break; }
        }
    }
    if (!TargetAsset)
    {
        Notify(LOCTEXT("NoSkeletonSelected", "No Skeleton or SkeletalMesh selected."), true);
        return;
    }

    FString OutputDir;
    FString DefaultPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    if (!PickFolder(LOCTEXT("PickOutputFolder", "Select output directory"), DefaultPath, OutputDir))
    {
        return;
    }

    FString FileName;
    if (ExportAsset(TargetAsset, OutputDir, FileName))
    {
        Notify(FText::Format(LOCTEXT("ExportDone", "Exported {0} to {1}"),
            FText::FromString(TargetAsset->GetName()), FText::FromString(OutputDir)), false);
    }
    else
    {
        Notify(LOCTEXT("ExportFailed", "Export failed (no reference skeleton found)."), true);
    }
}

void FSkeletonRefPoseToJsonExporter::OpenDropPanel()
{
    if (!FSlateApplication::IsInitialized())
    {
        return;
    }

    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(LOCTEXT("DropWindowTitle", "Export Skeleton Ref Pose to JSON"))
        .ClientSize(FVector2D(640.0f, 480.0f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    Window->SetContent(SNew(SSkeletonRefPoseToJsonDropPanel));
    FSlateApplication::Get().AddWindow(Window);
}

const FReferenceSkeleton* FSkeletonRefPoseToJsonExporter::ResolveRefSkeleton(UObject* InAsset, FString& OutAssetKind)
{
    if (!InAsset) { return nullptr; }

    if (USkeleton* Skel = Cast<USkeleton>(InAsset))
    {
        OutAssetKind = TEXT("Skeleton");
        return &Skel->GetReferenceSkeleton();
    }
    if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(InAsset))
    {
        OutAssetKind = TEXT("SkeletalMesh");
        return &Mesh->GetRefSkeleton();
    }
    return nullptr;
}

bool FSkeletonRefPoseToJsonExporter::ExportAsset(UObject* InAsset, const FString& InOutputDir, FString& OutFileName)
{
    FString AssetKind;
    const FReferenceSkeleton* RefSkel = ResolveRefSkeleton(InAsset, AssetKind);
    if (!RefSkel)
    {
        return false;
    }

    const TArray<FMeshBoneInfo>& BoneInfo = RefSkel->GetRefBoneInfo();
    const TArray<FTransform>& BonePose = RefSkel->GetRefBonePose();
    const int32 NumBones = RefSkel->GetNum();

    // ------------------------------------------------------------------
    // Build per-bone world transforms by accumulating local transforms.
    // ------------------------------------------------------------------
    TArray<FTransform> WorldPose;
    WorldPose.AddDefaulted(NumBones);

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("name"), InAsset->GetName());
    Root->SetStringField(TEXT("package"), InAsset->GetOutermost() ? InAsset->GetOutermost()->GetName() : TEXT(""));
    Root->SetStringField(TEXT("class"), InAsset->GetClass()->GetName());
    Root->SetStringField(TEXT("assetKind"), AssetKind);
    Root->SetNumberField(TEXT("boneCount"), NumBones);

    TArray<TSharedPtr<FJsonValue>> BonesArray;
    // base name -> { side -> index } for the L/R pair summary.
    TMap<FString, TMap<int32, int32>> SideByBase;

    for (int32 i = 0; i < NumBones; ++i)
    {
        const FMeshBoneInfo& Info = BoneInfo[i];
        const FTransform Local = BonePose[i];
        const int32 ParentIdx = Info.ParentIndex;

        // World = parent world * local.
        FTransform World = (ParentIdx >= 0 && ParentIdx < i) ? WorldPose[ParentIdx] * Local : Local;
        WorldPose[i] = World;

        TSharedRef<FJsonObject> BoneObj = MakeShared<FJsonObject>();
        BoneObj->SetStringField(TEXT("name"), Info.Name.ToString());
        BoneObj->SetNumberField(TEXT("index"), i);
        BoneObj->SetStringField(TEXT("parent"), (ParentIdx >= 0 && ParentIdx < NumBones) ? BoneInfo[ParentIdx].Name.ToString() : TEXT(""));
        BoneObj->SetNumberField(TEXT("parentIndex"), ParentIdx);

        // LOCAL
        {
            const FQuat Q = Local.GetRotation();
            const FRotator R = Q.Rotator();
            TSharedRef<FJsonObject> Loc = MakeShared<FJsonObject>();
            Loc->SetStringField(TEXT("translation"), SkeletonRefPosePrivate::VecToString(Local.GetTranslation()));
            Loc->SetStringField(TEXT("rotationQuat"), SkeletonRefPosePrivate::QuatToString(Q));
            Loc->SetNumberField(TEXT("pitchDeg"), R.Pitch);
            Loc->SetNumberField(TEXT("yawDeg"), R.Yaw);
            Loc->SetNumberField(TEXT("rollDeg"), R.Roll);
            Loc->SetStringField(TEXT("scale"), SkeletonRefPosePrivate::VecToString(Local.GetScale3D()));
            BoneObj->SetObjectField(TEXT("local"), Loc);
        }

        // WORLD
        {
            const FQuat Q = World.GetRotation();
            const FRotator R = Q.Rotator();
            TSharedRef<FJsonObject> Wld = MakeShared<FJsonObject>();
            Wld->SetStringField(TEXT("translation"), SkeletonRefPosePrivate::VecToString(World.GetTranslation()));
            Wld->SetStringField(TEXT("rotationQuat"), SkeletonRefPosePrivate::QuatToString(Q));
            Wld->SetNumberField(TEXT("pitchDeg"), R.Pitch);
            Wld->SetNumberField(TEXT("yawDeg"), R.Yaw);
            Wld->SetNumberField(TEXT("rollDeg"), R.Roll);
            Wld->SetStringField(TEXT("scale"), SkeletonRefPosePrivate::VecToString(World.GetScale3D()));
            BoneObj->SetObjectField(TEXT("world"), Wld);
        }

        BonesArray.Add(MakeShared<FJsonValueObject>(BoneObj));

        // Index L/R pairs by base name.
        SkeletonRefPosePrivate::FSideResult Side = SkeletonRefPosePrivate::DetectSide(Info.Name.ToString());
        if (Side.Side >= 0)
        {
            TMap<int32, int32>& Inner = SideByBase.FindOrAdd(Side.Base);
            if (!Inner.Contains(Side.Side))
            {
                Inner.Add(Side.Side, i);
            }
        }
    }
    Root->SetArrayField(TEXT("bones"), BonesArray);

    // ------------------------------------------------------------------
    // L/R pair summary: compare world rotations of left vs right bone.
    //   worldRotIdentical == true  -> left and right point the SAME way
    //                                (i.e. a NON-mirrored rig like JX3)
    //   worldRotIdentical == false -> they differ (typical mirror rig like
    //                                  Mannequin, where yaw differs ~180).
    // The euler deltas show exactly which axis differs.
    // ------------------------------------------------------------------
    TArray<TSharedPtr<FJsonValue>> PairsArray;
    for (const auto& KV : SideByBase)
    {
        const TMap<int32, int32>& Inner = KV.Value;
        const int32* LeftIdx = Inner.Find(0);
        const int32* RightIdx = Inner.Find(1);
        if (!LeftIdx || !RightIdx) { continue; }

        const FTransform& LW = WorldPose[*LeftIdx];
        const FTransform& RW = WorldPose[*RightIdx];
        const FQuat LQ = LW.GetRotation();
        const FQuat RQ = RW.GetRotation();
        const FRotator LR = LQ.Rotator();
        const FRotator RR = RQ.Rotator();

        const double AngleDeg = SkeletonRefPosePrivate::RotAngleDeg(LQ, RQ);
        const bool bIdentical = AngleDeg < 0.5;

        TSharedRef<FJsonObject> Pair = MakeShared<FJsonObject>();
        Pair->SetStringField(TEXT("base"), KV.Key);
        Pair->SetStringField(TEXT("leftBone"), BoneInfo[*LeftIdx].Name.ToString());
        Pair->SetStringField(TEXT("rightBone"), BoneInfo[*RightIdx].Name.ToString());
        Pair->SetBoolField(TEXT("worldRotIdentical"), bIdentical);
        Pair->SetNumberField(TEXT("worldRotAngleDiffDeg"), AngleDeg);

        TSharedRef<FJsonObject> Deltas = MakeShared<FJsonObject>();
        Deltas->SetNumberField(TEXT("pitchDeg"), RR.Pitch - LR.Pitch);
        Deltas->SetNumberField(TEXT("yawDeg"), RR.Yaw - LR.Yaw);
        Deltas->SetNumberField(TEXT("rollDeg"), RR.Roll - LR.Roll);
        Pair->SetObjectField(TEXT("worldEulerDelta"), Deltas);

        PairsArray.Add(MakeShared<FJsonValueObject>(Pair));
    }
    Root->SetArrayField(TEXT("lrPairs"), PairsArray);
    Root->SetNumberField(TEXT("lrPairCount"), PairsArray.Num());

    // ------------------------------------------------------------------
    // Write file.
    // ------------------------------------------------------------------
    FString PackageName = InAsset->GetOutermost() ? InAsset->GetOutermost()->GetName() : InAsset->GetName();
    OutFileName = SkeletonRefPosePrivate::MakeSafeFileName(PackageName);
    const FString FullPath = FPaths::Combine(InOutputDir, OutFileName);

    FString JsonString;
    TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonString);
    FJsonSerializer::Serialize(Root, Writer);

    return FFileHelper::SaveStringToFile(JsonString, *FullPath);
}

bool FSkeletonRefPoseToJsonExporter::PickFolder(const FText& InTitle, const FString& InDefaultPath, FString& OutPath)
{
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform) { return false; }

    void* ParentWindow = nullptr;
    if (FSlateApplication::IsInitialized())
    {
        TSharedPtr<SWindow> Parent = FSlateApplication::Get().GetActiveTopLevelWindow();
        if (Parent.IsValid() && Parent->GetNativeWindow().IsValid())
        {
            ParentWindow = Parent->GetNativeWindow()->GetOSWindowHandle();
        }
    }
    return DesktopPlatform->OpenDirectoryDialog(ParentWindow, InTitle.ToString(), InDefaultPath, OutPath);
}

void FSkeletonRefPoseToJsonExporter::Notify(const FText& InMessage, bool bIsWarning)
{
    FNotificationInfo Info(InMessage);
    Info.bFireAndForget = true;
    Info.FadeInDuration = 0.1f;
    Info.FadeOutDuration = 0.5f;
    Info.ExpireDuration = bIsWarning ? 6.0f : 3.0f;

    if (bIsWarning)
    {
        Info.Image = FAppStyle::GetBrush(TEXT("NotificationBlend.Warning"));
    }
    if (FSlateApplication::IsInitialized())
    {
        FSlateNotificationManager::Get().AddNotification(Info);
    }
    UE_LOG(LogTemp, Log, TEXT("SkeletonRefPoseToJson: %s"), *InMessage.ToString());
}

#undef LOCTEXT_NAMESPACE
