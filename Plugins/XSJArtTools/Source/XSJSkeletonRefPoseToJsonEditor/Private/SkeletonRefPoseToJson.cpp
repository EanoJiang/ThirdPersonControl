#include "SkeletonRefPoseToJson.h"
#include "SkeletonRefPoseToJsonExporter.h"

#define LOCTEXT_NAMESPACE "FSkeletonRefPoseToJsonModule"

void FSkeletonRefPoseToJsonModule::StartupModule()
{
    FSkeletonRefPoseToJsonExporter::Get().Register();
}

void FSkeletonRefPoseToJsonModule::ShutdownModule()
{
    FSkeletonRefPoseToJsonExporter::Get().Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSkeletonRefPoseToJsonModule, XSJSkeletonRefPoseToJsonEditor)
