#include "ControlRigToJsonExporter.h"
#include "SControlRigToJsonExportPanel.h"
#include "SControlRigToJsonDropPanel.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Styling/AppStyle.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformFileManager.h"
#include "IDesktopPlatform.h"
#include "Json.h"
#include "Misc/FeedbackContext.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "UObject/ScriptInterface.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SWindow.h"
#include "XSJArtToolsCore.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigRuntimeAsset.h"
#include "ControlRigEditorAsset.h"
#include "RigVMEditorAsset.h"
#include "RigVMModel/RigVMClient.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/RigVMNode.h"
#include "RigVMModel/RigVMPin.h"
#include "RigVMModel/RigVMLink.h"
#include "Rigs/RigHierarchyDefines.h"
#include "Rigs/RigHierarchyElements.h"
#include "Rigs/RigHierarchy.h"

#define LOCTEXT_NAMESPACE "ControlRigToJsonExporter"

namespace ControlRigToJsonPrivate
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

    static FString PinTypeToFriendlyType(const FString& CPPType)
    {
        if (CPPType == TEXT("bool")) { return TEXT("bool"); }
        if (CPPType == TEXT("int32")) { return TEXT("int32"); }
        if (CPPType == TEXT("float")) { return TEXT("float"); }
        if (CPPType == TEXT("double")) { return TEXT("double"); }
        if (CPPType == TEXT("FString")) { return TEXT("string"); }
        if (CPPType == TEXT("FName")) { return TEXT("name"); }
        if (CPPType == TEXT("FVector")) { return TEXT("vector"); }
        if (CPPType == TEXT("FVector2D")) { return TEXT("vector2d"); }
        if (CPPType == TEXT("FRotator")) { return TEXT("rotator"); }
        if (CPPType == TEXT("FQuat")) { return TEXT("quat"); }
        if (CPPType == TEXT("FTransform")) { return TEXT("transform"); }
        if (CPPType == TEXT("FLinearColor")) { return TEXT("linearcolor"); }
        return CPPType;
    }

    static FString TransformToString(const FTransform& T)
    {
        const FVector Tr = T.GetTranslation();
        const FQuat Q = T.GetRotation();
        const FVector S = T.GetScale3D();
        return FString::Printf(TEXT("T[%.3f,%.3f,%.3f] R[%.3f,%.3f,%.3f,%.3f] S[%.3f,%.3f,%.3f]"),
            Tr.X, Tr.Y, Tr.Z, Q.X, Q.Y, Q.Z, Q.W, S.X, S.Y, S.Z);
    }

    // ---------------------------------------------------------------
    // Type-agnostic resolvers.
    //
    // UControlRigBlueprint and UControlRigRuntimeAsset are different
    // UClass hierarchies, but they both carry the same editable data:
    //   * a URigHierarchy (bones/controls/nulls)
    //   * one or more URigVMGraph models (nodes/pins/links)
    //
    // A blueprint exposes them directly (it IS the editor asset).
    // A runtime asset delegates the graph to its EditorOnly object,
    // which is a UControlRigEditorAsset (a URigVMEditorAsset) and thus
    // implements IRigVMEditorAssetInterface (GetDefaultModel / GetAllModels /
    // GetController / GetAssetVariables).
    // ---------------------------------------------------------------

    static URigHierarchy* ResolveHierarchy(UObject* InRigAsset)
    {
        if (!InRigAsset) { return nullptr; }
        if (UControlRigBlueprint* BP = Cast<UControlRigBlueprint>(InRigAsset))
        {
            return BP->GetHierarchy();
        }
        if (UControlRigRuntimeAsset* RT = Cast<UControlRigRuntimeAsset>(InRigAsset))
        {
            return RT->GetHierarchy();
        }
        return nullptr;
    }

    static TScriptInterface<IRigVMEditorAssetInterface> ResolveEditorInterface(UObject* InRigAsset)
    {
        if (!InRigAsset) { return TScriptInterface<IRigVMEditorAssetInterface>(); }

        if (UControlRigBlueprint* BP = Cast<UControlRigBlueprint>(InRigAsset))
        {
            // The blueprint itself is the editor asset.
            return BP->GetRigVMAssetInterface();
        }

        if (UControlRigRuntimeAsset* RT = Cast<UControlRigRuntimeAsset>(InRigAsset))
        {
#if WITH_EDITORONLY_DATA
            // The runtime asset's editor-only subobject holds the editable graph.
            if (UObject* EditorObj = RT->GetEditorOnlyData())
            {
                return TScriptInterface<IRigVMEditorAssetInterface>(EditorObj);
            }
#endif
        }
        return TScriptInterface<IRigVMEditorAssetInterface>();
    }

    static URigVMGraph* ResolveDefaultModel(UObject* InRigAsset)
    {
        TScriptInterface<IRigVMEditorAssetInterface> Ed = ResolveEditorInterface(InRigAsset);
        return Ed ? Ed->GetDefaultModel() : nullptr;
    }

    static URigVMController* ResolveController(UObject* InRigAsset)
    {
        TScriptInterface<IRigVMEditorAssetInterface> Ed = ResolveEditorInterface(InRigAsset);
        // GetController has two overloads (URigVMGraph* / UEdGraph*); pass an explicit
        // nullptr of the model-graph type to disambiguate (that overload's default arg).
        return Ed ? Ed->GetController((URigVMGraph*)nullptr) : nullptr;
    }

    static TArray<URigVMGraph*> ResolveAllModels(UObject* InRigAsset)
    {
        TScriptInterface<IRigVMEditorAssetInterface> Ed = ResolveEditorInterface(InRigAsset);
        return Ed ? Ed->GetAllModels() : TArray<URigVMGraph*>();
    }

    static FString ResolvePreviewMeshPath(UObject* InRigAsset)
    {
        if (!InRigAsset) { return TEXT(""); }
        if (UControlRigBlueprint* BP = Cast<UControlRigBlueprint>(InRigAsset))
        {
            USkeletalMesh* Mesh = BP->GetPreviewMesh();
            return Mesh ? Mesh->GetPathName() : TEXT("");
        }
        if (UControlRigRuntimeAsset* RT = Cast<UControlRigRuntimeAsset>(InRigAsset))
        {
            const TSoftObjectPtr<USkeletalMesh> Soft = RT->GetPreviewSkeletalMesh();
            if (!Soft.IsNull())
            {
                // Avoid loading: emit the path string directly.
                return Soft.ToSoftObjectPath().ToString();
            }
        }
        return TEXT("");
    }

    static FString ResolveParentClassName(UObject* InRigAsset)
    {
        if (UControlRigBlueprint* BP = Cast<UControlRigBlueprint>(InRigAsset))
        {
            return BP->ParentClass ? BP->ParentClass->GetName() : TEXT("");
        }
        // Runtime assets have no blueprint parent class.
        return TEXT("");
    }

    static FString ResolveAssetKind(UObject* InRigAsset)
    {
        if (InRigAsset && InRigAsset->IsA(UControlRigBlueprint::StaticClass()))
        {
            return TEXT("blueprint");
        }
        return TEXT("runtimeAsset");
    }

    static bool IsControlRigAsset(UObject* InRigAsset)
    {
        return InRigAsset
            && (InRigAsset->IsA(UControlRigBlueprint::StaticClass())
                || InRigAsset->IsA(UControlRigRuntimeAsset::StaticClass()));
    }
}

