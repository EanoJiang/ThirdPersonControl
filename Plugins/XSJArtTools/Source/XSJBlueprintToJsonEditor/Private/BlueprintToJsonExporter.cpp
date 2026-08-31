// 版权所有 Epic Games, Inc. 保留所有权利。

#include "BlueprintToJsonExporter.h"
#include "SBlueprintToJsonExportPanel.h"
#include "SBlueprintToJsonDropPanel.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Styling/AppStyle.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
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
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SWindow.h"
#include "XSJArtToolsCore.h"

#define LOCTEXT_NAMESPACE "BlueprintToJsonExporter"

// 匿名命名空间：导出器内部使用的纯静态辅助函数，不对外暴露。
namespace BlueprintToJsonPrivate
{
	// 把 FProperty 转换为对人类/LLM 友好的类型名，用于变量与参数的 type 字段。
	static FString PropertyToFriendlyType(const FProperty* Property)
	{
		if (!Property)
		{
			return TEXT("unknown");
		}

		// 基础数值与字符串类型，直接返回可读名称。
		if (Property->IsA<FBoolProperty>()) { return TEXT("bool"); }
		if (Property->IsA<FIntProperty>()) { return TEXT("int32"); }
		if (Property->IsA<FInt8Property>()) { return TEXT("int8"); }
		if (Property->IsA<FInt16Property>()) { return TEXT("int16"); }
		if (Property->IsA<FInt64Property>()) { return TEXT("int64"); }
		if (Property->IsA<FUInt16Property>()) { return TEXT("uint16"); }
		if (Property->IsA<FUInt32Property>()) { return TEXT("uint32"); }
		if (Property->IsA<FUInt64Property>()) { return TEXT("uint64"); }
		if (Property->IsA<FFloatProperty>()) { return TEXT("float"); }
		if (Property->IsA<FDoubleProperty>()) { return TEXT("double"); }
		if (Property->IsA<FByteProperty>()) { return TEXT("byte"); }
		if (Property->IsA<FNameProperty>()) { return TEXT("name"); }
		if (Property->IsA<FStrProperty>()) { return TEXT("string"); }
		if (Property->IsA<FTextProperty>()) { return TEXT("text"); }

		// 枚举类型：返回枚举名。
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			return EnumProperty->GetEnum() ? EnumProperty->GetEnum()->GetName() : TEXT("enum");
		}

