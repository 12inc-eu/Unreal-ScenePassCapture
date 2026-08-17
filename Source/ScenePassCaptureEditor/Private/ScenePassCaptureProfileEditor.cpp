// Copyright Exiin Game Studio. All Rights Reserved.

#include "ScenePassCaptureProfileEditor.h"

#include "ScenePassCaptureFormatRules.h"
#include "ScenePassCaptureProfile.h"
#include "ScenePassCaptureSubsystem.h"

#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Slate/DeferredCleanupSlateBrush.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ScenePassCaptureProfileEditor"

namespace ScenePassCaptureEditorTabs
{
	static const FName Details(TEXT("ScenePassCaptureProfileEditor_Details"));
	static const FName Preview(TEXT("ScenePassCaptureProfileEditor_Preview"));
	static const FName Analysis(TEXT("ScenePassCaptureProfileEditor_Analysis"));
}

namespace
{
	constexpr float PreviewTileWidth = 280.0f;

	FText GetSourceDisplayLabel(EScenePassCaptureSource Source)
	{
		if (const UEnum* EnumPtr = StaticEnum<EScenePassCaptureSource>())
		{
			return EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(Source));
		}
		return LOCTEXT("UnknownSource", "Unknown");
	}

	FLinearColor GetSeverityColor(EScenePassCaptureIssueSeverity Severity)
	{
		switch (Severity)
		{
		case EScenePassCaptureIssueSeverity::Error:
			return FLinearColor(1.0f, 0.35f, 0.3f);
		case EScenePassCaptureIssueSeverity::Warning:
			return FLinearColor(1.0f, 0.75f, 0.25f);
		default:
			return FLinearColor(0.6f, 0.7f, 0.8f);
		}
	}

	FName GetSeverityIcon(EScenePassCaptureIssueSeverity Severity)
	{
		switch (Severity)
		{
		case EScenePassCaptureIssueSeverity::Error:
			return TEXT("Icons.Error");
		case EScenePassCaptureIssueSeverity::Warning:
			return TEXT("Icons.Warning");
		default:
			return TEXT("Icons.Info");
		}
	}
}

// ------------------------------------------------------------------------------------------------
// Setup

void FScenePassCaptureProfileEditor::InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UScenePassCaptureProfile* InProfile)
{
	Profile = InProfile;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(Profile);
	DetailsView->OnFinishedChangingProperties().AddSP(this, &FScenePassCaptureProfileEditor::OnProfileChanged);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TEXT("ScenePassCaptureProfileEditor_Layout_v1"))
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.35f)
				->AddTab(ScenePassCaptureEditorTabs::Details, ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.65f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.65f)
					->AddTab(ScenePassCaptureEditorTabs::Preview, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.35f)
					->AddTab(ScenePassCaptureEditorTabs::Analysis, ETabState::OpenedTab)
				)
			)
		);

	BuildToolbar();

	InitAssetEditor(Mode, InitToolkitHost, TEXT("ScenePassCaptureProfileEditor"), Layout, true, true, InProfile);

	RefreshPreview();
	RefreshAnalysis();

	RegenerateMenusAndToolbars();
}

FName FScenePassCaptureProfileEditor::GetToolkitFName() const
{
	return FName(TEXT("ScenePassCaptureProfileEditor"));
}

FText FScenePassCaptureProfileEditor::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Scene Pass Capture Profile");
}

FString FScenePassCaptureProfileEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("TabPrefix", "Pass Capture ").ToString();
}

FLinearColor FScenePassCaptureProfileEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.31f, 0.63f, 0.78f, 0.5f);
}

void FScenePassCaptureProfileEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Profile);
}

FString FScenePassCaptureProfileEditor::GetReferencerName() const
{
	return TEXT("FScenePassCaptureProfileEditor");
}

// ------------------------------------------------------------------------------------------------
// Tabs

void FScenePassCaptureProfileEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	const TSharedRef<FWorkspaceItem> Category = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu", "Scene Pass Capture"));

	InTabManager->RegisterTabSpawner(ScenePassCaptureEditorTabs::Details, FOnSpawnTab::CreateSP(this, &FScenePassCaptureProfileEditor::SpawnDetailsTab))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(Category)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("LevelEditor.Tabs.Details")));

	InTabManager->RegisterTabSpawner(ScenePassCaptureEditorTabs::Preview, FOnSpawnTab::CreateSP(this, &FScenePassCaptureProfileEditor::SpawnPreviewTab))
		.SetDisplayName(LOCTEXT("PreviewTab", "Targets"))
		.SetGroup(Category)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("LevelEditor.Tabs.Viewports")));

	InTabManager->RegisterTabSpawner(ScenePassCaptureEditorTabs::Analysis, FOnSpawnTab::CreateSP(this, &FScenePassCaptureProfileEditor::SpawnAnalysisTab))
		.SetDisplayName(LOCTEXT("AnalysisTab", "Cost and Validation"))
		.SetGroup(Category)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("LevelEditor.Tabs.StatsViewer")));
}

void FScenePassCaptureProfileEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(ScenePassCaptureEditorTabs::Details);
	InTabManager->UnregisterTabSpawner(ScenePassCaptureEditorTabs::Preview);
	InTabManager->UnregisterTabSpawner(ScenePassCaptureEditorTabs::Analysis);
}

TSharedRef<SDockTab> FScenePassCaptureProfileEditor::SpawnDetailsTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTab", "Details"))
		[
			DetailsView.ToSharedRef()
		];
}

TSharedRef<SDockTab> FScenePassCaptureProfileEditor::SpawnPreviewTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PreviewTab", "Targets"))
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			.Padding(8.0f)
			[
				SAssignNew(PreviewBox, SWrapBox)
				.UseAllottedSize(true)
				.InnerSlotPadding(FVector2D(8.0f, 8.0f))
			]
		];
}

TSharedRef<SDockTab> FScenePassCaptureProfileEditor::SpawnAnalysisTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("AnalysisTab", "Cost and Validation"))
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			.Padding(10.0f)
			[
				SAssignNew(AnalysisBox, SVerticalBox)
			]
		];
}

// ------------------------------------------------------------------------------------------------
// Toolbar

void FScenePassCaptureProfileEditor::BuildToolbar()
{
	const TSharedRef<FExtender> ToolbarExtender = MakeShared<FExtender>();

	ToolbarExtender->AddToolBarExtension(TEXT("Asset"), EExtensionHook::After, GetToolkitCommands(), FToolBarExtensionDelegate::CreateSP(this, &FScenePassCaptureProfileEditor::FillToolbar));

	AddToolbarExtender(ToolbarExtender);
}