FControlRigToJsonExporter& FControlRigToJsonExporter::Get()
{
    static FControlRigToJsonExporter Instance;
    return Instance;
}

void FControlRigToJsonExporter::Register()
{
    if (bRegisteredMenu)
    {
        return;
    }
    if (!UToolMenus::Get())
    {
        return;
    }
    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FControlRigToJsonExporter::RegisterMenu));
}

void FControlRigToJsonExporter::Unregister()
{
    if (UToolMenus::Get() && bRegisteredMenu)
    {
        UToolMenus::UnRegisterStartupCallback(this);
        bRegisteredMenu = false;
    }
}

bool FControlRigToJsonExporter::IsControlRigAssetClass(const FAssetData& InAsset)
{
    static const FTopLevelAssetPath BlueprintPath = UControlRigBlueprint::StaticClass()->GetClassPathName();
    static const FTopLevelAssetPath RuntimeAssetPath = UControlRigRuntimeAsset::StaticClass()->GetClassPathName();
    return InAsset.AssetClassPath == BlueprintPath || InAsset.AssetClassPath == RuntimeAssetPath;
}

void FControlRigToJsonExporter::RegisterMenu()
{
    if (bRegisteredMenu) { return; }

    // 1) Folder right-click: batch export all Control Rigs in the selected folder
    if (UToolMenu* FolderMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.FolderContextMenu"))
    {
        FToolMenuSection& Section = FolderMenu->FindOrAddSection("XSJArtTools");
        Section.AddMenuEntry(
            "ExportControlRigsInFolderToJson",
            LOCTEXT("ExportFolderCR", "Export Control Rigs to JSON (folder)..."),
            LOCTEXT("ExportFolderCRTooltip", "Batch-export all Control Rig assets (blueprint or runtime) in this folder to JSON files."),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.ControlRigBlueprint"),
            FUIAction(FExecuteAction::CreateRaw(this, &FControlRigToJsonExporter::OnExportFolderClicked)));
    }

    // 2) Asset right-click: export a single selected Control Rig
    if (UToolMenu* AssetMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu"))
    {
        FToolMenuSection& Section = AssetMenu->FindOrAddSection("XSJArtTools");
        Section.AddDynamicEntry("ExportSingleControlRigToJson", FNewToolMenuSectionDelegate::CreateLambda(
            [this](FToolMenuSection& InSection)
            {
                TArray<FAssetData> SelectedAssets;
                GEditor->GetContentBrowserSelections(SelectedAssets);

                bool bHasCR = false;
                for (const FAssetData& Asset : SelectedAssets)
                {
                    if (IsControlRigAssetClass(Asset))
                    {
                        bHasCR = true;
                        break;
                    }
                }

                if (!bHasCR) { return; }

                InSection.AddMenuEntry(
                    "ExportControlRigToJson",
                    LOCTEXT("ExportCR", "Export Control Rig to JSON..."),
                    LOCTEXT("ExportCRTooltip", "Export this Control Rig (blueprint or runtime asset) to a JSON file."),
                    FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.ControlRigBlueprint"),
                    FUIAction(FExecuteAction::CreateRaw(this, &FControlRigToJsonExporter::OnExportClicked)));
            }));
    }

    // 3) XSJArtTools main menu: open the drag-and-drop export panel
    if (UToolMenu* MainMenu = UToolMenus::Get()->ExtendMenu(FXSJArtToolsMenuNames::ControlRigToJson))
    {
        FToolMenuSection& Section = MainMenu->FindOrAddSection("ControlRigToJsonTools", LOCTEXT("ControlRigToJsonSection", "Control Rig To JSON"));
        Section.AddMenuEntry(
            "ControlRigToJson.OpenDropPanel",
            LOCTEXT("OpenCRDropPanel", "Export Control Rig to JSON..."),
            LOCTEXT("OpenCRDropPanelTooltip", "Open a panel where you can drag Control Rig assets in and export them to JSON."),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.ControlRigBlueprint"),
            FUIAction(FExecuteAction::CreateRaw(this, &FControlRigToJsonExporter::OpenDropPanel)));
    }

    bRegisteredMenu = true;
}

void FControlRigToJsonExporter::OnExportClicked()
{
    // Check if a Control Rig asset is currently selected in the Content Browser
    TArray<FAssetData> SelectedAssets;
    GEditor->GetContentBrowserSelections(SelectedAssets);

    UObject* SelectedRig = nullptr;
    for (const FAssetData& Asset : SelectedAssets)
    {
        if (IsControlRigAssetClass(Asset))
        {
            SelectedRig = Asset.GetAsset();
            if (SelectedRig) { break; }
        }
    }

    if (SelectedRig)
    {
        // Single asset: ask for output folder, then export immediately
        FString OutputDir;
        FString DefaultPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
        if (PickFolder(LOCTEXT("PickOutputFolderSingle", "Select output directory"), DefaultPath, OutputDir))
        {
            ExportSingleControlRig(SelectedRig, OutputDir);
        }
    }
    else
    {
        // No single rig selected: show the folder-based panel
        TSharedRef<SWindow> Window = SNew(SWindow)
            .Title(LOCTEXT("ExportWindowTitle", "Export Control Rig to JSON"))
            .ClientSize(FVector2D(640, 320))
            .SupportsMinimize(false)
            .SupportsMaximize(false);

        Window->SetContent(SNew(SControlRigToJsonExportPanel));
        FSlateApplication::Get().AddWindow(Window);
    }
}

void FControlRigToJsonExporter::OnExportFolderClicked()
{
    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(LOCTEXT("ExportWindowTitle", "Export Control Rig to JSON"))
        .ClientSize(FVector2D(640, 320))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    Window->SetContent(SNew(SControlRigToJsonExportPanel));
    FSlateApplication::Get().AddWindow(Window);
}

void FControlRigToJsonExporter::OpenDropPanel()
{
    if (!FSlateApplication::IsInitialized())
    {
        return;
    }

    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(LOCTEXT("DropWindowTitle", "Export Control Rig to JSON"))
        .ClientSize(FVector2D(640.0f, 480.0f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    Window->SetContent(SNew(SControlRigToJsonDropPanel));
    FSlateApplication::Get().AddWindow(Window);
}

void FControlRigToJsonExporter::ExportSingleControlRig(UObject* InRigAsset, const FString& InOutputDir)
{
    if (!InRigAsset)
    {
        Notify(LOCTEXT("NoRigSelected", "No Control Rig selected."), true);
        return;
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(InRigAsset));

    TSharedPtr<FJsonObject> Entry;
    FString FileName;
    if (ExportSingleAsset(AssetData, InOutputDir, Entry, FileName) && Entry.IsValid())
    {
        TArray<TSharedPtr<FJsonObject>> Entries;
        Entries.Add(Entry);
        WriteIndexJson(Entries, FString(), InOutputDir);
        Notify(FText::Format(LOCTEXT("ExportSingleDone", "Exported {0} to {1}"),
            FText::FromString(InRigAsset->GetName()), FText::FromString(InOutputDir)), false);
    }
    else
    {
        Notify(LOCTEXT("ExportFailed", "Export failed."), true);
    }
}

bool FControlRigToJsonExporter::GatherControlRigAssets(const FString& InGamePath, TArray<FAssetData>& OutAssets)
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*InGamePath));
    Filter.bRecursivePaths = true;
    // Match the base classes AND any subclasses (e.g. native UControlRigRuntimeAsset derivatives).
    Filter.bRecursiveClasses = true;
    Filter.ClassPaths.Add(UControlRigBlueprint::StaticClass()->GetClassPathName());
    Filter.ClassPaths.Add(UControlRigRuntimeAsset::StaticClass()->GetClassPathName());

    TArray<FAssetData> AllAssets;
    AssetRegistry.GetAssets(Filter, AllAssets);

    if (AllAssets.Num() == 0)
    {
        return !AssetRegistry.IsLoadingAssets();
    }
    OutAssets = AllAssets;
    return true;
}

bool FControlRigToJsonExporter::ExportSingleAsset(const FAssetData& InAsset, const FString& InOutputDir,
    TSharedPtr<FJsonObject>& OutEntry, FString& OutFileName)
{
    UObject* RigAsset = InAsset.GetAsset();
    if (!ControlRigToJsonPrivate::IsControlRigAsset(RigAsset)) { return false; }

    TSharedPtr<FJsonObject> JsonObj = SerializeControlRig(RigAsset);
    if (!JsonObj.IsValid()) { return false; }

    if (!WriteControlRigJson(JsonObj.ToSharedRef(), InAsset.PackageName.ToString(), InOutputDir, OutFileName))
    {
        return false;
    }

    OutEntry = MakeShared<FJsonObject>();
    OutEntry->SetStringField(TEXT("name"), InAsset.AssetName.ToString());
    OutEntry->SetStringField(TEXT("package"), InAsset.PackageName.ToString());
    OutEntry->SetStringField(TEXT("file"), OutFileName);
    OutEntry->SetStringField(TEXT("class"), InAsset.AssetClassPath.GetAssetName().ToString());
    OutEntry->SetStringField(TEXT("assetKind"), ControlRigToJsonPrivate::ResolveAssetKind(RigAsset));
    return true;
}

void FControlRigToJsonExporter::WriteIndexJson(const TArray<TSharedPtr<FJsonObject>>& InEntries,
    const FString& InGamePath, const FString& InOutputDir)
{
    TSharedRef<FJsonObject> Index = MakeShared<FJsonObject>();
    Index->SetStringField(TEXT("source_path"), InGamePath);
    Index->SetNumberField(TEXT("exported_count"), InEntries.Num());

    TArray<TSharedPtr<FJsonValue>> EntriesArray;
    for (const TSharedPtr<FJsonObject>& Entry : InEntries)
    {
        if (Entry.IsValid()) { EntriesArray.Add(MakeShared<FJsonValueObject>(Entry)); }
    }
    Index->SetArrayField(TEXT("entries"), EntriesArray);

    FString JsonString;
    TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonString);
    FJsonSerializer::Serialize(Index, Writer);

    FFileHelper::SaveStringToFile(JsonString, *(FPaths::Combine(InOutputDir, TEXT("index.json"))));
}