		// 结构体类型：返回结构体名。
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			return StructProperty->Struct ? StructProperty->Struct->GetName() : TEXT("struct");
		}

		// 对象引用类型：返回被引用对象的类名。
		if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			return ObjectProperty->PropertyClass ? ObjectProperty->PropertyClass->GetName() : TEXT("object");
		}

		// 容器类型：递归取元素类型并附加容器标记。
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			return PropertyToFriendlyType(ArrayProperty->Inner) + TEXT("[]");
		}
		if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))
		{
			return TEXT("set<") + PropertyToFriendlyType(SetProperty->ElementProp) + TEXT(">");
		}
		if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
		{
			return TEXT("map<") + PropertyToFriendlyType(MapProperty->KeyProp) + TEXT(",") + PropertyToFriendlyType(MapProperty->ValueProp) + TEXT(">");
		}

		// 单播委托。
		if (Property->IsA<FDelegateProperty>())
		{
			return TEXT("delegate");
		}

		// 多播委托：事件分发器（Event Dispatcher）是可被蓝图赋值的多播委托，据此区分。
		if (const FMulticastDelegateProperty* MulticastProperty = CastField<FMulticastDelegateProperty>(Property))
		{
			return MulticastProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable) ? TEXT("eventdispatcher") : TEXT("multicastdelegate");
		}

		// 兜底：返回属性所属类的名称。
		return Property->GetClass()->GetName();
	}

	// 把属性标志位（EPropertyFlags）翻译为可读关键词列表，写入变量的 flags 字段。
	static void AppendPropertyFlags(uint64 PropertyFlags, TArray<FString>& OutFlags)
	{
		if (PropertyFlags & CPF_Edit) { OutFlags.Add(TEXT("EditAnywhere")); }
		if (PropertyFlags & CPF_BlueprintVisible) { OutFlags.Add(TEXT("BlueprintVisible")); }
		if (PropertyFlags & CPF_BlueprintReadOnly) { OutFlags.Add(TEXT("BlueprintReadOnly")); }
		if (PropertyFlags & CPF_Net) { OutFlags.Add(TEXT("Replicated")); }
		if (PropertyFlags & CPF_RepNotify) { OutFlags.Add(TEXT("RepNotify")); }
		if (PropertyFlags & CPF_ExposeOnSpawn) { OutFlags.Add(TEXT("ExposeOnSpawn")); }
		if (PropertyFlags & CPF_DisableEditOnTemplate) { OutFlags.Add(TEXT("DisableEditOnTemplate")); }
		if (PropertyFlags & CPF_DisableEditOnInstance) { OutFlags.Add(TEXT("DisableEditOnInstance")); }
		if (PropertyFlags & CPF_SaveGame) { OutFlags.Add(TEXT("SaveGame")); }
		if (PropertyFlags & CPF_Transient) { OutFlags.Add(TEXT("Transient")); }
	}

	// 把函数标志位（EFunctionFlags）翻译为可读关键词列表，写入函数的 flags 字段。
	static void AppendFunctionFlags(uint64 FunctionFlags, TArray<FString>& OutFlags)
	{
		if (FunctionFlags & FUNC_BlueprintCallable) { OutFlags.Add(TEXT("BlueprintCallable")); }
		if (FunctionFlags & FUNC_BlueprintPure) { OutFlags.Add(TEXT("BlueprintPure")); }
		if (FunctionFlags & FUNC_Net) { OutFlags.Add(TEXT("Net")); }
		if (FunctionFlags & FUNC_NetRequest) { OutFlags.Add(TEXT("NetRequest")); }
		if (FunctionFlags & FUNC_BlueprintAuthorityOnly) { OutFlags.Add(TEXT("AuthorityOnly")); }
		if (FunctionFlags & FUNC_BlueprintCosmetic) { OutFlags.Add(TEXT("Cosmetic")); }
	}

	// 把包路径（如 "/Game/Characters/BP_Enemy.BP_Enemy"）转换为安全的文件名（去掉非法字符）。
	static FString MakeSafeFileName(const FString& AssetPackagePath)
	{
		FString Safe = AssetPackagePath;
		Safe.ReplaceInline(TEXT("/"), TEXT("_"));
		Safe.ReplaceInline(TEXT("\\"), TEXT("_"));
		Safe.ReplaceInline(TEXT("."), TEXT("_"));
		Safe.ReplaceInline(TEXT(":"), TEXT("_"));
		return Safe + TEXT(".json");
	}

	// 类路径转可读字符串：内置类型（/Script/Engine.*）去掉前缀只留类名，其余返回完整路径。
	static FString ClassPathToString(UClass* Class)
	{
		if (!Class)
		{
			return TEXT("");
		}

		const FString PathName = Class->GetPathName();
		// 内置类型去掉 "/Script/Engine." 前缀，使输出更简洁可读。
		static const FString ScriptEnginePrefix(TEXT("/Script/Engine."));
		if (PathName.StartsWith(ScriptEnginePrefix))
		{
			return PathName.RightChop(ScriptEnginePrefix.Len());
		}
		return PathName;
	}

	// 把蓝图引脚类型（FEdGraphPinType）转换为友好的类型名，用于节点 pins 数组的 type 字段。
	// 与 PropertyToFriendlyType 区别：这里处理的是图引脚的类别（PinCategory），不是反射属性。
	static FString PinTypeToFriendlyType(const FEdGraphPinType& PinType)
	{
		const FName Category = PinType.PinCategory;

		// 标量类别。
		if (Category == UEdGraphSchema_K2::PC_Boolean) { return TEXT("bool"); }
		if (Category == UEdGraphSchema_K2::PC_Byte) { return TEXT("byte"); }
		if (Category == UEdGraphSchema_K2::PC_Int) { return TEXT("int32"); }
		if (Category == UEdGraphSchema_K2::PC_Int64) { return TEXT("int64"); }
		if (Category == UEdGraphSchema_K2::PC_Float) { return TEXT("float"); }
		if (Category == UEdGraphSchema_K2::PC_Double) { return TEXT("double"); }
		if (Category == UEdGraphSchema_K2::PC_Name) { return TEXT("name"); }
		if (Category == UEdGraphSchema_K2::PC_String) { return TEXT("string"); }
		if (Category == UEdGraphSchema_K2::PC_Text) { return TEXT("text"); }

		// 特殊类别。
		if (Category == UEdGraphSchema_K2::PC_Exec) { return TEXT("exec"); }
		if (Category == UEdGraphSchema_K2::PC_Wildcard) { return TEXT("wildcard"); }
		if (Category == UEdGraphSchema_K2::PC_Delegate) { return TEXT("delegate"); }
		if (Category == UEdGraphSchema_K2::PC_Interface) { return TEXT("interface"); }

		// 类引用类型：附带具体类名。
		if (Category == UEdGraphSchema_K2::PC_Class)
		{
			if (UClass* Class = Cast<UClass>(PinType.PinSubCategoryObject.Get()))
			{
				return TEXT("class<") + ClassPathToString(Class) + TEXT(">");
			}
			return TEXT("class");
		}

		// 结构体引用类型：返回结构体名。
		if (Category == UEdGraphSchema_K2::PC_Struct)
		{
			if (UStruct* Struct = Cast<UStruct>(PinType.PinSubCategoryObject.Get()))
			{
				return Struct->GetName();
			}
			return TEXT("struct");
		}

		// 对象引用类型：返回对象所属类名。
		if (Category == UEdGraphSchema_K2::PC_Object)
		{
			if (UClass* Class = Cast<UClass>(PinType.PinSubCategoryObject.Get()))
			{
				return ClassPathToString(Class);
			}
			return TEXT("object");
		}

		// 枚举引用类型：返回枚举名。
		if (Category == UEdGraphSchema_K2::PC_Enum)
		{
			if (UEnum* Enum = Cast<UEnum>(PinType.PinSubCategoryObject.Get()))
			{
				return Enum->GetName();
			}
			return TEXT("enum");
		}

		// 兜底：直接返回类别名。
		return Category.IsValid() ? Category.ToString() : TEXT("unknown");
	}
}

FBlueprintToJsonExporter& FBlueprintToJsonExporter::Get()
{
	// 单例：导出器全局唯一实例，由模块在启动/关闭时注册/注销菜单。
	static FBlueprintToJsonExporter Instance;
	return Instance;
}

void FBlueprintToJsonExporter::Register()
{
	// 已注册则跳过，避免重复。
	if (bRegisteredMenu)
	{
		return;
	}

	// ToolMenus 尚未就绪时无法注册，直接返回。
	if (!UToolMenus::Get())
	{
		return;
	}

	// 注册一个编辑器启动回调，等编辑器菜单系统就绪后再挂载菜单项。
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBlueprintToJsonExporter::RegisterMenu));
}

