#include "ControlRigToJson.h"
#include "ControlRigToJsonExporter.h"

#define LOCTEXT_NAMESPACE "FControlRigToJsonModule"

void FControlRigToJsonModule::StartupModule()
{
    FControlRigToJsonExporter::Get().Register();
}

void FControlRigToJsonModule::ShutdownModule()
{
    FControlRigToJsonExporter::Get().Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FControlRigToJsonModule, XSJControlRigToJsonEditor)