bool FControlRigToJsonExporter::ContentFolderToGamePath(const FString& AbsoluteContentFolder,
    FString& OutGamePath, FString& OutError)
{
    const FString ContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
    FString NormalizedInput = FPaths::ConvertRelativePathToFull(AbsoluteContentFolder);
    FPaths::NormalizeFilename(const_cast<FString&>(ContentDir));
    FPaths::NormalizeFilename(NormalizedInput);

    if (!NormalizedInput.StartsWith(ContentDir))
    {
        OutError = FString::Printf(TEXT("Selected folder must be under: %s"), *ContentDir);
        return false;
    }

    FString Relative = NormalizedInput.RightChop(ContentDir.Len());
    Relative.RemoveFromEnd(TEXT("/"));
    Relative.RemoveFromEnd(TEXT("\\"));
    OutGamePath = FString::Printf(TEXT("/Game/%s"), *Relative);
    OutGamePath.ReplaceInline(TEXT("\\"), TEXT("/"));
    OutGamePath.ReplaceInline(TEXT("//"), TEXT("/"));
    return true;
}

bool FControlRigToJsonExporter::PickFolder(const FText& InTitle, const FString& InDefaultPath, FString& OutPath)
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

int32 FControlRigToJsonExporter::ExportDirectory(const FString& InGamePath, const FString& InOutputDir)
{
    TArray<FAssetData> Assets;
    if (!GatherControlRigAssets(InGamePath, Assets))
    {
        Notify(LOCTEXT("AssetRegBusy", "Asset registry is still loading."), true);
        return -1;
    }
    if (Assets.Num() == 0)
    {
        Notify(FText::Format(LOCTEXT("NoCRs", "No Control Rig assets under {0}"), FText::FromString(InGamePath)), true);
        return 0;
    }

    TArray<TSharedPtr<FJsonObject>> IndexEntries;
    int32 ExportedCount = 0;
    for (const FAssetData& Asset : Assets)
    {
        TSharedPtr<FJsonObject> Entry;
        FString FileName;
        if (ExportSingleAsset(Asset, InOutputDir, Entry, FileName) && Entry.IsValid())
        {
            IndexEntries.Add(Entry);
            ++ExportedCount;
        }
    }
    WriteIndexJson(IndexEntries, InGamePath, InOutputDir);
    Notify(FText::Format(LOCTEXT("ExportDone", "Exported {0} Control Rig(s)."), ExportedCount), false);
    return ExportedCount;
}