void FBlueprintToJsonExporter::Unregister()
{
	// 注销启动回调，清理菜单注册。
	if (UToolMenus::Get() && bRegisteredMenu)
	{
		UToolMenus::UnRegisterStartupCallback(this);
		bRegisteredMenu = false;
	}
}

void FBlueprintToJsonExporter::RegisterMenu()
{
	UToolMenu* BlueprintToJsonMenu = UToolMenus::Get()->ExtendMenu(FXSJArtToolsMenuNames::BlueprintToJson);
	if (BlueprintToJsonMenu == nullptr)
	{
		return;
	}

	// // 在主菜单栏新增一个顶层 "BluePrintTool" 菜单，与 File/Edit/Window 并列。
	// UToolMenu* MenuBar = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu");
	// if (!MenuBar)
	// {
	// 	return;
	// }

	// FToolMenuSection& MenuBarSection = MenuBar->FindOrAddSection(NAME_None);

	// // 添加顶层菜单项，并定位到 XSJArtTools 菜单（其内部名为 "PythonSubMenu"，
	// // 由 XsjTempEditorTools 插件注册）的右侧。启动回调每次编辑器会话只执行一次，
	// // 但热重载可能再次触发，因此先检查菜单栏是否已存在本条目，避免重复添加。
	// static const FName BlueprintToolEntryName(TEXT("BluePrintTool"));
	// const bool bAlreadyOnBar = MenuBarSection.FindEntry(BlueprintToolEntryName) != nullptr;
	// if (!bAlreadyOnBar)
	// {
	// 	FToolMenuEntry& TopEntry = MenuBarSection.AddSubMenu(
	// 		BlueprintToolEntryName,
	// 		LOCTEXT("BluePrintToolMenu", "BluePrintTool"),
	// 		LOCTEXT("BluePrintToolMenu_ToolTip", "Blueprint authoring tools"),
	// 		FNewToolMenuChoice()
	// 	);
	// 	// 定位到 XSJArtTools 菜单（"PythonSubMenu"）之后。
	// 	TopEntry.InsertPosition = FToolMenuInsert(TEXT("PythonSubMenu"), EToolMenuInsertType::After);
	// }

	// // 注册并填充顶层菜单的内容。
	// UToolMenu* ToolMenu = UToolMenus::Get()->RegisterMenu(
	// 	"LevelEditor.MainMenu.BluePrintTool",
	// 	NAME_None,
	// 	EMultiBoxType::Menu,
	// 	/*bWarnIfAlreadyRegistered*/ false);
	// if (!ToolMenu)
	// {
	// 	return;
	// }

	FToolMenuSection& Section = BlueprintToJsonMenu->FindOrAddSection("BlueprintToJsonTools", LOCTEXT("BlueprintToJsonSection", "Blueprint To JSON"));
	// 1) 主菜单：打开拖入导出面板（与其它 ToJson 插件一致）。
	Section.AddMenuEntry(
		"BlueprintToJson.OpenDropPanel",
		LOCTEXT("OpenDropPanelLabel", "Export Blueprints to JSON (drag & drop)..."),
		LOCTEXT("OpenDropPanelTooltip", "Open a panel where you can drag Blueprint assets in and export them to JSON files."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FBlueprintToJsonExporter::OpenDropPanel))
	);

	// 2) 文件夹右键：批量按文件夹导出（旧面板流程）。
	if (UToolMenu* FolderMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.FolderContextMenu"))
	{
		FToolMenuSection& FolderSection = FolderMenu->FindOrAddSection("XSJArtTools");
		FolderSection.AddMenuEntry(
			"BlueprintToJson.ExportFolder",
			LOCTEXT("ExportFolder", "Export Blueprints to JSON (folder)..."),
			LOCTEXT("ExportFolderTooltip", "Batch-export every Blueprint asset under the selected content folder to JSON files."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Blueprint"),
			FUIAction(FExecuteAction::CreateRaw(this, &FBlueprintToJsonExporter::OnExportClicked)));
	}

	// 3) 资产右键：导出当前选中的单个蓝图。
	if (UToolMenu* AssetMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu"))
	{
		FToolMenuSection& AssetSection = AssetMenu->FindOrAddSection("XSJArtTools");
		AssetSection.AddDynamicEntry("XSJBlueprintToJson.ExportSelected", FNewToolMenuSectionDelegate::CreateLambda(
			[this](FToolMenuSection& InSection)
			{
				TArray<FAssetData> SelectedAssets;
				GEditor->GetContentBrowserSelections(SelectedAssets);

				bool bHasBP = false;
				for (const FAssetData& Asset : SelectedAssets)
				{
					if (IsAssetClass(Asset))
					{
						bHasBP = true;
						break;
					}
				}

				if (!bHasBP) { return; }

				InSection.AddMenuEntry(
					"XSJBlueprintToJson.ExportSelected",
					LOCTEXT("ExportSelectedBP", "Export Selected Blueprint to JSON..."),
					LOCTEXT("ExportSelectedBPTooltip", "Export the currently selected Blueprint asset to a JSON file."),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Blueprint"),
					FUIAction(FExecuteAction::CreateRaw(this, &FBlueprintToJsonExporter::OnExportSelectedClicked)));
			}));
	}

	bRegisteredMenu = true;
}

bool FBlueprintToJsonExporter::PickFolder(const FText& InTitle, const FString& InDefaultPath, FString& OutPath)
{
	// 获取桌面平台接口，用于弹出原生文件夹选择对话框。
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		UE_LOG(LogTemp, Warning, TEXT("BlueprintToJson: 当前构建没有桌面平台支持，无法打开文件夹对话框。"));
		return false;
	}

	// 取当前最顶层活动窗口的原生句柄作为对话框父窗口，使对话框模态挂在编辑器上。
	const void* ParentWindowHandle = nullptr;
	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
		if (ActiveWindow.IsValid())
		{
			ParentWindowHandle = ActiveWindow->GetNativeWindow()->GetOSWindowHandle();
		}
	}

	FString OutFolderName;
	const bool bSuccess = DesktopPlatform->OpenDirectoryDialog(
		ParentWindowHandle,
		InTitle.ToString(),
		InDefaultPath,
		OutFolderName);

	// 用户取消或未选择则返回失败。
	if (!bSuccess || OutFolderName.IsEmpty())
	{
		return false;
	}

	OutPath = OutFolderName;
	FPaths::NormalizeDirectoryName(OutPath);
	return true;
}

