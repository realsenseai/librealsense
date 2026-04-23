using UnrealBuildTool;
using System.IO;

public class RealSense : ModuleRules
{
	public RealSense(ReadOnlyTargetRules Target) : base(Target)
	{
		bEnableExceptions = true;
		bUseRTTI = false;

		PrivateIncludePaths.AddRange(new string[] { Path.Combine( ModuleDirectory, "Private") });
		PublicIncludePaths.AddRange(new string[] { Path.Combine(ModuleDirectory, "Public") });

		PrivatePCHHeaderFile = "Private/PCH.h";

		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"RHI",
				"RenderCore",
				"Slate",
				"SlateCore",
				"RuntimeMeshComponent"
			}
		);

		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.AddRange(
			new string[] {
				"UnrealEd",
				"EditorStyle",
				"DesktopWidgets",
				"DesktopPlatform"
			});
		}

		//string RealSenseDirectory = Environment.GetEnvironmentVariable("RSSDK_DIR");
		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			string RealSenseDirectory = "/usr/local/";
			PublicIncludePaths.Add(Path.Combine(RealSenseDirectory, "include"));
			PublicAdditionalLibraries.Add(Path.Combine(RealSenseDirectory, "lib64/librealsense2.so"));
		}
		else
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string RealSenseDirectory = "C:/Program Files (x86)/Intel RealSense SDK 2.0";
			PublicIncludePaths.Add(Path.Combine(RealSenseDirectory, "include"));
			PublicAdditionalLibraries.Add(Path.Combine(RealSenseDirectory, "lib", "x64", "realsense2.lib"));
			
			string Destination = Path.Combine("$(BinaryOutputDir)", "realsense2.dll");
			string Source = Path.Combine(RealSenseDirectory, "bin", "x64", "realsense2.dll");
			RuntimeDependencies.Add(Destination, Source);
		}
	}
}
