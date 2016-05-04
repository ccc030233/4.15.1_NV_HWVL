// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;

public class NVGodrays : ModuleRules
{
    public NVGodrays(TargetInfo Target)
	{
		Type = ModuleType.External;

        Definitions.Add("WITH_GAMEWORKS_NVGODRAYS=1");
		
		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			Definitions.Add("__GFSDK_DX11__=1");
		}
		
        string NVGodraysDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "GameWorks/NVGodrays/";

        PublicIncludePaths.Add(NVGodraysDir + "include");

		// Libraries and DLLs for windows platform
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
            PublicLibraryPaths.Add(NVGodraysDir + "/lib/x64");

            PublicAdditionalLibraries.Add("GFSDK_GodraysLib.x64.lib");
            PublicDelayLoadDLLs.Add("GFSDK_GodraysLib.x64.dll");

            string[] RuntimeDependenciesX64 =
			{
				"GFSDK_GodraysLib.x64.dll",
			};

            string NVGodraysBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/GameWorks/NVGodrays/Win64/");
            foreach (string RuntimeDependency in RuntimeDependenciesX64)
            {
                RuntimeDependencies.Add(new RuntimeDependency(NVGodraysBinariesDir + RuntimeDependency));
            }
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
            PublicLibraryPaths.Add(NVGodraysDir + "/lib/win32");

            PublicAdditionalLibraries.Add("GFSDK_GodraysLib.Win32.lib");
            PublicDelayLoadDLLs.Add("GFSDK_GodraysLib.Win32.dll");

            string[] RuntimeDependenciesX86 =
			{
				"GFSDK_GodraysLib.Win32.dll",
			};

            string NVGodraysBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/GameWorks/NVGodrays/Win32/");
            foreach (string RuntimeDependency in RuntimeDependenciesX86)
            {
                RuntimeDependencies.Add(new RuntimeDependency(NVGodraysBinariesDir + RuntimeDependency));
            }
        }
	}
}