bool FBlueprintToJsonExporter::IsAssetClass(const FAssetData& InAsset)
{
	// 蓝图资产在内容浏览器中其 AssetClassPath 通常是 UBlueprint 本身。先做最精确的
	// 类路径比较以覆盖主要路径，避免触发资产加载。
	if (InAsset.AssetClassPath == UBlueprint::StaticClass()->GetClassPathName())
	{
		return true;
	}

	// 兜底：加载资产的真实类，判断是否为 UBlueprint 的子类，以覆盖蓝图的子类资产。
	if (UClass* RealClass = InAsset.GetClass())
	{
		return RealClass->IsChildOf(UBlueprint::StaticClass());
	}
	return false;
}

void FBlueprintToJsonExporter::OnExportClicked()
{
	// 菜单点击：弹出导出面板窗口（而非连续弹两次文件夹对话框）。
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	const TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("ExportPanelTitle", "Export Blueprints to JSON"))
		.ClientSize(FVector2D(560.0f, 300.0f))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::FixedSize);

	Window->SetContent(SNew(SBlueprintToJsonExportPanel));

	FSlateApplication::Get().AddWindow(Window);
}

void FBlueprintToJsonExporter::OpenDropPanel()
{
	// 打开拖入导出面板：把蓝图资产拖入，列表显示，点击导出再选择输出目录。
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	const TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("DropPanelTitle", "Export Blueprints to JSON (drag & drop)"))
		.ClientSize(FVector2D(560.0f, 420.0f))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::FixedSize);

	Window->SetContent(SNew(SBlueprintToJsonDropPanel));

	FSlateApplication::Get().AddWindow(Window);
}

void FBlueprintToJsonExporter::OnExportSelectedClicked()
{
	// 把当前在内容浏览器中选中的单个蓝图资产导出到所选目录。
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	// 收集当前选中的资产，找到第一个蓝图资产。
	TArray<FAssetData> SelectedAssets;
	GEditor->GetContentBrowserSelections(SelectedAssets);

	FAssetData SelectedBlueprint;
	for (const FAssetData& Asset : SelectedAssets)
	{
		if (IsAssetClass(Asset))
		{
			SelectedBlueprint = Asset;
			break;
		}
	}
	if (!SelectedBlueprint.IsValid())
	{
		Notify(LOCTEXT("NoBlueprintSelected", "当前没有选中任何蓝图资产。"), true);
		return;
	}

	// 选择输出目录。
	FString OutputDir;
	const FString DefaultPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	if (!PickFolder(LOCTEXT("PickOutputFolder", "选择导出目录"), DefaultPath, OutputDir))
	{
		return;
	}

	TSharedPtr<FJsonObject> Entry;
	FString FileName;
	if (ExportSingleAsset(SelectedBlueprint, OutputDir, Entry, FileName) && Entry.IsValid())
	{
		TArray<TSharedPtr<FJsonObject>> Entries;
		Entries.Add(Entry);
		WriteIndexJson(Entries, FString(), OutputDir);
		Notify(FText::Format(LOCTEXT("ExportSingleDone", "导出完成：{0}"), FText::FromString(SelectedBlueprint.GetObjectPathString())), false);
	}
	else
	{
		Notify(LOCTEXT("ExportSingleFailed", "导出失败。"), true);
	}
}

bool FBlueprintToJsonExporter::ContentFolderToGamePath(const FString& AbsoluteContentFolder, FString& OutGamePath, FString& OutError)
{
	// 规范化输入路径：去首尾空白、反斜杠转正斜杠、去掉末尾多余的斜杠。
	FString Normalized = AbsoluteContentFolder;
	Normalized.TrimStartAndEndInline();
	Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
	while (Normalized.Len() > 1 && Normalized.EndsWith(TEXT("/")))
	{
		Normalized.LeftChopInline(1, EAllowShrinking::No);
	}

	// 取工程 Content 目录的绝对路径并同样规范化，用于做前缀比对。
	const FString ContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	FString NormalizedContentDir = ContentDir;
	NormalizedContentDir.ReplaceInline(TEXT("\\"), TEXT("/"));
	while (NormalizedContentDir.Len() > 1 && NormalizedContentDir.EndsWith(TEXT("/")))
	{
		NormalizedContentDir.LeftChopInline(1, EAllowShrinking::No);
	}

	// 输入必须位于 Content 目录之下，否则无法映射到 /Game/... 包路径。
	if (!Normalized.StartsWith(NormalizedContentDir))
	{
		OutError = FString::Printf(
			TEXT("所选目录必须位于工程 Content 目录内：\n%s\n你选择的是：\n%s"),
			*NormalizedContentDir, *Normalized);
		return false;
	}

	// 截取相对 Content 的子路径，拼成 /Game/... 形式。
	FString Relative = Normalized.RightChop(NormalizedContentDir.Len());
	Relative.RemoveFromStart(TEXT("/"));
	if (Relative.IsEmpty())
	{
		OutGamePath = TEXT("/Game");
	}
	else
	{
		OutGamePath = TEXT("/Game/") + Relative;
	}
	return true;
}