// ============================================================
// SerializeControlRig - type-agnostic over UControlRigBlueprint | UControlRigRuntimeAsset
// ============================================================
TSharedPtr<FJsonObject> FControlRigToJsonExporter::SerializeControlRig(UObject* InRigAsset)
{
    if (!InRigAsset) { return nullptr; }

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("name"), InRigAsset->GetName());
    Root->SetStringField(TEXT("package"), InRigAsset->GetOutermost() ? InRigAsset->GetOutermost()->GetName() : TEXT(""));
    Root->SetStringField(TEXT("class"), InRigAsset->GetClass()->GetName());
    Root->SetStringField(TEXT("assetKind"), ControlRigToJsonPrivate::ResolveAssetKind(InRigAsset));

    const FString ParentClass = ControlRigToJsonPrivate::ResolveParentClassName(InRigAsset);
    if (!ParentClass.IsEmpty())
    {
        Root->SetStringField(TEXT("parentClass"), ParentClass);
    }

    const FString PreviewMesh = ControlRigToJsonPrivate::ResolvePreviewMeshPath(InRigAsset);
    if (!PreviewMesh.IsEmpty())
    {
        Root->SetStringField(TEXT("previewMesh"), PreviewMesh);
    }

    TSharedPtr<FJsonObject> HierarchyObj = SerializeRigHierarchy(ControlRigToJsonPrivate::ResolveHierarchy(InRigAsset));
    if (HierarchyObj.IsValid()) { Root->SetObjectField(TEXT("hierarchy"), HierarchyObj); }

    TSharedPtr<FJsonObject> VMObj = SerializeRigVM(InRigAsset);
    if (VMObj.IsValid()) { Root->SetObjectField(TEXT("rigVM"), VMObj); }

    // Variables: local variables of the default model (matches the prior blueprint behaviour).
    // UE 5.8: NewVariables -> FRigVMGraphVariableDescription, read through the model.
    TArray<TSharedPtr<FJsonValue>> VarsArray;
    if (URigVMGraph* Model = ControlRigToJsonPrivate::ResolveDefaultModel(InRigAsset))
    {
        const TArray<FRigVMGraphVariableDescription>& LocalVars = Model->GetLocalVariables();
        for (const FRigVMGraphVariableDescription& Var : LocalVars)
        {
            TSharedRef<FJsonObject> VarObj = MakeShared<FJsonObject>();
            VarObj->SetStringField(TEXT("name"), Var.Name.ToString());
            VarObj->SetStringField(TEXT("cppType"), Var.CPPType);
            VarObj->SetStringField(TEXT("cppTypeObject"), Var.CPPTypeObject ? Var.CPPTypeObject->GetPathName() : TEXT(""));
            if (!Var.DefaultValue.IsEmpty())
            {
                VarObj->SetStringField(TEXT("defaultValue"), Var.DefaultValue);
            }
            VarsArray.Add(MakeShared<FJsonValueObject>(VarObj));
        }
    }
    Root->SetArrayField(TEXT("variables"), VarsArray);

    return Root;
}

