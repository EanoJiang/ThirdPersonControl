#pragma once

#include "CoreMinimal.h"

class UObject;
struct FAssetData;

/**
 * Skeleton reference-pose -> JSON exporter (singleton).
 *
 * Registers a Content Browser menu entry that exports the reference pose of a
 * Skeleton (USkeleton) or SkeletalMesh (USkeletalMesh) asset to a JSON file.
 *
 * Per bone it records:
 *   - name, index, parent name/index
 *   - LOCAL translation / rotation (quaternion + euler degrees) / scale
 *   - WORLD translation / rotation (quaternion + euler degrees) / scale
 *     (world = accumulated local transforms down the bone chain)
 *
 * Plus an auto-detected left/right bone-pair summary ("lrPairs") that reports,
 * for each matching _l/_r (or L/R / Left/Right) bone pair, whether their WORLD
 * rotations are identical and the per-axis euler deltas. This directly answers
 * "are the left/right legs oriented identically, or mirrored like Mannequin?".
 *
 * Entry point:
 *   - Right-click a Skeleton or SkeletalMesh file -> "Export Skeleton Ref Pose to JSON..."
 */
class FSkeletonRefPoseToJsonExporter
{
public:
    static FSkeletonRefPoseToJsonExporter& Get();

    void Register();
    void Unregister();

    /** Returns true if the asset is a USkeleton or a USkeletalMesh. */
    static bool IsSkeletonAssetClass(const FAssetData& InAsset);

    /** Exports a single skeleton/skeletal-mesh asset; returns the written file name. */
    bool ExportAsset(UObject* InAsset, const FString& InOutputDir, FString& OutFileName);

    static bool PickFolder(const FText& InTitle, const FString& InDefaultPath, FString& OutPath);

private:
    void RegisterMenu();
    void OnExportClicked();

    /** XSJArtTools menu entry point: opens the drag-and-drop export panel. */
    void OpenDropPanel();

    static void Notify(const FText& InMessage, bool bIsWarning);

    /** Resolves the FReferenceSkeleton from either a USkeleton or a USkeletalMesh. */
    static const struct FReferenceSkeleton* ResolveRefSkeleton(UObject* InAsset, FString& OutAssetKind);

    bool bRegisteredMenu = false;
};
