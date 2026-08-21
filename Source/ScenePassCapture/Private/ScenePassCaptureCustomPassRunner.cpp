// Copyright Exiin Game Studio. All Rights Reserved.

#include "ScenePassCaptureCustomPassRunner.h"

#include "ScenePassCaptureProfile.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "ShowFlags.h"

FScenePassCaptureCustomPassRunner::FScenePassCaptureCustomPassRunner(UWorld* InWorld)
	: World(InWorld)
{
}

FScenePassCaptureCustomPassRunner::~FScenePassCaptureCustomPassRunner()
{
	Shutdown();
}

void FScenePassCaptureCustomPassRunner::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(CaptureActor);

	for (TObjectPtr<USceneCaptureComponent2D>& Component : Components)
	{
		Collector.AddReferencedObject(Component);
	}
}

FString FScenePassCaptureCustomPassRunner::GetReferencerName() const
{
	return TEXT("FScenePassCaptureCustomPassRunner");
}

void FScenePassCaptureCustomPassRunner::Shutdown()
{
	for (TObjectPtr<USceneCaptureComponent2D>& Component : Components)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	Components.Reset();
	FrameCounters.Reset();

	if (CaptureActor)
	{
		CaptureActor->Destroy();
		CaptureActor = nullptr;
	}
}

void FScenePassCaptureCustomPassRunner::RequestCapture(FName PassName)
{
	if (PassName.IsNone())
	{
		bRequestAll = true;
	}
	else
	{
		PendingRequests.Add(PassName);
	}
}

void FScenePassCaptureCustomPassRunner::AddRuntimeShowOnlyActor(FName PassName, AActor* Actor)
{
	if (Actor)
	{
		RuntimeShowOnlyActors.FindOrAdd(PassName).AddUnique(Actor);
	}
}

void FScenePassCaptureCustomPassRunner::AddRuntimeHiddenActor(FName PassName, AActor* Actor)
{
	if (Actor)
	{
		RuntimeHiddenActors.FindOrAdd(PassName).AddUnique(Actor);
	}
}

void FScenePassCaptureCustomPassRunner::ClearRuntimeActors(FName PassName)
{
	RuntimeShowOnlyActors.Remove(PassName);
	RuntimeHiddenActors.Remove(PassName);
}

void FScenePassCaptureCustomPassRunner::EnsureComponents(const UScenePassCaptureProfile* Profile)
{
	UWorld* WorldPtr = World.Get();
	if (!WorldPtr || !Profile)
	{
		return;
	}

	const int32 Required = Profile->CustomPasses.Num();

	if (Required == 0)
	{
		Shutdown();
		return;
	}

	if (!CaptureActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.Name = MakeUniqueObjectName(WorldPtr, AActor::StaticClass(), TEXT("ScenePassCaptureCustomPasses"));

		CaptureActor = WorldPtr->SpawnActor<AActor>(AActor::StaticClass(), SpawnParams);

		if (CaptureActor)
		{
			CaptureActor->SetActorHiddenInGame(true);
#if WITH_EDITOR
			// Keep it out of the outliner: it is plumbing, not content.
			CaptureActor->bIsEditorOnlyActor = false;
			CaptureActor->SetActorLabel(TEXT("ScenePassCaptureCustomPasses"));
#endif
		}
	}

	if (!CaptureActor)
	{
		return;
	}

	// Grow or shrink to match the profile.
	while (Components.Num() > Required)
	{
		if (USceneCaptureComponent2D* Component = Components.Last())
		{
			Component->DestroyComponent();
		}
		Components.Pop();
	}

	while (Components.Num() < Required)
	{
		USceneCaptureComponent2D* Component = NewObject<USceneCaptureComponent2D>(CaptureActor, NAME_None, RF_Transient);
		Component->SetupAttachment(CaptureActor->GetRootComponent());
		Component->RegisterComponent();

		// Captures are driven explicitly from Tick so the timing options actually mean something.
		Component->bCaptureEveryFrame = false;
		Component->bCaptureOnMovement = false;

		Components.Add(Component);
	}

	FrameCounters.SetNumZeroed(Required, EAllowShrinking::Yes);
}