void FScenePassCaptureProfileEditor::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection(TEXT("Capture"));
	{
		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateSP(this, &FScenePassCaptureProfileEditor::ToggleCapture),
				FCanExecuteAction::CreateSP(this, &FScenePassCaptureProfileEditor::CanCapture)),
			NAME_None,
			TAttribute<FText>::CreateLambda([this]() { return IsCapturing() ? LOCTEXT("StopCapture", "Stop") : LOCTEXT("StartCapture", "Capture"); }),
			LOCTEXT("ToggleCaptureTooltip", "Start or stop continuous capture. Targets the PIE world if one is running, otherwise the editor viewport."),
			TAttribute<FSlateIcon>::CreateLambda([this]() { return FSlateIcon(FAppStyle::GetAppStyleSetName(), IsCapturing() ? TEXT("PlayWorld.StopPlaySession") : TEXT("Icons.Play")); }));

		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateSP(this, &FScenePassCaptureProfileEditor::CaptureSingleFrame),
				FCanExecuteAction::CreateSP(this, &FScenePassCaptureProfileEditor::CanCapture)),
			NAME_None,
			LOCTEXT("SingleFrame", "Single Frame"),
			LOCTEXT("SingleFrameTooltip", "Capture exactly one frame into the targets, then stop."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Adjust")));
	}
	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection(TEXT("View"));
	{
		ToolbarBuilder.AddToolBarButton(
			FUIAction(FExecuteAction::CreateLambda([this]()
			{
				RefreshPreview();
				RefreshAnalysis();
			})),
			NAME_None,
			LOCTEXT("Refresh", "Refresh"),
			LOCTEXT("RefreshTooltip", "Rebuild the preview tiles and re-run validation."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Refresh")));
	}
	ToolbarBuilder.EndSection();
}

// ------------------------------------------------------------------------------------------------
// Capture control

UWorld* FScenePassCaptureProfileEditor::GetTargetWorld() const
{
	if (!GEditor)
	{
		return nullptr;
	}

	if (const FWorldContext* PIEContext = GEditor->GetPIEWorldContext())
	{
		if (UWorld* PIEWorld = PIEContext->World())
		{
			return PIEWorld;
		}
	}

	return GEditor->GetEditorWorldContext().World();
}

UScenePassCaptureSubsystem* FScenePassCaptureProfileEditor::GetTargetSubsystem() const
{
	UWorld* World = GetTargetWorld();
	return World ? World->GetSubsystem<UScenePassCaptureSubsystem>() : nullptr;
}

bool FScenePassCaptureProfileEditor::CanCapture() const
{
	return Profile != nullptr && GetTargetSubsystem() != nullptr;
}

bool FScenePassCaptureProfileEditor::IsCapturing() const
{
	const UScenePassCaptureSubsystem* Subsystem = GetTargetSubsystem();
	return Subsystem && Subsystem->IsCaptureEnabled() && Subsystem->GetProfile() == Profile;
}

void FScenePassCaptureProfileEditor::ToggleCapture()
{
	UScenePassCaptureSubsystem* Subsystem = GetTargetSubsystem();
	if (!Subsystem)
	{
		return;
	}

	if (IsCapturing())
	{
		Subsystem->StopCapture();
	}
	else
	{
		Subsystem->StartCapture(Profile);
	}
}

void FScenePassCaptureProfileEditor::CaptureSingleFrame()
{
	if (UScenePassCaptureSubsystem* Subsystem = GetTargetSubsystem())
	{
		Subsystem->SetProfile(Profile);
		Subsystem->CaptureSingleFrame();
	}
}

// ------------------------------------------------------------------------------------------------
// Preview and analysis

void FScenePassCaptureProfileEditor::OnProfileChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
	RefreshPreview();
	RefreshAnalysis();
}

void FScenePassCaptureProfileEditor::RefreshPreview()
{
	if (!PreviewBox.IsValid())
	{
		return;
	}

	PreviewBox->ClearChildren();
	PreviewBrushes.Reset();

	if (!Profile)
	{
		return;
	}

	for (const FScenePassCaptureEntry& Entry : Profile->Passes)
	{
		if (!Entry.bEnabled || !Entry.Target)
		{
			continue;
		}

		UTextureRenderTarget2D* Target = Entry.Target;

		const float Aspect = Target->SizeY > 0 ? static_cast<float>(Target->SizeX) / static_cast<float>(Target->SizeY) : 1.0f;
		const float TileHeight = Aspect > 0.0f ? PreviewTileWidth / Aspect : PreviewTileWidth;

		const TSharedRef<FDeferredCleanupSlateBrush> Brush = FDeferredCleanupSlateBrush::CreateBrush(Target, FVector2D(PreviewTileWidth, TileHeight));
		PreviewBrushes.Add(Brush);

		// The format matters here: a single channel target previews as pure red, not greyscale, which
		// reads as "broken" unless you know the format.
		const FText FormatName = ScenePassCapture_GetFormatDisplayName(Target->RenderTargetFormat);
		const FText Caption = FText::Format(LOCTEXT("TileCaption", "{0} x {1}   {2}   {3}"), FText::AsNumber(Target->SizeX), FText::AsNumber(Target->SizeY), FormatName, FText::FromString(Target->GetName()));

		PreviewBox->AddSlot()
		[
			SNew(SBox)
			.WidthOverride(PreviewTileWidth)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
				.Padding(6.0f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						SNew(STextBlock)
						.Text(GetSourceDisplayLabel(Entry.Source))
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont")))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox)
						.WidthOverride(PreviewTileWidth)
						.HeightOverride(TileHeight)
						[
							SNew(SImage)
							.Image_Lambda([Brush]() { return Brush->GetSlateBrush(); })
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(Caption)
						.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
				]
			]
		];
	}

	if (PreviewBox->GetChildren()->Num() == 0)
	{
		PreviewBox->AddSlot()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoTargets", "No enabled entry has a render target yet. Add one in the Details panel."))
			.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
		];
	}
}

