// Copyright Exiin Game Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ScenePassCaptureProjectSettings.generated.h"

class UScenePassCaptureProfile;

/**
 * Project Settings, Plugins, Scene Pass Capture.
 *
 * Point Default Profile at a profile and tick an auto start box, and capture runs with no Blueprint
 * wiring and no console command. Saved into DefaultGame.ini so it travels with the project.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Scene Pass Capture"))
class SCENEPASSCAPTURE_API UScenePassCaptureProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** The profile the auto start options below use. Leave empty to disable auto start entirely. */
	UPROPERTY(config, EditAnywhere, Category = "Auto Start")
	TSoftObjectPtr<UScenePassCaptureProfile> DefaultProfile;

	/** Start capturing as soon as a game or PIE world begins play. */
	UPROPERTY(config, EditAnywhere, Category = "Auto Start")
	bool bAutoStartInGame = false;

	/** Start capturing in the editor viewport as soon as a map finishes loading, without entering play. */
	UPROPERTY(config, EditAnywhere, Category = "Auto Start")
	bool bAutoStartInEditor = false;
};
