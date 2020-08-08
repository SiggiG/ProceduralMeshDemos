// Copyright 2016, Sigurdur Gunnarsson. All Rights Reserved. 

using UnrealBuildTool;

public class ProceduralMeshes : ModuleRules
{
	public ProceduralMeshes(ReadOnlyTargetRules Target) : base(Target)
    {
	    PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	    
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "RuntimeMeshComponent" });
        
        PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
			});
    }
}