int32 FBlueprintToJsonExporter::ExportDirectory(const FString& InGamePath, const FString& InOutputDir)
{
	// 同步批量导出入口（面板用的是分帧版本，此函数保留给其他同步调用方）。
	TArray<FAssetData> AssetData;
	if (!GatherBlueprintAssets(InGamePath, AssetData))
	{
		// 资产注册表仍在加载，GatherBlueprintAssets 返回 false，这里给出提示。
		Notify(LOCTEXT("AssetRegistryLoading", "Asset Registry is still discovering assets. Please wait a moment and try again."), true);
		return -1;
	}

	if (AssetData.Num() == 0)
	{
		return 0;
	}

	// 带取消按钮的进度对话框（同步版本，会阻塞 UI）。
	FScopedSlowTask SlowTask(AssetData.Num(), LOCTEXT("ExportingBlueprints", "Exporting blueprints to JSON..."));
	SlowTask.MakeDialog(true);

	int32 ExportedCount = 0;
	TArray<TSharedPtr<FJsonObject>> IndexEntries;

	for (const FAssetData& Data : AssetData)
	{
		if (SlowTask.ShouldCancel())
		{
			break;
		}

		SlowTask.EnterProgressFrame(1, FText::FromString(Data.ToSoftObjectPath().ToString()));

		// 逐个资产导出，成功则累积到索引列表。
		TSharedPtr<FJsonObject> Entry;
		FString FileName;
		if (ExportSingleAsset(Data, InOutputDir, Entry, FileName))
		{
			IndexEntries.Add(Entry);
			++ExportedCount;
		}
	}

	// 全部导出后写汇总索引文件。
	WriteIndexJson(IndexEntries, InGamePath, InOutputDir);
	return ExportedCount;
}

bool FBlueprintToJsonExporter::GatherBlueprintAssets(const FString& InGamePath, TArray<FAssetData>& OutAssets)
{
	// 通过资产注册表递归收集指定 /Game/... 路径下的所有蓝图资产。
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	if (AssetRegistry.IsLoadingAssets())
	{
		// 资产注册表仍在后台发现资产，此时查询结果不完整，返回 false 让调用方稍后重试。
		return false;
	}

	FARFilter Filter;
	Filter.PackagePaths.Add(*InGamePath);
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;

	AssetRegistry.GetAssets(Filter, OutAssets);
	return true;
}

bool FBlueprintToJsonExporter::ExportSingleAsset(const FAssetData& InAsset, const FString& InOutputDir, TSharedPtr<FJsonObject>& OutEntry, FString& OutFileName)
{
	// 导出单个蓝图资产：加载 → 序列化为 JSON → 写盘 → 生成索引条目。
	OutEntry.Reset();
	OutFileName.Reset();

	const FString AssetObjectPath = InAsset.ToSoftObjectPath().ToString();

	// 加载蓝图对象（注意：蓝图资产是 UBlueprint，而非其生成的类）。
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetObjectPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Warning, TEXT("BlueprintToJson: 加载蓝图失败 %s"), *AssetObjectPath);
		return false;
	}

	// 序列化为 JSON 对象。
	TSharedPtr<FJsonObject> Json = SerializeBlueprint(Blueprint);
	if (!Json.IsValid())
	{
		return false;
	}

	// 写入磁盘文件。
	if (!WriteBlueprintJson(Json.ToSharedRef(), AssetObjectPath, InOutputDir, OutFileName))
	{
		UE_LOG(LogTemp, Warning, TEXT("BlueprintToJson: 写出 JSON 失败 %s"), *AssetObjectPath);
		return false;
	}

	// 构造索引条目，供最终 index.json 汇总使用。
	TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
	Entry->SetStringField(TEXT("name"), Blueprint->GetName());
	Entry->SetStringField(TEXT("assetPath"), AssetObjectPath);
	Entry->SetStringField(TEXT("file"), OutFileName);
	OutEntry = Entry;
	return true;
}

void FBlueprintToJsonExporter::WriteIndexJson(const TArray<TSharedPtr<FJsonObject>>& InEntries, const FString& InGamePath, const FString& InOutputDir)
{
	// 将所有已导出蓝图的索引汇总写入 index.json，便于整体检索。
	TSharedRef<FJsonObject> IndexObject = MakeShared<FJsonObject>();
	IndexObject->SetNumberField(TEXT("count"), InEntries.Num());
	IndexObject->SetStringField(TEXT("inputPath"), InGamePath);

	TArray<TSharedPtr<FJsonValue>> IndexArray;
	for (const TSharedPtr<FJsonObject>& Entry : InEntries)
	{
		if (Entry.IsValid())
		{
			IndexArray.Add(MakeShared<FJsonValueObject>(Entry));
		}
	}
	IndexObject->SetArrayField(TEXT("blueprints"), IndexArray);

	// 序列化为带缩进的 pretty JSON 字符串。
	FString IndexJsonString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> IndexWriter = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&IndexJsonString);
	FJsonSerializer::Serialize(IndexObject, IndexWriter);

	const FString IndexPath = FPaths::Combine(InOutputDir, TEXT("index.json"));
	if (!FFileHelper::SaveStringToFile(IndexJsonString, *IndexPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("BlueprintToJson: 写出 index.json 失败 %s"), *IndexPath);
	}
}

