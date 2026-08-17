// Copyright Exiin Game Studio. All Rights Reserved.

#include "ScenePassCaptureEntryCustomization.h"

#include "ScenePassCaptureFormatRules.h"
#include "ScenePassCaptureTypes.h"

#include "AssetToolsModule.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Factories/TextureRenderTargetFactoryNew.h"
#include "IAssetTools.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PropertyHandle.h"
#include "Styling/AppStyle.h"
#include "UnrealClient.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ScenePassCaptureEntryCustomization"

namespace
{
	/** New targets default to the size of whatever viewport you are looking at, falling back to 1080p. */
	FIntPoint GetDefaultTargetSize()
	{
		if (GEditor)
		{
			if (const FViewport* Viewport = GEditor->GetActiveViewport())
			{
				const FIntPoint Size = Viewport->GetSizeXY();
				if (Size.X > 0 && Size.Y > 0)
				{
					return Size;
				}
			}
		}
		return FIntPoint(1920, 1080);
	}
}

TSharedRef<IPropertyTypeCustomization> FScenePassCaptureEntryCustomization::MakeInstance()
{
	return MakeShared<FScenePassCaptureEntryCustomization>();
}

FScenePassCaptureEntry* FScenePassCaptureEntryCustomization::GetEntry() const
{
	if (!StructHandle.IsValid())
	{
		return nullptr;
	}

	TArray<void*> RawData;
	StructHandle->AccessRawData(RawData);

	// Multi-selection is deliberately unsupported: creating one target for several entries is meaningless.
	if (RawData.Num() != 1 || RawData[0] == nullptr)
	{
		return nullptr;
	}

	return static_cast<FScenePassCaptureEntry*>(RawData[0]);
}

void FScenePassCaptureEntryCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructHandle = PropertyHandle;
	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();

	// The stock header just says "6 members", which is useless once you have eight passes in the list.
	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(260.0f)
	[
		SNew(STextBlock)
		.Text(this, &FScenePassCaptureEntryCustomization::GetHeaderSummary)
		.Font(CustomizationUtils.GetRegularFont())
	];
}

FText FScenePassCaptureEntryCustomization::GetHeaderSummary() const
{
	const FScenePassCaptureEntry* Entry = GetEntry();
	if (!Entry)
	{
		return FText::GetEmpty();
	}

	FText SourceName = LOCTEXT("UnknownSource", "Unknown");
	if (const UEnum* EnumPtr = StaticEnum<EScenePassCaptureSource>())
	{
		SourceName = EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(Entry->Source));
	}

	if (!Entry->Target)
	{
		return FText::Format(LOCTEXT("HeaderNoTarget", "{0}  (no target)"), SourceName);
	}

	return FText::Format(LOCTEXT("HeaderWithTarget", "{0}  ->  {1}"), SourceName, FText::FromString(Entry->Target->GetName()));
}

void FScenePassCaptureEntryCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructHandle = PropertyHandle;
	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();

	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);

	static const FName TargetPropertyName = GET_MEMBER_NAME_CHECKED(FScenePassCaptureEntry, Target);

	for (uint32 Index = 0; Index < NumChildren; ++Index)
	{
		const TSharedPtr<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(Index);
		if (!ChildHandle.IsValid() || !ChildHandle->GetProperty())
		{
			continue;
		}

		if (ChildHandle->GetProperty()->GetFName() != TargetPropertyName)
		{
			ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
			continue;
		}

		TargetHandle = ChildHandle;

		// Keep the stock object picker, and hang the create button underneath it.
		ChildBuilder.AddProperty(ChildHandle.ToSharedRef())
		.CustomWidget()
		.NameContent()
		[
			ChildHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(320.0f)
		.MaxDesiredWidth(0.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ChildHandle->CreatePropertyValueWidget()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ToolTipText(this, &FScenePassCaptureEntryCustomization::GetCreateTargetTooltip)
				.IsEnabled(this, &FScenePassCaptureEntryCustomization::CanCreateTarget)
				.OnClicked(this, &FScenePassCaptureEntryCustomization::OnCreateTargetClicked)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CreateTarget", "Create Target"))
					.Font(CustomizationUtils.GetRegularFont())
				]
			]
		];
	}

	// Only present when there is something wrong, so a clean profile stays visually clean.
	ChildBuilder.AddCustomRow(LOCTEXT("FormatWarningFilter", "Format Warning"))
	.Visibility(TAttribute<EVisibility>(this, &FScenePassCaptureEntryCustomization::GetWarningVisibility))
	.WholeRowContent()
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		.Padding(0.0f, 2.0f, 6.0f, 2.0f)
		[
			SNew(SImage)
			.Image(this, &FScenePassCaptureEntryCustomization::GetWarningIcon)
			.ColorAndOpacity(this, &FScenePassCaptureEntryCustomization::GetWarningColor)
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(this, &FScenePassCaptureEntryCustomization::GetWarningText)
			.ColorAndOpacity(this, &FScenePassCaptureEntryCustomization::GetWarningColor)
			.AutoWrapText(true)
		]
	];
}

// ------------------------------------------------------------------------------------------------
// Create Target

bool FScenePassCaptureEntryCustomization::CanCreateTarget() const
{
	return GetEntry() != nullptr;
}