// ============================================================
// SerializeRigHierarchy - UE 5.8 API, takes the hierarchy directly
// ============================================================
TSharedPtr<FJsonObject> FControlRigToJsonExporter::SerializeRigHierarchy(URigHierarchy* InHierarchy)
{
    TSharedRef<FJsonObject> HierarchyRoot = MakeShared<FJsonObject>();

    URigHierarchy* H = InHierarchy;
    if (!H)
    {
        HierarchyRoot->SetStringField(TEXT("error"), TEXT("No hierarchy data"));
        return HierarchyRoot;
    }

    int32 BoneCount = 0, ControlCount = 0, NullCount = 0;
    TArray<TSharedPtr<FJsonValue>> ElementsArray;

    const int32 NumElements = H->Num();
    for (int32 i = 0; i < NumElements; ++i)
    {
        const FRigElementKey Key = H->GetKey(i);
        TSharedRef<FJsonObject> ElemObj = MakeShared<FJsonObject>();

        ElemObj->SetStringField(TEXT("name"), Key.Name.ToString());
        ElemObj->SetNumberField(TEXT("index"), i);

        switch (Key.Type)
        {
        case ERigElementType::Bone:
        {
            ElemObj->SetStringField(TEXT("type"), TEXT("bone"));
            ++BoneCount;
            const FTransform LocalTransform = H->GetLocalTransform(Key, false);
            ElemObj->SetStringField(TEXT("localTransform"),
                ControlRigToJsonPrivate::TransformToString(LocalTransform));
            break;
        }
        case ERigElementType::Control:
        {
            ElemObj->SetStringField(TEXT("type"), TEXT("control"));
            ++ControlCount;

            // UE 5.8: Find takes FRigElementKey, not int32
            const FRigControlElement* CtrlElem = H->Find<FRigControlElement>(Key);
            if (CtrlElem)
            {
                ElemObj->SetStringField(TEXT("controlType"),
                    StaticEnum<ERigControlType>()->GetNameStringByValue(
                        static_cast<int64>(CtrlElem->Settings.ControlType)));
            }
            const FTransform OffsetTransform = H->GetLocalTransform(Key, false);
            ElemObj->SetStringField(TEXT("offsetTransform"),
                ControlRigToJsonPrivate::TransformToString(OffsetTransform));
            break;
        }
        case ERigElementType::Null:
            ElemObj->SetStringField(TEXT("type"), TEXT("null"));
            ++NullCount;
            break;
        case ERigElementType::Connector:
            ElemObj->SetStringField(TEXT("type"), TEXT("connector"));
            break;
        case ERigElementType::Socket:
            ElemObj->SetStringField(TEXT("type"), TEXT("socket"));
            break;
        default:
            ElemObj->SetStringField(TEXT("type"), TEXT("unknown"));
            break;
        }

        // UE 5.8: GetFirstParent instead of GetParentKey
        const FRigElementKey ParentKey = H->GetFirstParent(Key);
        ElemObj->SetStringField(TEXT("parent"), ParentKey.IsValid() ? ParentKey.Name.ToString() : TEXT(""));

        ElementsArray.Add(MakeShared<FJsonValueObject>(ElemObj));
    }

    HierarchyRoot->SetNumberField(TEXT("boneCount"), BoneCount);
    HierarchyRoot->SetNumberField(TEXT("controlCount"), ControlCount);
    HierarchyRoot->SetNumberField(TEXT("nullCount"), NullCount);
    HierarchyRoot->SetNumberField(TEXT("totalElements"), NumElements);
    HierarchyRoot->SetArrayField(TEXT("elements"), ElementsArray);
    return HierarchyRoot;
}

