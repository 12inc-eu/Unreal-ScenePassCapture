// Copyright Exiin Game Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScenePassCaptureAnalysis.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UObject/GCObject.h"

class FDeferredCleanupSlateBrush;
class IDetailsView;
class SVerticalBox;
class SWrapBox;
class UScenePassCaptureProfile;
class UScenePassCaptureSubsystem;
class UWorld;
struct FPropertyChangedEvent;

/**
 * Asset editor for a Scene Pass Capture Profile.
 *
 * Three panes: the usual details panel, a live preview wall showing every target as it fills in,
 * and an analysis pane with the memory and bandwidth cost plus everything that looks misconfigured.
 * The toolbar drives capture against the PIE world if one is running, otherwise the editor viewport.
 */
class FScenePassCaptureProfileEditor : public FAssetEditorToolkit, public FGCObject
{
public:
	void InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UScenePassCaptureProfile* InProfile);

	// FAssetEditorToolkit
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

private:
	TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnPreviewTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnAnalysisTab(const FSpawnTabArgs& Args);

	void BuildToolbar();
	void FillToolbar(FToolBarBuilder& ToolbarBuilder);

	void OnProfileChanged(const FPropertyChangedEvent& PropertyChangedEvent);
	void RefreshPreview();
	void RefreshAnalysis();

	/** PIE world if one is running, otherwise the editor world. */
	UWorld* GetTargetWorld() const;
	UScenePassCaptureSubsystem* GetTargetSubsystem() const;

	void ToggleCapture();
	bool IsCapturing() const;
	void CaptureSingleFrame();
	bool CanCapture() const;

	TObjectPtr<UScenePassCaptureProfile> Profile;

	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<SWrapBox> PreviewBox;
	TSharedPtr<SVerticalBox> AnalysisBox;

	/** Kept alive for as long as the preview tiles reference them. */
	TArray<TSharedPtr<FDeferredCleanupSlateBrush>> PreviewBrushes;

	FScenePassCaptureAnalysis Analysis;
};