TSharedPtr<FJsonObject> FBlueprintToJsonExporter::SerializeBlueprint(UBlueprint* InBlueprint)
{
	// 将单个蓝图完整序列化为 JSON 对象（变量/函数/事件分发器/组件/各图）。
	if (!InBlueprint || !InBlueprint->GeneratedClass)
	{
		return nullptr;
	}

	UClass* GeneratedClass = InBlueprint->GeneratedClass;

	// 类默认对象（CDO）：持有蓝图变量的真实默认值。
	// 注意 FBPVariableDescription::DefaultValue 只是一个可选字符串，常常为空，
	// 因此默认值需从 CDO 读取。
	UObject* CDO = GeneratedClass->GetDefaultObject(/*bCreate*/ true);

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("name"), InBlueprint->GetName());
	Root->SetStringField(TEXT("assetPath"), InBlueprint->GetPathName());
	Root->SetStringField(TEXT("packagePath"), InBlueprint->GetOutermost() ? InBlueprint->GetOutermost()->GetName() : TEXT(""));
	Root->SetStringField(TEXT("parentClass"), BlueprintToJsonPrivate::ClassPathToString(GeneratedClass->GetSuperClass()));

	// 该生成类实现的接口列表。
	TArray<TSharedPtr<FJsonValue>> InterfacesArray;
	for (const FImplementedInterface& Implemented : GeneratedClass->Interfaces)
	{
		if (Implemented.Class)
		{
			InterfacesArray.Add(MakeShared<FJsonValueString>(Implemented.Class->GetPathName()));
		}
	}
	Root->SetArrayField(TEXT("interfaces"), InterfacesArray);

	// 变量：遍历生成类的属性，但只保留本蓝图定义的（跳过继承来的），通过比较 owner 类判断。
	TArray<TSharedPtr<FJsonValue>> VariablesArray;
	for (TFieldIterator<FProperty> It(GeneratedClass); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property || Property->GetOwnerClass() != GeneratedClass)
		{
			continue;
		}

		TSharedRef<FJsonObject> Var = MakeShared<FJsonObject>();
		Var->SetStringField(TEXT("name"), Property->GetName());
		Var->SetStringField(TEXT("type"), BlueprintToJsonPrivate::PropertyToFriendlyType(Property));
		Var->SetStringField(TEXT("category"), Property->GetMetaDataMap() ? Property->GetMetaDataMap()->FindRef(TEXT("Category")) : TEXT(""));

		// 在 NewVariables 中查找同名条目，用于取 tooltip 和可选的字符串默认值。
		const FBPVariableDescription* FoundDesc = nullptr;
		for (const FBPVariableDescription& Desc : InBlueprint->NewVariables)
		{
			if (Desc.VarName == Property->GetFName())
			{
				FoundDesc = &Desc;
				break;
			}
		}

		// Tooltip：蓝图变量的提示文字存在 "tooltip" 元数据键下。
		FString Tooltip;
		if (FoundDesc && FoundDesc->HasMetaData(FBlueprintMetadata::MD_Tooltip))
		{
			Tooltip = FoundDesc->GetMetaData(FBlueprintMetadata::MD_Tooltip);
		}
		Var->SetStringField(TEXT("tooltip"), Tooltip);

		// 默认值：优先用 CDO 的真实值（覆盖数值/FText/String 等字符串字段读不到的情况），
		// CDO 读不到时再回退到可选字符串默认值。
		FString DefaultValue;
		if (CDO)
		{
			Property->ExportText_InContainer(0, DefaultValue, CDO, /*Delta*/ nullptr, CDO, PPF_None);
		}
		if (DefaultValue.IsEmpty() && FoundDesc)
		{
			DefaultValue = FoundDesc->DefaultValue;
		}
		Var->SetStringField(TEXT("default"), DefaultValue);

		// 若变量是枚举类型，导出枚举项及其显示名。
		UEnum* Enum = nullptr;
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			Enum = EnumProperty->GetEnum();
		}
		else if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			Enum = ByteProperty->Enum;
		}
		if (Enum)
		{
			TArray<TSharedPtr<FJsonValue>> EnumValuesArray;
			const int32 MaxEnum = Enum->NumEnums();
			for (int32 EnumIndex = 0; EnumIndex < MaxEnum; ++EnumIndex)
			{
				// 跳过原生枚举末尾隐式的 "_MAX" 哨兵项。
				FString EntryName = Enum->GetNameByIndex(EnumIndex).ToString();
				if (EntryName.EndsWith(TEXT("_MAX")))
				{
					continue;
				}

				TSharedRef<FJsonObject> EnumEntry = MakeShared<FJsonObject>();
				EnumEntry->SetStringField(TEXT("name"), EntryName);
				EnumEntry->SetStringField(TEXT("displayName"), Enum->GetDisplayNameTextByIndex(EnumIndex).ToString());
				EnumValuesArray.Add(MakeShared<FJsonValueObject>(EnumEntry));
			}
			Var->SetArrayField(TEXT("enumValues"), EnumValuesArray);
		}

		// 属性标志位翻译为关键词数组。
		TArray<TSharedPtr<FJsonValue>> FlagsArray;
		TArray<FString> Flags;
		BlueprintToJsonPrivate::AppendPropertyFlags(Property->GetPropertyFlags(), Flags);
		for (const FString& Flag : Flags)
		{
			FlagsArray.Add(MakeShared<FJsonValueString>(Flag));
		}
		Var->SetArrayField(TEXT("flags"), FlagsArray);

		VariablesArray.Add(MakeShared<FJsonValueObject>(Var));
	}
	Root->SetArrayField(TEXT("variables"), VariablesArray);

	// 函数：只保留本生成类拥有的（在本蓝图定义或覆写的），跳过继承来的。
	TArray<TSharedPtr<FJsonValue>> FunctionsArray;
	for (TFieldIterator<UFunction> It(GeneratedClass); It; ++It)
	{
		UFunction* Function = *It;
		if (!Function || Function->GetOwnerClass() != GeneratedClass)
		{
			continue;
		}

		TSharedRef<FJsonObject> Func = MakeShared<FJsonObject>();
		Func->SetStringField(TEXT("name"), Function->GetName());

		// 函数的 tooltip/说明：蓝图函数的提示文字同样存在 "tooltip" 元数据键下（与变量同键）。
		if (Function->HasMetaData(FBlueprintMetadata::MD_Tooltip))
		{
			Func->SetStringField(TEXT("tooltip"), Function->GetMetaData(FBlueprintMetadata::MD_Tooltip));
		}

		// 遍历函数的参数与返回值属性。
		TArray<TSharedPtr<FJsonValue>> ParamsArray;
		TArray<TSharedPtr<FJsonValue>> ReturnArray;
		for (TFieldIterator<FProperty> ParamIt(Function); ParamIt; ++ParamIt)
		{
			FProperty* Param = *ParamIt;
			if (!Param)
			{
				continue;
			}

			TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
			P->SetStringField(TEXT("name"), Param->GetName());
			P->SetStringField(TEXT("type"), BlueprintToJsonPrivate::PropertyToFriendlyType(Param));

			// 带返回值标志的归入返回值，其余归入参数。
			if (Param->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				ReturnArray.Add(MakeShared<FJsonValueObject>(P));
			}
			else
			{
				ParamsArray.Add(MakeShared<FJsonValueObject>(P));
			}
		}
		Func->SetArrayField(TEXT("params"), ParamsArray);

		// 返回值类型列表（只取类型字符串）。
		TArray<TSharedPtr<FJsonValue>> ReturnTypesArray;
		for (const TSharedPtr<FJsonValue>& Ret : ReturnArray)
		{
			ReturnTypesArray.Add(MakeShared<FJsonValueString>(Ret->AsObject()->GetStringField(TEXT("type"))));
		}
		Func->SetArrayField(TEXT("returnTypes"), ReturnTypesArray);

		// 函数标志位翻译为关键词数组。
		TArray<TSharedPtr<FJsonValue>> FuncFlagsArray;
		TArray<FString> FuncFlags;
		BlueprintToJsonPrivate::AppendFunctionFlags(Function->FunctionFlags, FuncFlags);
		for (const FString& Flag : FuncFlags)
		{
			FuncFlagsArray.Add(MakeShared<FJsonValueString>(Flag));
		}
		Func->SetArrayField(TEXT("flags"), FuncFlagsArray);

		FunctionsArray.Add(MakeShared<FJsonValueObject>(Func));
	}
	Root->SetArrayField(TEXT("functions"), FunctionsArray);

	// 事件分发器：本类定义的、可被蓝图赋值的多播委托属性（带 CPF_BlueprintAssignable 标志）。
	TArray<TSharedPtr<FJsonValue>> DispatcherArray;
	for (TFieldIterator<FMulticastDelegateProperty> It(GeneratedClass); It; ++It)
	{
		FMulticastDelegateProperty* DelegateProperty = *It;
		if (!DelegateProperty || DelegateProperty->GetOwnerClass() != GeneratedClass)
		{
			continue;
		}
		if (DelegateProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable))
		{
			DispatcherArray.Add(MakeShared<FJsonValueString>(DelegateProperty->GetName()));
		}
	}
	Root->SetArrayField(TEXT("eventDispatchers"), DispatcherArray);

	// 组件：来自简单构造脚本（SimpleConstructionScript），仅 Actor 类蓝图才有。
	TArray<TSharedPtr<FJsonValue>> ComponentsArray;
	if (InBlueprint->SimpleConstructionScript)
	{
		const TArray<USCS_Node*>& Nodes = InBlueprint->SimpleConstructionScript->GetAllNodes();
		for (USCS_Node* Node : Nodes)
		{
			if (!Node)
			{
				continue;
			}

			TSharedRef<FJsonObject> Comp = MakeShared<FJsonObject>();
			Comp->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
			Comp->SetStringField(TEXT("type"), Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT(""));
			ComponentsArray.Add(MakeShared<FJsonValueObject>(Comp));
		}
	}
	Root->SetArrayField(TEXT("components"), ComponentsArray);

	// 图：事件图、函数图、宏图（5.8 没有独立的事件分发器图）。
	TArray<TSharedPtr<FJsonValue>> GraphsArray;
	auto AppendGraph = [&](UEdGraph* Graph)
	{
		if (Graph)
		{
			if (TSharedPtr<FJsonObject> GraphJson = SerializeGraph(Graph))
			{
				GraphsArray.Add(MakeShared<FJsonValueObject>(GraphJson.ToSharedRef()));
			}
		}
	};

	for (UEdGraph* Graph : InBlueprint->UbergraphPages)	// 事件图
	{
		AppendGraph(Graph);
	}
	for (UEdGraph* Graph : InBlueprint->FunctionGraphs)	// 函数图
	{
		AppendGraph(Graph);
	}
	for (UEdGraph* Graph : InBlueprint->MacroGraphs)		// 宏图
	{
		AppendGraph(Graph);
	}
	Root->SetArrayField(TEXT("graphs"), GraphsArray);

	return Root;
}

