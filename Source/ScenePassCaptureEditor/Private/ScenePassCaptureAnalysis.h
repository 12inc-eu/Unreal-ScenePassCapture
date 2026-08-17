// Copyright Exiin Game Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"

class UScenePassCaptureProfile;

enum class EScenePassCaptureIssueSeverity : uint8
{
	Info,
	Warning,
	Error,
};

struct FScenePassCaptureIssue
{
	int32 EntryIndex = INDEX_NONE;
	EScenePassCaptureIssueSeverity Severity = EScenePassCaptureIssueSeverity::Warning;
	FText Message;
};

/** Static analysis of a profile: what it will cost, and everything that looks wrong with it. */
struct FScenePassCaptureAnalysis
{
	int32 ActiveEntryCount = 0;

	/** Total bytes of render target memory the profile pins. */
	int64 TotalTargetBytes = 0;

	/** Bytes read plus bytes written per captured frame, which is what the blits are actually bound by. */
	int64 BytesMovedPerFrame = 0;

	TArray<FScenePassCaptureIssue> Issues;

	/** Rough blit time at a given memory bandwidth, in milliseconds. */
	double EstimateMillisecondsAt(double BytesPerSecond) const;

	int32 CountIssues(EScenePassCaptureIssueSeverity Severity) const;
};

FScenePassCaptureAnalysis AnalyzeScenePassCaptureProfile(const UScenePassCaptureProfile& Profile);

/** "16.6 MB", "1.2 GB". */
FText FormatScenePassCaptureBytes(int64 Bytes);
