// Copyright Exiin Game Studio. All Rights Reserved.

#include "AssetDefinition_ScenePassCaptureProfile.h"

#include "ScenePassCaptureProfileEditor.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_ScenePassCaptureProfile"

FText UAssetDefinition_ScenePassCaptureProfile::GetAssetDisplayName() const
{
	return LOCTEXT("AssetDisplayName", "Scene Pass Capture Profile");
}

FLinearColor UAssetDefinition_ScenePassCaptureProfile::GetAssetColor() const
{
	return FLinearColor(FColor(80, 160, 200));
}

TSoftClassPtr<UObject> UAssetDefinition_ScenePassCaptureProfile::GetAssetClass() const
{
	return UScenePassCaptureProfile::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_ScenePassCaptureProfile::GetAssetCategories() const
{
	static const auto Categories = { FAssetCategoryPath(LOCTEXT("AssetCategory", "Rendering")) };
	return Categories;
}

EAssetCommandResult UAssetDefinition_ScenePassCaptureProfile::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UScenePassCaptureProfile* Profile : OpenArgs.LoadObjects<UScenePassCaptureProfile>())
	{
		const TSharedRef<FScenePassCaptureProfileEditor> Editor = MakeShared<FScenePassCaptureProfileEditor>();
		Editor->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Profile);
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
