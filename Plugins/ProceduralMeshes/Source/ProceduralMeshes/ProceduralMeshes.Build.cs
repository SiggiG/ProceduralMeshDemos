// Copyright 2016, Sigurdur Gunnarsson. All Rights Reserved. 

using UnrealBuildTool;

public class ProceduralMeshes : ModuleRules
{
	public ProceduralMeshes(ReadOnlyTargetRules Target) : base(Target)
    {
		
		PublicIncludePaths.AddRange(new string[] { "ProceduralMeshes/Public" });

        PrivateIncludePaths.AddRange(new string[] { "ProceduralMeshes/Private" });

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "RenderCore", "RHI", "RuntimeMeshComponent" });


        PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
// 				"Slate",
// 				"SlateCore",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(new string[] {  });
    }
}