// ============================================================
// SerializeRigVM / SerializeRigVMGraph
// ============================================================
TSharedPtr<FJsonObject> FControlRigToJsonExporter::SerializeRigVM(UObject* InRigAsset)
{
    if (!InRigAsset) { return nullptr; }

    TSharedRef<FJsonObject> VMRoot = MakeShared<FJsonObject>();

    URigVMGraph* Model = ControlRigToJsonPrivate::ResolveDefaultModel(InRigAsset);
    if (!Model)
    {
        VMRoot->SetStringField(TEXT("error"), TEXT("No RigVM model"));
        return VMRoot;
    }

    VMRoot->SetObjectField(TEXT("model"), SerializeRigVMGraph(Model));

    // For authored blueprints a controller already exists; for a freshly loaded
    // runtime asset it may not, so guard the pointer.
    URigVMController* Controller = ControlRigToJsonPrivate::ResolveController(InRigAsset);
    URigVMGraph* CtrlGraph = Controller ? Controller->GetGraph() : nullptr;
    if (CtrlGraph && CtrlGraph != Model)
    {
        VMRoot->SetObjectField(TEXT("controllerGraph"), SerializeRigVMGraph(CtrlGraph));
    }

    // Also surface every model (default graph + function libraries + nested graphs)
    // so runtime assets / modular rigs are fully captured.
    TArray<URigVMGraph*> AllModels = ControlRigToJsonPrivate::ResolveAllModels(InRigAsset);
    if (AllModels.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> GraphsArray;
        for (URigVMGraph* Graph : AllModels)
        {
            if (Graph)
            {
                GraphsArray.Add(MakeShared<FJsonValueObject>(SerializeRigVMGraph(Graph)));
            }
        }
        VMRoot->SetArrayField(TEXT("allModels"), GraphsArray);
    }

    return VMRoot;
}