FText FScenePassCaptureEntryCustomization::GetCreateTargetTooltip() const
{
	const FScenePassCaptureEntry* Entry = GetEntry();
	if (!Entry)
	{
		return LOCTEXT("CreateTargetUnavailable", "Select a single pass entry to create a target for it.");
	}

	const FIntPoint Size = ScenePassCapture_ApplyResolutionScale(GetDefaultTargetSize(), Entry->ResolutionScale, Entry->bAlignSizeToMultipleOfTwo);
	const FText FormatName = ScenePassCapture_GetFormatDisplayName(ScenePassCapture_GetRecommendedFormat(*Entry));

	return FText::Format(LOCTEXT("CreateTargetTooltip", "Create a {0} render target at {1} x {2}, already configured for this source, in the same folder as this profile, and assign it here.\n\nThe object picker's own Render Target entry makes an unconfigured default instead."), FormatName, FText::AsNumber(Size.X), FText::AsNumber(Size.Y));
}

FReply FScenePassCaptureEntryCustomization::OnCreateTargetClicked()
{
	const FScenePassCaptureEntry* Entry = GetEntry();
	if (!Entry || !TargetHandle.IsValid() || !StructHandle.IsValid())
	{
		return FReply::Handled();
	}

	// Drop the new asset next to the profile that references it.
	FString PackagePath = TEXT("/Game");
	TArray<UObject*> OuterObjects;
	StructHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() > 0 && OuterObjects[0] && OuterObjects[0]->GetOutermost())
	{
		PackagePath = FPackageName::GetLongPackagePath(OuterObjects[0]->GetOutermost()->GetName());
	}

	const ETextureRenderTargetFormat Format = ScenePassCapture_GetRecommendedFormat(*Entry);
	const FIntPoint Size = ScenePassCapture_ApplyResolutionScale(GetDefaultTargetSize(), Entry->ResolutionScale, Entry->bAlignSizeToMultipleOfTwo);
	const FString BaseName = ScenePassCapture_SuggestTargetName(*Entry);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

	FString AssetName;
	FString PackageName;
	AssetTools.CreateUniqueAssetName(PackagePath / BaseName, FString(), PackageName, AssetName);

	UTextureRenderTargetFactoryNew* Factory = NewObject<UTextureRenderTargetFactoryNew>();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, FPackageName::GetLongPackagePath(PackageName), UTextureRenderTarget2D::StaticClass(), Factory);

	UTextureRenderTarget2D* NewTarget = Cast<UTextureRenderTarget2D>(NewAsset);
	if (!NewTarget)
	{
		return FReply::Handled();
	}

	NewTarget->RenderTargetFormat = Format;
	NewTarget->ClearColor = FLinearColor::Black;
	NewTarget->bAutoGenerateMips = false;
	NewTarget->AddressX = TA_Clamp;
	NewTarget->AddressY = TA_Clamp;
	NewTarget->InitAutoFormat(Size.X, Size.Y);
	NewTarget->UpdateResourceImmediate(true);
	NewTarget->PostEditChange();
	NewTarget->MarkPackageDirty();

	// Route the assignment through the handle so it lands in a transaction and the panel refreshes.
	TargetHandle->SetValueFromFormattedString(NewTarget->GetPathName());

	return FReply::Handled();
}

// ------------------------------------------------------------------------------------------------
// Format warning

EVisibility FScenePassCaptureEntryCustomization::GetWarningVisibility() const
{
	const FScenePassCaptureEntry* Entry = GetEntry();
	if (!Entry)
	{
		return EVisibility::Collapsed;
	}

	FText Message;
	bool bIsError = false;
	return ScenePassCapture_ValidateTargetFormat(*Entry, Message, bIsError) ? EVisibility::Collapsed : EVisibility::Visible;
}

FText FScenePassCaptureEntryCustomization::GetWarningText() const
{
	const FScenePassCaptureEntry* Entry = GetEntry();
	if (!Entry)
	{
		return FText::GetEmpty();
	}

	FText Message;
	bool bIsError = false;
	ScenePassCapture_ValidateTargetFormat(*Entry, Message, bIsError);
	return Message;
}

FSlateColor FScenePassCaptureEntryCustomization::GetWarningColor() const
{
	const FScenePassCaptureEntry* Entry = GetEntry();
	if (!Entry)
	{
		return FSlateColor::UseForeground();
	}

	FText Message;
	bool bIsError = false;
	ScenePassCapture_ValidateTargetFormat(*Entry, Message, bIsError);

	return bIsError ? FSlateColor(FLinearColor(1.0f, 0.35f, 0.3f)) : FSlateColor(FLinearColor(1.0f, 0.75f, 0.25f));
}

const FSlateBrush* FScenePassCaptureEntryCustomization::GetWarningIcon() const
{
	const FScenePassCaptureEntry* Entry = GetEntry();
	if (!Entry)
	{
		return FAppStyle::GetBrush(TEXT("Icons.Warning"));
	}

	FText Message;
	bool bIsError = false;
	ScenePassCapture_ValidateTargetFormat(*Entry, Message, bIsError);

	return FAppStyle::GetBrush(bIsError ? TEXT("Icons.Error") : TEXT("Icons.Warning"));
}

#undef LOCTEXT_NAMESPACE
