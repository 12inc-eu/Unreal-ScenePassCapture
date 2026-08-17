// Copyright Exiin Game Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "ScenePassCaptureProfileFactory.generated.h"

UCLASS()
class UScenePassCaptureProfileFactory : public UFactory
{
	GENERATED_BODY()

public:
	UScenePassCaptureProfileFactory();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool ShouldShowInNewMenu() const override { return true; }
};