TSharedPtr<FJsonObject> FBlueprintToJsonExporter::SerializeGraph(UEdGraph* InGraph)
{
	// 将单个图序列化为 JSON：图名 + 节点数组（每个节点含 pins 与 links）。
	if (!InGraph)
	{
		return nullptr;
	}

	TSharedRef<FJsonObject> GraphObject = MakeShared<FJsonObject>();
	GraphObject->SetStringField(TEXT("name"), InGraph->GetName());

	TArray<TSharedPtr<FJsonValue>> NodesArray;
	for (UEdGraphNode* Node : InGraph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		TSharedRef<FJsonObject> NodeObject = MakeShared<FJsonObject>();
		// 稳定唯一标识：作为 links 的引用键（节点标题不唯一，不能作引用键）。
		NodeObject->SetStringField(TEXT("nodeGuid"), Node->NodeGuid.ToString());
		const FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		NodeObject->SetStringField(TEXT("title"), NodeTitle);
		NodeObject->SetStringField(TEXT("type"), Node->GetClass() ? Node->GetClass()->GetName() : TEXT(""));
		// 节点坐标，可用于还原空间/执行流的顺序。
		NodeObject->SetNumberField(TEXT("x"), Node->NodePosX);
		NodeObject->SetNumberField(TEXT("y"), Node->NodePosY);

		// 标记注释节点并捕获注释正文：NodeComment 才是注释框的真实正文，
		// 而 GetNodeTitle 对注释节点只返回固定的 "Comment" 字面量。
		const bool bIsComment = Node->IsA(UEdGraphNode_Comment::StaticClass());
		if (bIsComment)
		{
			NodeObject->SetStringField(TEXT("kind"), TEXT("comment"));
		}
		if (!Node->NodeComment.IsEmpty())
		{
			NodeObject->SetStringField(TEXT("comment"), Node->NodeComment);
		}

		// 引脚描述：方向、是否执行引脚、类型 —— 让读取方区分执行流与数据流。
		TArray<TSharedPtr<FJsonValue>> PinsArray;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin)
			{
				continue;
			}

			TSharedRef<FJsonObject> PinObject = MakeShared<FJsonObject>();
			PinObject->SetStringField(TEXT("name"), Pin->PinName.ToString());
			PinObject->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Output ? TEXT("out") : TEXT("in"));
			const bool bIsExec = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
			PinObject->SetBoolField(TEXT("isExec"), bIsExec);
			PinObject->SetStringField(TEXT("type"), bIsExec ? TEXT("") : BlueprintToJsonPrivate::PinTypeToFriendlyType(Pin->PinType));

			// 字面值：仅对未连线的输入引脚有意义。已连线引脚的值来自连线，其 DefaultValue
			// 通常为空或被忽略，跳过以免产生噪声。导出未连线输入引脚的默认值，可捕获节点上的
			// 字面量，例如 MapRangeClamped 节点的 InRangeA/OutRangeA 阈值。
			if (Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() == 0 && !Pin->DefaultValue.IsEmpty())
			{
				PinObject->SetStringField(TEXT("value"), Pin->DefaultValue);
			}

			PinsArray.Add(MakeShared<FJsonValueObject>(PinObject));
		}
		NodeObject->SetArrayField(TEXT("pins"), PinsArray);

		// 出向连线：以稳定的节点 GUID 为引用键（同时保留可读的对方标题作为辅助）。
		TArray<TSharedPtr<FJsonValue>> LinksArray;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output)
			{
				continue;
			}

			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				UEdGraphNode* LinkedNode = Linked ? Linked->GetOwningNode() : nullptr;
				if (!LinkedNode)
				{
					continue;
				}

				TSharedRef<FJsonObject> Link = MakeShared<FJsonObject>();
				Link->SetStringField(TEXT("fromPin"), Pin->PinName.ToString());
				Link->SetStringField(TEXT("toNodeGuid"), LinkedNode->NodeGuid.ToString());
				Link->SetStringField(TEXT("toNode"), LinkedNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
				Link->SetStringField(TEXT("toPin"), Linked->PinName.ToString());
				LinksArray.Add(MakeShared<FJsonValueObject>(Link));
			}
		}
		NodeObject->SetArrayField(TEXT("links"), LinksArray);

		NodesArray.Add(MakeShared<FJsonValueObject>(NodeObject));
	}
	GraphObject->SetArrayField(TEXT("nodes"), NodesArray);

	return GraphObject;
}

bool FBlueprintToJsonExporter::WriteBlueprintJson(const TSharedRef<FJsonObject>& Object, const FString& AssetPackagePath, const FString& OutputDirectory, FString& OutFileName)
{
	// 将单个蓝图的 JSON 对象序列化为带缩进字符串并写入文件，返回所用文件名。
	OutFileName = BlueprintToJsonPrivate::MakeSafeFileName(AssetPackagePath);
	const FString FullPath = FPaths::Combine(OutputDirectory, OutFileName);

	FString JsonString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(Object, Writer);

	return FFileHelper::SaveStringToFile(JsonString, *FullPath);
}

void FBlueprintToJsonExporter::Notify(const FText& InMessage, bool bIsWarning)
{
	// 显示一个短暂的编辑器通知，同时输出到日志。
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

	UE_LOG(LogTemp, Log, TEXT("BlueprintToJson: %s"), *InMessage.ToString());
}

#undef LOCTEXT_NAMESPACE