void FScenePassCaptureCustomPassRunner::ApplyPassSettings(USceneCaptureComponent2D& Component, const FScenePassCaptureCustomPass& Pass, const FScenePassCaptureViewPoint& ViewPoint)
{
	Component.TextureTarget = Pass.Target;
	Component.CaptureSource = Pass.CaptureSource;
	Component.FOVAngle = ViewPoint.FOVDegrees;
	Component.bAlwaysPersistRenderingState = Pass.bPersistRenderingState;

	Component.SetWorldLocationAndRotation(ViewPoint.Location, ViewPoint.Rotation);

	// ShowFlags is the live runtime copy. Setting it directly avoids round-tripping through the
	// ShowFlagSettings string array, which is meant for the details panel rather than code.
	FEngineShowFlags& Flags = Component.ShowFlags;

	Flags.SetLighting(Pass.ShowFlags.bLighting);
	Flags.SetDynamicShadows(Pass.ShowFlags.bDynamicShadows);
	Flags.SetGlobalIllumination(Pass.ShowFlags.bGlobalIllumination);
	Flags.SetAmbientOcclusion(Pass.ShowFlags.bAmbientOcclusion);
	Flags.SetReflectionEnvironment(Pass.ShowFlags.bReflectionEnvironment);
	Flags.SetLightingOnlyOverride(Pass.ShowFlags.bLightingOnlyOverride);

	// The GlobalIllumination and ReflectionEnvironment show flags do not switch Lumen off on their own.
	// Lumen is chosen per view through post process settings, so it has to be overridden there too.
	if (!Pass.ShowFlags.bGlobalIllumination)
	{
		Component.PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
		Component.PostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
	}
	else
	{
		Component.PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = false;
	}

	if (!Pass.ShowFlags.bReflectionEnvironment)
	{
		Component.PostProcessSettings.bOverride_ReflectionMethod = true;
		Component.PostProcessSettings.ReflectionMethod = EReflectionMethod::None;
	}
	else
	{
		Component.PostProcessSettings.bOverride_ReflectionMethod = false;
	}

	Flags.SetStaticMeshes(Pass.ShowFlags.bStaticMeshes);
	Flags.SetSkeletalMeshes(Pass.ShowFlags.bSkeletalMeshes);
	Flags.SetHair(Pass.ShowFlags.bHair);
	Flags.SetNaniteMeshes(Pass.ShowFlags.bNaniteMeshes);
	Flags.SetInstancedStaticMeshes(Pass.ShowFlags.bInstancedStaticMeshes);
	Flags.SetInstancedGrass(Pass.ShowFlags.bInstancedGrass);
	Flags.SetBSP(Pass.ShowFlags.bBSP);
	Flags.SetLandscape(Pass.ShowFlags.bLandscape);
	Flags.SetInstancedFoliage(Pass.ShowFlags.bInstancedFoliage);
	Flags.SetParticles(Pass.ShowFlags.bParticles);
	Flags.SetTranslucency(Pass.ShowFlags.bTranslucency);
	Flags.SetDecals(Pass.ShowFlags.bDecals);

	Flags.SetFog(Pass.ShowFlags.bFog);
	Flags.SetVolumetricFog(Pass.ShowFlags.bVolumetricFog);
	Flags.SetAtmosphere(Pass.ShowFlags.bAtmosphere);

	Flags.SetPostProcessing(Pass.ShowFlags.bPostProcessing);
	Flags.SetBloom(Pass.ShowFlags.bBloom);
	Flags.SetAntiAliasing(Pass.ShowFlags.bAntiAliasing);
	Flags.SetMotionBlur(Pass.ShowFlags.bMotionBlur);
	Flags.SetDepthOfField(Pass.ShowFlags.bDepthOfField);

	// Actor filtering. This is the only lever that reduces the render thread cost, since visibility
	// is per-primitive: fewer primitives considered, less CPU, regardless of resolution.
	//
	// Three sources, in order of how well they work at runtime: tags resolve against the live level so
	// they work in a packaged game, runtime actors come from Blueprint, and the soft pointers only
	// resolve on a level someone edited by hand.
	Component.ClearShowOnlyComponents();
	Component.ClearHiddenComponents();
	Component.ShowOnlyActors.Reset();
	Component.HiddenActors.Reset();

	UWorld* WorldPtr = World.Get();

	if (Pass.ShowOnlyActorTags.Num() > 0 || Pass.HiddenActorTags.Num() > 0)
	{
		if (WorldPtr)
		{
			for (TActorIterator<AActor> It(WorldPtr); It; ++It)
			{
				AActor* Actor = *It;
				if (!Actor)
				{
					continue;
				}

				for (const FName& Tag : Pass.ShowOnlyActorTags)
				{
					if (!Tag.IsNone() && Actor->ActorHasTag(Tag))
					{
						Component.ShowOnlyActors.Add(Actor);
						break;
					}
				}

				for (const FName& Tag : Pass.HiddenActorTags)
				{
					if (!Tag.IsNone() && Actor->ActorHasTag(Tag))
					{
						Component.HiddenActors.Add(Actor);
						break;
					}
				}
			}
		}
	}

	if (const TArray<TWeakObjectPtr<AActor>>* RuntimeShowOnly = RuntimeShowOnlyActors.Find(Pass.PassName))
	{
		for (const TWeakObjectPtr<AActor>& WeakActor : *RuntimeShowOnly)
		{
			if (AActor* Actor = WeakActor.Get())
			{
				Component.ShowOnlyActors.AddUnique(Actor);
			}
		}
	}

	if (const TArray<TWeakObjectPtr<AActor>>* RuntimeHidden = RuntimeHiddenActors.Find(Pass.PassName))
	{
		for (const TWeakObjectPtr<AActor>& WeakActor : *RuntimeHidden)
		{
			if (AActor* Actor = WeakActor.Get())
			{
				Component.HiddenActors.AddUnique(Actor);
			}
		}
	}

	for (const TSoftObjectPtr<AActor>& SoftActor : Pass.ShowOnlyActors)
	{
		if (AActor* Actor = SoftActor.Get())
		{
			Component.ShowOnlyActors.AddUnique(Actor);
		}
	}

	for (const TSoftObjectPtr<AActor>& SoftActor : Pass.HiddenActors)
	{
		if (AActor* Actor = SoftActor.Get())
		{
			Component.HiddenActors.AddUnique(Actor);
		}
	}

	// Component tags. Finer than the actor lists, and the only way to reach component types that have
	// no show flag at all. A tag is just an FName, so unlike a component reference it stores fine in a
	// data asset and resolves against whatever is in the level at runtime.
	bool bAnyShowOnlyComponent = false;

	if (WorldPtr && (Pass.ShowOnlyComponentTags.Num() > 0 || Pass.HiddenComponentTags.Num() > 0))
	{
		for (TActorIterator<AActor> It(WorldPtr); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor)
			{
				continue;
			}

			TArray<UPrimitiveComponent*> Primitives;
			Actor->GetComponents<UPrimitiveComponent>(Primitives, /*bIncludeFromChildActors*/ true);

			for (UPrimitiveComponent* Primitive : Primitives)
			{
				if (!Primitive)
				{
					continue;
				}

				for (const FName& Tag : Pass.ShowOnlyComponentTags)
				{
					if (!Tag.IsNone() && Primitive->ComponentHasTag(Tag))
					{
						Component.ShowOnlyComponent(Primitive);
						bAnyShowOnlyComponent = true;
						break;
					}
				}

				for (const FName& Tag : Pass.HiddenComponentTags)
				{
					if (!Tag.IsNone() && Primitive->ComponentHasTag(Tag))
					{
						Component.HideComponent(Primitive);
						break;
					}
				}
			}
		}
	}

	// Show-only mode has to be on if EITHER an actor or a component was named, otherwise the show-only
	// component list is collected and then quietly ignored.
	Component.PrimitiveRenderMode = (Component.ShowOnlyActors.Num() > 0 || bAnyShowOnlyComponent) ? ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList : ESceneCapturePrimitiveRenderMode::PRM_LegacySceneCapture;

	// Depth slab. With a focus actor the slab tracks that actor's distance from the camera, so a
	// character stays inside it as they move. Without one, Near and Far are plain camera distances.
	if (Pass.bUseDepthSlab)
	{
		float NearDistance = Pass.SlabNear;
		float FarDistance = Pass.SlabFar;

		if (const AActor* Focus = Pass.FocusActor.Get())
		{
			const float FocusDistance = FVector::Dist(ViewPoint.Location, Focus->GetActorLocation());
			NearDistance = FocusDistance - Pass.SlabNear;
			FarDistance = FocusDistance + Pass.SlabFar;
		}

		// The near plane cannot be at or behind the eye.
		NearDistance = FMath::Max(NearDistance, 1.0f);
		FarDistance = FMath::Max(FarDistance, NearDistance + 1.0f);

		Component.bOverride_CustomNearClippingPlane = true;
		Component.CustomNearClippingPlane = NearDistance;
		Component.MaxViewDistanceOverride = FarDistance;
	}
	else
	{
		Component.bOverride_CustomNearClippingPlane = false;
		Component.MaxViewDistanceOverride = -1.0f;
	}
}