TSharedPtr<FJsonObject> FControlRigToJsonExporter::SerializeRigVMGraph(URigVMGraph* InGraph)
{
    if (!InGraph) { return nullptr; }

    TSharedRef<FJsonObject> GraphObj = MakeShared<FJsonObject>();
    GraphObj->SetStringField(TEXT("name"), InGraph->GetName());
    GraphObj->SetStringField(TEXT("graphPath"), InGraph->GetNodePath());

    const TArray<URigVMNode*>& Nodes = InGraph->GetNodes();
    TArray<TSharedPtr<FJsonValue>> NodesArray;

    for (URigVMNode* Node : Nodes)
    {
        if (!Node) { continue; }

        TSharedRef<FJsonObject> NodeObj = MakeShared<FJsonObject>();
        NodeObj->SetStringField(TEXT("name"), Node->GetName());
        NodeObj->SetStringField(TEXT("guid"), Node->GetNodePath());
        NodeObj->SetStringField(TEXT("type"), Node->GetClass()->GetName());
        NodeObj->SetStringField(TEXT("eventName"), Node->GetEventName().ToString());

        // Node title is FText, store as string
        NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle());

        FVector2D Pos = Node->GetPosition();
        NodeObj->SetNumberField(TEXT("x"), Pos.X);
        NodeObj->SetNumberField(TEXT("y"), Pos.Y);

        // Pins
        TArray<TSharedPtr<FJsonValue>> PinsArray;
        for (URigVMPin* Pin : Node->GetPins())
        {
            if (!Pin) { continue; }

            TSharedRef<FJsonObject> PinObj = MakeShared<FJsonObject>();
            PinObj->SetStringField(TEXT("name"), Pin->GetName());
            PinObj->SetStringField(TEXT("pinPath"), Pin->GetPinPath());

            switch (Pin->GetDirection())
            {
            case ERigVMPinDirection::Input:  PinObj->SetStringField(TEXT("direction"), TEXT("input")); break;
            case ERigVMPinDirection::Output: PinObj->SetStringField(TEXT("direction"), TEXT("output")); break;
            default: PinObj->SetStringField(TEXT("direction"), TEXT("io")); break;
            }

            PinObj->SetStringField(TEXT("cppType"), Pin->GetCPPType());
            PinObj->SetStringField(TEXT("friendlyType"),
                ControlRigToJsonPrivate::PinTypeToFriendlyType(Pin->GetCPPType()));
            PinObj->SetBoolField(TEXT("isArray"), Pin->IsArray());
            PinObj->SetBoolField(TEXT("isExecute"), Pin->IsExecuteContext());
            PinObj->SetBoolField(TEXT("isExpanded"), Pin->IsExpanded());

            if (!Pin->GetDefaultValue().IsEmpty())
            {
                PinObj->SetStringField(TEXT("defaultValue"), Pin->GetDefaultValue());
            }

            const TArray<URigVMPin*>& Subs = Pin->GetSubPins();
            if (Subs.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> SubArr;
                for (URigVMPin* Sub : Subs)
                {
                    if (!Sub) { continue; }
                    TSharedRef<FJsonObject> SubObj = MakeShared<FJsonObject>();
                    SubObj->SetStringField(TEXT("name"), Sub->GetName());
                    SubObj->SetStringField(TEXT("cppType"), Sub->GetCPPType());
                    if (!Sub->GetDefaultValue().IsEmpty())
                    {
                        SubObj->SetStringField(TEXT("defaultValue"), Sub->GetDefaultValue());
                    }
                    SubArr.Add(MakeShared<FJsonValueObject>(SubObj));
                }
                PinObj->SetArrayField(TEXT("subPins"), SubArr);
            }

            PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
        }
        NodeObj->SetArrayField(TEXT("pins"), PinsArray);

        // Links
        TArray<TSharedPtr<FJsonValue>> LinksArray;
        for (URigVMLink* Link : Node->GetLinks())
        {
            if (!Link) { continue; }
            TSharedRef<FJsonObject> LinkObj = MakeShared<FJsonObject>();

            if (Link->GetSourcePin())
            {
                LinkObj->SetStringField(TEXT("sourcePin"), Link->GetSourcePin()->GetPinPath());
                if (Link->GetSourcePin()->GetNode())
                {
                    LinkObj->SetStringField(TEXT("sourceNode"), Link->GetSourcePin()->GetNode()->GetName());
                }
            }
            if (Link->GetTargetPin())
            {
                LinkObj->SetStringField(TEXT("targetPin"), Link->GetTargetPin()->GetPinPath());
                if (Link->GetTargetPin()->GetNode())
                {
                    LinkObj->SetStringField(TEXT("targetNode"), Link->GetTargetPin()->GetNode()->GetName());
                }
            }
            LinksArray.Add(MakeShared<FJsonValueObject>(LinkObj));
        }
        NodeObj->SetArrayField(TEXT("links"), LinksArray);

        NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
    }
    GraphObj->SetArrayField(TEXT("nodes"), NodesArray);
    GraphObj->SetNumberField(TEXT("nodeCount"), Nodes.Num());
    return GraphObj;
}

// ============================================================
// Helpers
// ============================================================
bool FControlRigToJsonExporter::WriteControlRigJson(const TSharedRef<FJsonObject>& Object,
    const FString& AssetPackagePath, const FString& OutputDirectory, FString& OutFileName)
{
    OutFileName = ControlRigToJsonPrivate::MakeSafeFileName(AssetPackagePath);
    const FString FullPath = FPaths::Combine(OutputDirectory, OutFileName);

    FString JsonString;
    TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonString);
    FJsonSerializer::Serialize(Object, Writer);

    return FFileHelper::SaveStringToFile(JsonString, *FullPath);
}

void FControlRigToJsonExporter::Notify(const FText& InMessage, bool bIsWarning)
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
    UE_LOG(LogTemp, Log, TEXT("ControlRigToJson: %s"), *InMessage.ToString());
}

#undef LOCTEXT_NAMESPACE
