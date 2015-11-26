// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;

public class Godrays : ModuleRules
{
    public Godrays(TargetInfo Target)
	{
		Type = ModuleType.External;

        Definitions.Add("WITH_GAMEWORKS_GODRAYS=1");

        string GodraysDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "GameWorks/Godrays/";

        PublicIncludePaths.Add(GodraysDir + "include");

		// Libraries and DLLs for windows platform
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
            PublicLibraryPaths.Add(GodraysDir + "/lib/x64");

            PublicAdditionalLibraries.Add("GFSDK_GodraysLib.x64.lib");
            PublicDelayLoadDLLs.Add("GFSDK_GodraysLib.x64.dll");

            string[] RuntimeDependenciesX64 =
			{
				"GFSDK_GodraysLib.x64.dll",
			};

            string GodraysBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/GameWorks/Godrays/Win64/");
            foreach (string RuntimeDependency in RuntimeDependenciesX64)
            {
                RuntimeDependencies.Add(new RuntimeDependency(GodraysBinariesDir + RuntimeDependency));
            }
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
            PublicLibraryPaths.Add(GodraysDir + "/lib/win32");

            PublicAdditionalLibraries.Add("GFSDK_GodraysLib.Win32.lib");
            PublicDelayLoadDLLs.Add("GFSDK_GodraysLib.Win32.dll");

            string[] RuntimeDependenciesX86 =
			{
				"GFSDK_GodraysLib.Win32.dll",
			};

            string GodraysBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/GameWorks/Godrays/Win32/");
            foreach (string RuntimeDependency in RuntimeDependenciesX86)
            {
                RuntimeDependencies.Add(new RuntimeDependency(GodraysBinariesDir + RuntimeDependency));
            }
        }
	}
}
