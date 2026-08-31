#pragma once

#include "CoreMinimal.h"

struct FAssetData;
class UObject;
class UToolMenu;
class URigHierarchy;
class URigVMGraph;

/**
 * Control Rig -> JSON exporter (singleton).
 * Registers editor menu entries that export Control Rig assets
 * to LLM-readable JSON files (one per rig).
 *
 * Supports BOTH Control Rig asset kinds:
 *  - UControlRigBlueprint      (authored in the Control Rig editor; has a generated class)
 *  - UControlRigRuntimeAsset   (a saved runtime Control Rig / rig module; no blueprint)
 * Both expose the same underlying URigHierarchy + URigVMGraph data, so the
 * serialization is type-agnostic after a thin resolve step.
 *
 * Two entry points:
 *  - Right-click a Control Rig file -> "Export Control Rig to JSON..."
 *  - Right-click a folder -> "Export Control Rigs to JSON (folder)..."
 */
class FControlRigToJsonExporter
{
public:
    static FControlRigToJsonExporter& Get();

    void Register();
    void Unregister();

    // ---- public helpers for the export panel & other callers ----

    bool GatherControlRigAssets(const FString& InGamePath, TArray<FAssetData>& OutAssets);
    bool ExportSingleAsset(const FAssetData& InAsset, const FString& InOutputDir, TSharedPtr<FJsonObject>& OutEntry, FString& OutFileName);
    void WriteIndexJson(const TArray<TSharedPtr<FJsonObject>>& InEntries, const FString& InGamePath, const FString& InOutputDir);
    static bool ContentFolderToGamePath(const FString& AbsoluteContentFolder, FString& OutGamePath, FString& OutError);
    static bool PickFolder(const FText& InTitle, const FString& InDefaultPath, FString& OutPath);

    /** Returns true if the asset is a known Control Rig kind (blueprint or runtime asset). */
    static bool IsControlRigAssetClass(const FAssetData& InAsset);

private:
    void RegisterMenu();

    /** Folder right-click entry point: shows the batch export panel. */
    void OnExportFolderClicked();

    /** Smart entry point for single-asset right-click or folder menu fallback. */
    void OnExportClicked();

    /** XSJArtTools menu entry point: opens the drag-and-drop export panel. */
    void OpenDropPanel();

    /** Quick single-asset export directly to a chosen directory. InRigAsset may be a UControlRigBlueprint or UControlRigRuntimeAsset. */
    void ExportSingleControlRig(UObject* InRigAsset, const FString& InOutputDir);

    int32 ExportDirectory(const FString& InGamePath, const FString& InOutputDir);

    // ---- unified serialization (works for both Control Rig asset kinds) ----

    TSharedPtr<FJsonObject> SerializeControlRig(UObject* InRigAsset);
    TSharedPtr<FJsonObject> SerializeRigHierarchy(URigHierarchy* InHierarchy);
    TSharedPtr<FJsonObject> SerializeRigVM(UObject* InRigAsset);
    TSharedPtr<FJsonObject> SerializeRigVMGraph(URigVMGraph* InGraph);
    bool WriteControlRigJson(const TSharedRef<FJsonObject>& Object, const FString& AssetPackagePath, const FString& OutputDirectory, FString& OutFileName);
    void Notify(const FText& InMessage, bool bIsWarning);

    bool bRegisteredMenu = false;
};
