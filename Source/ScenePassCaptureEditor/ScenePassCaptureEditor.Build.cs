// Copyright Exiin Game Studio. All Rights Reserved.

using UnrealBuildTool;

public class ScenePassCaptureEditor : ModuleRules
{
	public ScenePassCaptureEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AssetDefinition",
			"AssetTools",
			"EditorFramework",
			"InputCore",
			"PropertyEditor",
			"RHI",
			"RenderCore",
			"ScenePassCapture",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"ToolWidgets",
			"UnrealEd",
			"WorkspaceMenuStructure",
		});
	}
}