void FScenePassCaptureCustomPassRunner::Tick(const UScenePassCaptureProfile* Profile, const FScenePassCaptureViewPoint& ViewPoint)
{
	if (!Profile || Profile->CustomPasses.Num() == 0)
	{
		Shutdown();
		return;
	}

	// Without a view point there is nothing to mirror, so skip rather than render from the origin.
	if (!ViewPoint.bValid)
	{
		return;
	}

	EnsureComponents(Profile);

	const int32 Count = FMath::Min(Components.Num(), Profile->CustomPasses.Num());

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FScenePassCaptureCustomPass& Pass = Profile->CustomPasses[Index];
		USceneCaptureComponent2D* Component = Components[Index];

		if (!Component)
		{
			continue;
		}

		if (!Pass.bEnabled || !Pass.Target)
		{
			continue;
		}

		// Size from the view, not from whatever the target happens to be. A scene capture frames by the
		// target's aspect ratio, so a mismatched target does not mirror the camera, it reframes it.
		// This also renders straight into the final resolution instead of rendering large and scaling.
		if (ViewPoint.ViewSize.X > 0 && ViewPoint.ViewSize.Y > 0)
		{
			const FIntPoint Desired = ScenePassCapture_ApplyResolutionScale(ViewPoint.ViewSize, Pass.ResolutionScale, Pass.bAlignSizeToMultipleOfTwo);
			if (Desired.X != Pass.Target->SizeX || Desired.Y != Pass.Target->SizeY)
			{
				Pass.Target->ResizeTarget(Desired.X, Desired.Y);
			}
		}

		bool bShouldCapture = false;

		switch (Pass.Timing)
		{
		case EScenePassCaptureTiming::EveryFrame:
			bShouldCapture = true;
			break;

		case EScenePassCaptureTiming::EveryNFrames:
		{
			const int32 Interval = FMath::Max(1, Pass.FrameInterval);
			int32& Counter = FrameCounters[Index];
			bShouldCapture = (Counter % Interval) == 0;
			Counter = (Counter + 1) % Interval;
			break;
		}

		case EScenePassCaptureTiming::OnDemand:
			bShouldCapture = bRequestAll || PendingRequests.Contains(Pass.PassName);
			break;
		}

		if (!bShouldCapture)
		{
			continue;
		}

		ApplyPassSettings(*Component, Pass, ViewPoint);

		// This is the expensive line in the whole plugin: a full scene render.
		Component->CaptureScene();
	}

	PendingRequests.Reset();
	bRequestAll = false;
}
