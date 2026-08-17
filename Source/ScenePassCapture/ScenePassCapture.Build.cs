// Copyright Exiin Game Studio. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ScenePassCapture : ModuleRules
{
	// The Lumen surface cache sources reach into Renderer private headers, which Epic does not treat
	// as a stable API. Everything Lumen sits behind SCENEPASSCAPTURE_LUMEN, so if an engine upgrade
	// breaks those headers, flip this to false and the rest of the plugin keeps building untouched.
	private const bool bEnableLumenSources = true;

	public ScenePassCapture(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		if (bEnableLumenSources)
		{
			PrivateIncludePaths.AddRange(new string[]
			{
				Path.Combine(EngineDirectory, "Source", "Runtime", "Renderer", "Private"),
				Path.Combine(EngineDirectory, "Source", "Runtime", "Renderer", "Internal"),
			});

			PublicDefinitions.Add("SCENEPASSCAPTURE_LUMEN=1");
		}
		else
		{
			PublicDefinitions.Add("SCENEPASSCAPTURE_LUMEN=0");
		}

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"DeveloperSettings",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects",
			"RenderCore",
			"RHI",
			"Renderer",
		});
	}
}
