// Copyright Exiin Game Studio. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "CoreMinimal.h"
#include "ScenePassCaptureProfile.h"

#include "AssetDefinition_ScenePassCaptureProfile.generated.h"

UCLASS()
class UAssetDefinition_ScenePassCaptureProfile : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};