void FScenePassCaptureProfileEditor::RefreshAnalysis()
{
	if (!AnalysisBox.IsValid())
	{
		return;
	}

	AnalysisBox->ClearChildren();

	if (!Profile)
	{
		return;
	}

	Analysis = AnalyzeScenePassCaptureProfile(*Profile);

	// A mid-range card sits near 360 GB/s, a high end one near 1 TB/s. Both are useful bookends.
	const double MidRangeMs = Analysis.EstimateMillisecondsAt(360.0 * 1024.0 * 1024.0 * 1024.0);
	const double HighEndMs = Analysis.EstimateMillisecondsAt(1000.0 * 1024.0 * 1024.0 * 1024.0);

	auto AddRow = [this](const FText& Label, const FText& Value)
	{
		AnalysisBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(220.0f)
				[
					SNew(STextBlock)
					.Text(Label)
					.ColorAndOpacity(FLinearColor(0.65f, 0.65f, 0.65f))
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(Value)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont")))
			]
		];
	};

	AnalysisBox->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 6.0f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("CostHeader", "Cost"))
		.Font(FAppStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
	];

	AddRow(LOCTEXT("ActivePasses", "Active passes"), FText::AsNumber(Analysis.ActiveEntryCount));
	AddRow(LOCTEXT("TargetMemory", "Render target memory"), FormatScenePassCaptureBytes(Analysis.TotalTargetBytes));
	AddRow(LOCTEXT("BytesPerFrame", "Traffic per captured frame"), FormatScenePassCaptureBytes(Analysis.BytesMovedPerFrame));
	AddRow(LOCTEXT("EstMidRange", "Estimated GPU, 360 GB/s"), FText::FromString(FString::Printf(TEXT("%.2f ms"), MidRangeMs)));
	AddRow(LOCTEXT("EstHighEnd", "Estimated GPU, 1 TB/s"), FText::FromString(FString::Printf(TEXT("%.2f ms"), HighEndMs)));

	AnalysisBox->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 8.0f, 0.0f, 0.0f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("MeasureHint", "Those two are estimates from bandwidth. For the real measured cost run \"stat GPU\" and look for \"Scene Pass Capture\", which times the actual passes on the hardware."))
		.ColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.55f))
		.AutoWrapText(true)
	];

	AnalysisBox->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 14.0f, 0.0f, 6.0f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("ValidationHeader", "Validation"))
		.Font(FAppStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
	];

	if (Analysis.Issues.Num() == 0)
	{
		AnalysisBox->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoIssues", "Nothing to flag."))
			.ColorAndOpacity(FLinearColor(0.45f, 0.8f, 0.45f))
		];
		return;
	}

	for (const FScenePassCaptureIssue& Issue : Analysis.Issues)
	{
		AnalysisBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 3.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush(GetSeverityIcon(Issue.Severity)))
				.ColorAndOpacity(GetSeverityColor(Issue.Severity))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(Issue.Message)
				.ColorAndOpacity(GetSeverityColor(Issue.Severity))
				.AutoWrapText(true)
			]
		];
	}
}

#undef LOCTEXT_NAMESPACE
