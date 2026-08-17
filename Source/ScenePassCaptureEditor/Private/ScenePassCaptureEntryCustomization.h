// Copyright Exiin Game Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class IPropertyUtilities;
struct FScenePassCaptureEntry;

/**
 * Details panel customization for one pass entry.
 *
 * Adds two things the stock panel cannot do:
 *  - a "Create Target" button under Target that makes a render target already set to the format
 *    and size the selected Source needs, in the same folder as the profile, and assigns it
 *  - an inline warning when the assigned target's format cannot represent the selected Source
 *
 * The object picker's own "Render Target" entry makes an unconfigured default, which is the
 * thing this button exists to avoid.
 */
class FScenePassCaptureEntryCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	/** Live pointer into the array element. Re-resolved on every use because the array can reallocate. */
	FScenePassCaptureEntry* GetEntry() const;

	FReply OnCreateTargetClicked();
	bool CanCreateTarget() const;
	FText GetCreateTargetTooltip() const;

	FText GetHeaderSummary() const;

	EVisibility GetWarningVisibility() const;
	FText GetWarningText() const;
	FSlateColor GetWarningColor() const;
	const FSlateBrush* GetWarningIcon() const;

	TSharedPtr<IPropertyHandle> StructHandle;
	TSharedPtr<IPropertyHandle> TargetHandle;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};
