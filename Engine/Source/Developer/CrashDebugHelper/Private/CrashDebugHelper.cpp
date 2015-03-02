// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashDebugHelperPrivatePCH.h"
#include "CrashDebugPDBCache.h"

#include "EngineVersion.h"
#include "ISourceControlModule.h"
#include "ISourceControlLabel.h"
#include "ISourceControlRevision.h"

#include "../../../../Launch/Resources/Version.h"

#ifndef MINIDUMPDIAGNOSTICS
	#define MINIDUMPDIAGNOSTICS	0
#endif

const TCHAR* ICrashDebugHelper::P4_DEPOT_PREFIX = TEXT( "//depot/" );

bool ICrashDebugHelper::Init()
{
	bInitialized = true;

	// Check if we have a valid EngineVersion, if so use it.
	FString CmdEngineVersion;
	const bool bHasEngineVersion = FParse::Value( FCommandLine::Get(), TEXT( "EngineVersion=" ), CmdEngineVersion );
	if( bHasEngineVersion )
	{
		FEngineVersion EngineVersion;
		FEngineVersion::Parse( CmdEngineVersion, EngineVersion );

		// Clean branch name.
		CrashInfo.DepotName = EngineVersion.GetBranch();
		CrashInfo.BuiltFromCL = (int32)EngineVersion.GetChangelist();

		CrashInfo.EngineVersion = CmdEngineVersion;
	}
	else
	{
		// Look up the depot name
		// Try to use the command line param
		FString DepotName;
		FString CmdLineBranchName;
		if( FParse::Value( FCommandLine::Get(), TEXT( "BranchName=" ), CmdLineBranchName ) )
		{
			DepotName = FString::Printf( TEXT( "%s%s" ), P4_DEPOT_PREFIX, *CmdLineBranchName );
		}
		// Default to BRANCH_NAME
		else
		{
			DepotName = FString::Printf( TEXT( "%s%s" ), P4_DEPOT_PREFIX, TEXT( BRANCH_NAME ) );
		}

		CrashInfo.DepotName = DepotName;

		// Try to get the BuiltFromCL from command line to use this instead of attempting to locate the CL in the minidump
		FString CmdLineBuiltFromCL;
		int32 BuiltFromCL = -1;
		if( FParse::Value( FCommandLine::Get(), TEXT( "BuiltFromCL=" ), CmdLineBuiltFromCL ) )
		{
			if( !CmdLineBuiltFromCL.IsEmpty() )
			{
				BuiltFromCL = FCString::Atoi( *CmdLineBuiltFromCL );
			}
		}
		// Default to BUILT_FROM_CHANGELIST.
		else
		{
			BuiltFromCL = int32( BUILT_FROM_CHANGELIST );
		}

		CrashInfo.BuiltFromCL = BuiltFromCL;
	}
	
	GConfig->GetString( TEXT( "Engine.CrashDebugHelper" ), TEXT( "SourceControlBuildLabelPattern" ), SourceControlBuildLabelPattern, GEngineIni );

	GConfig->GetArray( TEXT( "Engine.CrashDebugHelper" ), TEXT( "ExecutablePathPattern" ), ExecutablePathPatterns, GEngineIni );
	GConfig->GetArray( TEXT( "Engine.CrashDebugHelper" ), TEXT( "SymbolPathPattern" ), SymbolPathPatterns, GEngineIni );
	GConfig->GetArray( TEXT( "Engine.CrashDebugHelper" ), TEXT( "Branch" ), Branches, GEngineIni );
	const bool bCanUseSearchPatterns = Branches.Num() == ExecutablePathPatterns.Num() && ExecutablePathPatterns.Num() == SymbolPathPatterns.Num() && Branches.Num() > 0;
	UE_CLOG( !bCanUseSearchPatterns, LogCrashDebugHelper, Warning, TEXT( "Search patterns don't match" ) );

	GConfig->GetString( TEXT( "Engine.CrashDebugHelper" ), TEXT( "DepotRoot" ), DepotRoot, GEngineIni );
	const bool bHasDepotRoot = IFileManager::Get().DirectoryExists( *DepotRoot );
	UE_CLOG( !bHasDepotRoot, LogCrashDebugHelper, Warning, TEXT( "DepotRoot: %s is not valid" ), *DepotRoot );

	if( bCanUseSearchPatterns && bHasDepotRoot )
	{
		FPDBCache::Get().Init();
	}
	else
	{
		UE_LOG( LogCrashDebugHelper, Warning, TEXT( "PDB Cache disabled" ) );
	}
	

	return bInitialized;
}

/** 
 * Initialise the source control interface, and ensure we have a valid connection
 */
bool ICrashDebugHelper::InitSourceControl(bool bShowLogin)
{
	// Ensure we are in a valid state to sync
	if (bInitialized == false)
	{
		UE_LOG(LogCrashDebugHelper, Warning, TEXT("InitSourceControl: CrashDebugHelper is not initialized properly."));
		return false;
	}

	// Initialize the source control if it hasn't already been
	if( !ISourceControlModule::Get().IsEnabled() || !ISourceControlModule::Get().GetProvider().IsAvailable() )
	{
		// make sure our provider is set to Perforce
		ISourceControlModule::Get().SetProvider("Perforce");

		// Attempt to load in a source control module
		ISourceControlModule::Get().GetProvider().Init();

#if !MINIDUMPDIAGNOSTICS
		if ((ISourceControlModule::Get().GetProvider().IsAvailable() == false) || bShowLogin)
		{
			// Unable to connect? Prompt the user for login information
			ISourceControlModule::Get().ShowLoginDialog(FSourceControlLoginClosed(), ELoginWindowMode::Modeless, EOnLoginWindowStartup::PreserveProvider);
		}
#endif
		// If it's still disabled, none was found, so exit
		if( !ISourceControlModule::Get().IsEnabled() || !ISourceControlModule::Get().GetProvider().IsAvailable() )
		{
			UE_LOG(LogCrashDebugHelper, Warning, TEXT("InitSourceControl: Source control unavailable or disabled."));
			return false;
		}
	}

	return true;
}

/** 
 * Shutdown the connection to source control
 */
void ICrashDebugHelper::ShutdownSourceControl()
{
	ISourceControlModule::Get().GetProvider().Close();
}


bool ICrashDebugHelper::SyncModules()
{
	// Check source control
	if( !ISourceControlModule::Get().IsEnabled() )
	{
		return false;
	}

	if( !FPDBCache::Get().UsePDBCache() )
	{
		UE_LOG( LogCrashDebugHelper, Warning, TEXT( "The PDB Cache is disabled, cannot proceed, %s" ), *CrashInfo.EngineVersion );
		return false;
	}

	// @TODO yrx 2015-02-23 Obsolete, remove after 4.8
	const TCHAR* UESymbols = TEXT( "Rocket/Symbols/" );
	const bool bHasExecutable = !CrashInfo.ExecutablesPath.IsEmpty();
	const bool bHasSymbols = !CrashInfo.SymbolsPath.IsEmpty();
	TArray< TSharedRef<ISourceControlLabel> > Labels = ISourceControlModule::Get().GetProvider().GetLabels( CrashInfo.LabelName );
	
	const bool bContainsProductVersion = FPDBCache::Get().ContainsPDBCacheEntry( CrashInfo.EngineVersion );
	if( bHasExecutable && bHasSymbols )
	{
		if( bContainsProductVersion )
		{
			UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Using cached storage: %s" ), *CrashInfo.EngineVersion );
			CrashInfo.PDBCacheEntry = FPDBCache::Get().FindAndTouchPDBCacheEntry( CrashInfo.EngineVersion );
		}
		else
		{
			SCOPE_LOG_TIME_IN_SECONDS( TEXT( "SyncExecutableAndSymbolsFromNetwork" ), nullptr );

			// Find all executables.
			TArray<FString> NetworkExecutables;
			IFileManager::Get().FindFilesRecursive( NetworkExecutables, *CrashInfo.ExecutablesPath, TEXT( "*.dll" ), true, false, false );
			IFileManager::Get().FindFilesRecursive( NetworkExecutables, *CrashInfo.ExecutablesPath, TEXT( "*.exe" ), true, false, false );

			// Find all symbols.
			TArray<FString> NetworkSymbols;
			IFileManager::Get().FindFilesRecursive( NetworkSymbols, *CrashInfo.SymbolsPath, TEXT( "*.pdb" ), true, false, false );

			// From=Full pathname
			// To=Relative pathname
			TMap<FString, FString> FilesToBeCached;

			for( const auto& ExecutablePath : NetworkExecutables )
			{
				const FString NetworkRelativePath = ExecutablePath.Replace( *CrashInfo.ExecutablesPath, TEXT( "" ) );
				FilesToBeCached.Add( ExecutablePath, NetworkRelativePath );
			}

			for( const auto& SymbolPath : NetworkSymbols )
			{
				const FString SymbolRelativePath = SymbolPath.Replace( *CrashInfo.SymbolsPath, TEXT( "" ) );
				FilesToBeCached.Add( SymbolPath, SymbolRelativePath );
			}

			// Initialize and add a new PDB Cache entry to the database.
			CrashInfo.PDBCacheEntry = FPDBCache::Get().CreateAndAddPDBCacheEntryMixed( CrashInfo.EngineVersion, FilesToBeCached );
		}
	}
	// Get all labels associated with the crash info's label.
	else if( Labels.Num() >= 1 )
	{
		TSharedRef<ISourceControlLabel> Label = Labels[0];
		TSet<FString> FilesToSync;

		// Use product version instead of label name to make a distinguish between chosen methods.
		const bool bContainsLabelName = FPDBCache::Get().ContainsPDBCacheEntry( CrashInfo.LabelName );

		if( bContainsProductVersion )
		{
			UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Using cached storage: %s" ), *CrashInfo.EngineVersion );
			CrashInfo.PDBCacheEntry = FPDBCache::Get().FindAndTouchPDBCacheEntry( CrashInfo.EngineVersion );
		}
		else if( bContainsLabelName )
		{
			UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Using cached storage: %s" ), *CrashInfo.LabelName );
			CrashInfo.PDBCacheEntry = FPDBCache::Get().FindAndTouchPDBCacheEntry( CrashInfo.LabelName );
		}
		else if( bHasExecutable )
		{			
			SCOPE_LOG_TIME_IN_SECONDS( TEXT( "SyncModulesAndNetwork" ), nullptr );

			// Grab information about symbols.
			TArray< TSharedRef<class ISourceControlRevision, ESPMode::ThreadSafe> > PDBSourceControlRevisions;
			const FString PDBsPath = FString::Printf( TEXT( "%s/%s....pdb" ), *CrashInfo.DepotName, UESymbols );
			Label->GetFileRevisions( PDBsPath, PDBSourceControlRevisions );

			TSet<FString> PDBPaths;
			for( const auto& PDBSrc : PDBSourceControlRevisions )
			{
				PDBPaths.Add( PDBSrc->GetFilename() );
			}

			// Now, sync symbols.
			for( const auto& PDBPath : PDBPaths )
			{
				if( Label->Sync( PDBPath ) )
				{
					UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Synced PDB: %s" ), *PDBPath );
				}
			}

			// Find all the executables in the product network path.
			TArray<FString> NetworkExecutables;
			IFileManager::Get().FindFilesRecursive( NetworkExecutables, *CrashInfo.ExecutablesPath, TEXT( "*.dll" ), true, false, false );
			IFileManager::Get().FindFilesRecursive( NetworkExecutables, *CrashInfo.ExecutablesPath, TEXT( "*.exe" ), true, false, false );

			// From=Full pathname
			// To=Relative pathname
			TMap<FString, FString> FilesToBeCached;

			// If a symbol matches an executable, add the pair to the list of files that should be cached.
			for( const auto& NetworkExecutableFullpath : NetworkExecutables )
			{
				for( const auto& PDBPath : PDBPaths )
				{
					const FString PDBRelativePath = PDBPath.Replace( *CrashInfo.DepotName, TEXT( "" ) ).Replace( UESymbols, TEXT( "" ) );
					const FString PDBFullpath = DepotRoot / PDBPath.Replace( P4_DEPOT_PREFIX, TEXT( "" ) );

					const FString PDBMatch = PDBRelativePath.Replace( TEXT( "pdb" ), TEXT( "" ) );
					const FString NetworkRelativePath = NetworkExecutableFullpath.Replace( *CrashInfo.ExecutablesPath, TEXT( "" ) );
					const bool bMatch = NetworkExecutableFullpath.Contains( PDBMatch );
					if( bMatch )
					{
						// From -> Where
						FilesToBeCached.Add( NetworkExecutableFullpath, NetworkRelativePath );
						FilesToBeCached.Add( PDBFullpath, PDBRelativePath );
						break;
					}
				}
			}

			// Initialize and add a new PDB Cache entry to the database.
			CrashInfo.PDBCacheEntry = FPDBCache::Get().CreateAndAddPDBCacheEntryMixed( CrashInfo.EngineVersion, FilesToBeCached );
		}
		else
		{
			TArray<FString> FilesToBeCached;
			
			//@TODO: MAC: Excluding labels for Mac since we are only syncing windows binaries here...
			if( Label->GetName().Contains( TEXT( "Mac" ) ) )
			{
				UE_LOG( LogCrashDebugHelper, Log, TEXT( "Skipping Mac label: %s" ), *Label->GetName() );
			}
			else
			{
				// Sync all the dll, exes, and related symbol files
				UE_LOG( LogCrashDebugHelper, Log, TEXT( "Syncing modules with label: %s" ), *Label->GetName() );

				SCOPE_LOG_TIME_IN_SECONDS( TEXT( "SyncModules" ), nullptr );

				// Grab all dll and pdb files for the specified label.
				TArray< TSharedRef<class ISourceControlRevision, ESPMode::ThreadSafe> > DLLSourceControlRevisions;
				const FString DLLsPath = FString::Printf( TEXT( "%s/....dll" ), *CrashInfo.DepotName );
				Label->GetFileRevisions( DLLsPath, DLLSourceControlRevisions );

				TArray< TSharedRef<class ISourceControlRevision, ESPMode::ThreadSafe> > EXESourceControlRevisions;
				const FString EXEsPath = FString::Printf( TEXT( "%s/....exe" ), *CrashInfo.DepotName );
				Label->GetFileRevisions( EXEsPath, EXESourceControlRevisions );

				TArray< TSharedRef<class ISourceControlRevision, ESPMode::ThreadSafe> > PDBSourceControlRevisions;
				const FString PDBsPath = FString::Printf( TEXT( "%s/....pdb" ), *CrashInfo.DepotName );
				Label->GetFileRevisions( PDBsPath, PDBSourceControlRevisions );

				TSet<FString> ModulesPaths;
				for( const auto& DLLSrc : DLLSourceControlRevisions )
				{
					ModulesPaths.Add( DLLSrc->GetFilename().Replace( *CrashInfo.DepotName, TEXT( "" ) ) );
				}
				for( const auto& EXESrc : EXESourceControlRevisions )
				{
					ModulesPaths.Add( EXESrc->GetFilename().Replace( *CrashInfo.DepotName, TEXT( "" ) ) );
				}

				TSet<FString> PDBPaths;
				for( const auto& PDBSrc : PDBSourceControlRevisions )
				{
					PDBPaths.Add( PDBSrc->GetFilename().Replace( *CrashInfo.DepotName, TEXT( "" ) ) );
				}

				// Iterate through all module and see if we have dll and pdb associated with the module, if so add it to the files to sync.
				for( const auto& ModuleName : CrashInfo.ModuleNames )
				{
					const FString ModuleNamePDB = ModuleName.Replace( TEXT( ".dll" ), TEXT( ".pdb" ) ).Replace( TEXT( ".exe" ), TEXT( ".pdb" ) );

					for( const auto& ModulePath : ModulesPaths )
					{
						const bool bContainsModule = ModulePath.Contains( ModuleName );
						if( bContainsModule )
						{
							FilesToSync.Add( ModulePath );
						}
					}

					for( const auto& PDBPath : PDBPaths )
					{
						const bool bContainsPDB = PDBPath.Contains( ModuleNamePDB );
						if( bContainsPDB )
						{
							FilesToSync.Add( PDBPath );
						}
					}
				}

				// Now, sync all files.
				for( const auto& Filename : FilesToSync )
				{
					const FString DepotPath = CrashInfo.DepotName + Filename;
					if( Label->Sync( DepotPath ) )
					{
						UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Synced binary: %s" ), *DepotPath );
					}
					FilesToBeCached.Add( DepotPath );
				}
			}

			// Initialize and add a new PDB Cache entry to the database.
			CrashInfo.PDBCacheEntry = FPDBCache::Get().CreateAndAddPDBCacheEntry( CrashInfo.LabelName, DepotRoot, CrashInfo.DepotName, FilesToBeCached );
		}
	}
	else
	{
		UE_LOG( LogCrashDebugHelper, Error, TEXT( "Could not find label: %s"), *CrashInfo.LabelName );
		return false;
	}

	return true;
}

bool ICrashDebugHelper::SyncSourceFile()
{
	// Check source control
	if( !ISourceControlModule::Get().IsEnabled() )
	{
		return false;
	}

	// Sync a single source file to requested CL.
	FString DepotPath = CrashInfo.DepotName / CrashInfo.SourceFile + TEXT( "@" ) + TTypeToString<int32>::ToString( CrashInfo.BuiltFromCL );
	ISourceControlModule::Get().GetProvider().Execute(ISourceControlOperation::Create<FSync>(), DepotPath);

	UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Syncing a single source file: %s"), *DepotPath );

	return true;
}


bool ICrashDebugHelper::ReadSourceFile( const TCHAR* InFilename, TArray<FString>& OutStrings )
{
	FString Line;
	if( FFileHelper::LoadFileToString( Line, InFilename ) )
	{
		Line = Line.Replace( TEXT( "\r" ), TEXT( "" ) );
		Line.ParseIntoArray( &OutStrings, TEXT( "\n" ), false );
		
		return true;
	}
	else
	{
		UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Failed to open source file %s" ), InFilename );
		return false;
	}
}

void ICrashDebugHelper::AddSourceToReport()
{
	if( CrashInfo.SourceFile.Len() > 0 && CrashInfo.SourceLineNumber != 0 )
	{
		TArray<FString> Lines;
		FString FullPath = FString( TEXT( "../../../" ) ) + CrashInfo.SourceFile;
		ReadSourceFile( *FullPath, Lines );

		const uint32 MinLine = FMath::Clamp( CrashInfo.SourceLineNumber - 15, (uint32)1, (uint32)Lines.Num() );
		const uint32 MaxLine = FMath::Clamp( CrashInfo.SourceLineNumber + 15, (uint32)1, (uint32)Lines.Num() );

		for( uint32 Line = MinLine; Line < MaxLine; Line++ )
		{
			if( Line == CrashInfo.SourceLineNumber - 1 )
			{
				CrashInfo.SourceContext.Add( FString( TEXT( "*****" ) ) + Lines[Line] );
			}
			else
			{
				CrashInfo.SourceContext.Add( FString( TEXT( "     " ) ) + Lines[Line] );
			}
		}
	}
}

bool ICrashDebugHelper::AddAnnotatedSourceToReport()
{
	// Make sure we have a source file to interrogate
	if( CrashInfo.SourceFile.Len() > 0 && CrashInfo.SourceLineNumber != 0 && !CrashInfo.LabelName.IsEmpty() )
	{
		// Check source control
		if( !ISourceControlModule::Get().IsEnabled() )
		{
			return false;
		}

		// Ask source control to annotate the file for us
		FString DepotPath = CrashInfo.DepotName / CrashInfo.SourceFile;

		TArray<FAnnotationLine> Lines;
		SourceControlHelpers::AnnotateFile( ISourceControlModule::Get().GetProvider(), CrashInfo.LabelName, DepotPath, Lines );

		uint32 MinLine = FMath::Clamp( CrashInfo.SourceLineNumber - 15, (uint32)1, (uint32)Lines.Num() );
		uint32 MaxLine = FMath::Clamp( CrashInfo.SourceLineNumber + 15, (uint32)1, (uint32)Lines.Num() );

		// Display a source context in the report, and decorate each line with the last editor of the line
		for( uint32 Line = MinLine; Line < MaxLine; Line++ )
		{			
			if( Line == CrashInfo.SourceLineNumber )
			{
				CrashInfo.SourceContext.Add( FString::Printf( TEXT( "*****%20s: %s" ), *Lines[Line].UserName, *Lines[Line].Line ) );
			}
			else
			{
				CrashInfo.SourceContext.Add( FString::Printf( TEXT( "     %20s: %s" ), *Lines[Line].UserName, *Lines[Line].Line ) );
			}
		}
		return true;
	}

	return false;
}

void FCrashInfo::Log( FString Line )
{
	UE_LOG( LogCrashDebugHelper, Warning, TEXT("%s"), *Line );
	Report += Line + LINE_TERMINATOR;
}


const TCHAR* FCrashInfo::GetProcessorArchitecture( EProcessorArchitecture PA )
{
	switch( PA )
	{
	case PA_X86:
		return TEXT( "x86" );
	case PA_X64:
		return TEXT( "x64" );
	case PA_ARM:
		return TEXT( "ARM" );
	}

	return TEXT( "Unknown" );
}


int64 FCrashInfo::StringSize( const ANSICHAR* Line )
{
	int64 Size = 0;
	if( Line != nullptr )
	{
		while( *Line++ != 0 )
		{
			Size++;
		}
	}
	return Size;
}


void FCrashInfo::WriteLine( FArchive* ReportFile, const ANSICHAR* Line )
{
	if( Line != NULL )
	{
		int64 StringBytes = StringSize( Line );
		ReportFile->Serialize( ( void* )Line, StringBytes );
	}

	ReportFile->Serialize( TCHAR_TO_UTF8( LINE_TERMINATOR ), FCStringWide::Strlen(LINE_TERMINATOR) );
}


void FCrashInfo::GenerateReport( const FString& DiagnosticsPath )
{
	FArchive* ReportFile = IFileManager::Get().CreateFileWriter( *DiagnosticsPath );
	if( ReportFile != NULL )
	{
		FString Line;

		WriteLine( ReportFile, TCHAR_TO_UTF8( TEXT( "Generating report for minidump" ) ) );
		WriteLine( ReportFile );

		if ( EngineVersion.Len() > 0 )
		{
			Line = FString::Printf( TEXT( "Application version %s" ), *EngineVersion );
			WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		}
		else if( Modules.Num() > 0 )
		{
			Line = FString::Printf( TEXT( "Application version %d.%d.%d" ), Modules[0].Major, Modules[0].Minor, Modules[0].Patch );
			WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		}

		Line = FString::Printf( TEXT( " ... built from changelist %d" ), BuiltFromCL );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		if( LabelName.Len() > 0 )
		{
			Line = FString::Printf( TEXT( " ... based on label %s" ), *LabelName );
			WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		}
		WriteLine( ReportFile );

		Line = FString::Printf( TEXT( "OS version %d.%d.%d.%d" ), SystemInfo.OSMajor, SystemInfo.OSMinor, SystemInfo.OSBuild, SystemInfo.OSRevision );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );

		Line = FString::Printf( TEXT( "Running %d %s processors" ), SystemInfo.ProcessorCount, GetProcessorArchitecture( SystemInfo.ProcessorArchitecture ) );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );

		Line = FString::Printf( TEXT( "Exception was \"%s\"" ), *Exception.ExceptionString );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		WriteLine( ReportFile );

		Line = FString::Printf( TEXT( "Source context from \"%s\"" ), *SourceFile );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		WriteLine( ReportFile );

		Line = FString::Printf( TEXT( "<SOURCE START>" ) );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		for( int32 LineIndex = 0; LineIndex < SourceContext.Num(); LineIndex++ )
		{
			Line = FString::Printf( TEXT( "%s" ), *SourceContext[LineIndex] );
			WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		}
		Line = FString::Printf( TEXT( "<SOURCE END>" ) );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		WriteLine( ReportFile );

		Line = FString::Printf( TEXT( "<CALLSTACK START>" ) );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );

		for( int32 StackIndex = 0; StackIndex < Exception.CallStackString.Num(); StackIndex++ )
		{
			Line = FString::Printf( TEXT( "%s" ), *Exception.CallStackString[StackIndex] );
			WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		}

		Line = FString::Printf( TEXT( "<CALLSTACK END>" ) );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		WriteLine( ReportFile );

		Line = FString::Printf( TEXT( "%d loaded modules" ), Modules.Num() );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );

		for( int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ModuleIndex++ )
		{
			FCrashModuleInfo& Module = Modules[ModuleIndex];

			FString ModuleDirectory = FPaths::GetPath(Module.Name);
			FString ModuleName = FPaths::GetBaseFilename( Module.Name, true ) + FPaths::GetExtension( Module.Name, true );

			FString ModuleDetail = FString::Printf( TEXT( "%40s" ), *ModuleName );
			FString Version = FString::Printf( TEXT( " (%d.%d.%d.%d)" ), Module.Major, Module.Minor, Module.Patch, Module.Revision );
			ModuleDetail += FString::Printf( TEXT( " %22s" ), *Version );
			ModuleDetail += FString::Printf( TEXT( " 0x%016x 0x%08x" ), Module.BaseOfImage, Module.SizeOfImage );
			ModuleDetail += FString::Printf( TEXT( " %s" ), *ModuleDirectory );

			WriteLine( ReportFile, TCHAR_TO_UTF8( *ModuleDetail ) );
		}

		WriteLine( ReportFile );

		// Write out the processor debugging log
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Report ) );

		Line = FString::Printf( TEXT( "Report end!" ) );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line )  );

		ReportFile->Close();
		delete ReportFile;
	}
}

bool ICrashDebugHelper::SyncRequiredFilesForDebuggingFromLabel(const FString& InLabel, const FString& InPlatform)
{
	// @TODO yrx 2015-02-19 Use PDB cache

	return false;
}

bool ICrashDebugHelper::SyncRequiredFilesForDebuggingFromChangelist(int32 InChangelistNumber, const FString& InPlatform)
{
	// @TODO yrx 2015-02-19 Use PDB cache

	return false;
}

void ICrashDebugHelper::FindSymbolsAndBinariesStorage()
{
	CrashInfo.ExecutablesPath.Empty();
	CrashInfo.SymbolsPath.Empty();
	CrashInfo.LabelName.Empty();

	if( CrashInfo.BuiltFromCL == FCrashInfo::INVALID_CHANGELIST )
	{
		UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Invalid parameters" ) );
		return;
	}

	UE_LOG( LogCrashDebugHelper, Log, TEXT( "Engine version: %s" ), *CrashInfo.EngineVersion );

	int32 Index = 0;
	bool bFoundPattern = false;
	FString ExecutablePathPattern;
	FString SymbolPathPattern;
	// Find branch.
	for( ; Index < Branches.Num(); Index++ )
	{
		if( CrashInfo.DepotName.Contains( Branches[Index] ) )
		{
			bFoundPattern = true;
			ExecutablePathPattern = ExecutablePathPatterns[Index];
			SymbolPathPattern = SymbolPathPatterns[Index];
			break;
		}
	}

	if( bFoundPattern )
	{
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "Using branch: %s" ), *CrashInfo.DepotName );
	}
	else
	{
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "Branch not found: %s" ), *CrashInfo.DepotName );
		return;
	}

	// %ENGINE_VERSION% - Engine versions ie.: 4.7.0-2449961+UE4-Releases+4.7
	// %PLATFORM_NAME% - Platform name ie.: WindowsNoEditor, Win64 etc.
	// %UT_ENGINE_VERSION% - UT ie.: ++depot+UE4-UT-CL-2454691
	//\\epicgames.net\root\Builds\UnrealTournament\++depot+UE4-UT-CL-2417639
	//\\epicgames.net\root\Builds\Rocket\Automated\4.7.0-2449961+++depot+UE4-Releases+4.7
	//\\epicgames.net\root\Builds\UnrealEngineLauncher
	
	const FString StrENGINE_VERSION = CrashInfo.EngineVersion;
	const FString StrPLATFORM_NAME = TEXT( "" ); // Not implemented yet
	const FString StrUT_ENGINE_VERSION = FString::Printf( TEXT( "++depot+UE4-UT-CL-%i" ), CrashInfo.BuiltFromCL );

	const FString TestExecutablesPath = ExecutablePathPattern
		.Replace( TEXT( "%ENGINE_VERSION%" ), *StrENGINE_VERSION )
		.Replace( TEXT( "%PLATFORM_NAME%" ), *StrPLATFORM_NAME )
		.Replace( TEXT( "%UT_ENGINE_VERSION%" ), *StrUT_ENGINE_VERSION );

	const FString TestSymbolsPath = SymbolPathPattern
		.Replace( TEXT( "%ENGINE_VERSION%" ), *StrENGINE_VERSION )
		.Replace( TEXT( "%PLATFORM_NAME%" ), *StrPLATFORM_NAME )
		.Replace( TEXT( "%UT_ENGINE_VERSION%" ), *StrUT_ENGINE_VERSION );

	// Try to find the network path by using the pattern supplied via ini.
	// If this step successes, we will grab the executable from the network path instead of P4.
	bool bFoundDirectory = false;
	
	const bool bHasExecutables = IFileManager::Get().DirectoryExists( *TestExecutablesPath );
	const bool bHasSymbols = IFileManager::Get().DirectoryExists( *TestSymbolsPath );

	if( bHasExecutables && bHasSymbols )
	{
		CrashInfo.ExecutablesPath = TestExecutablesPath;
		CrashInfo.SymbolsPath = TestSymbolsPath;
		bFoundDirectory = true;
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "Using path for executables and symbols: %s" ), *CrashInfo.ExecutablesPath );
	}
	else if( bHasExecutables )
	{
		CrashInfo.ExecutablesPath = TestExecutablesPath;
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "Using path for executables: %s" ), *CrashInfo.ExecutablesPath );
	}
	else
	{
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "Path for executables not found: %s" ), *TestExecutablesPath );
	}
	

	// Try to find the label directly in source control by using the pattern supplied via ini.
	if( !bFoundDirectory && !SourceControlBuildLabelPattern.IsEmpty() )
	{	
		const FString ChangelistString = FString::Printf( TEXT( "%d" ), CrashInfo.BuiltFromCL );
		const FString LabelWithCL = SourceControlBuildLabelPattern.Replace( TEXT( "%CHANGELISTNUMBER%" ), *ChangelistString, ESearchCase::CaseSensitive );
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "Label matching pattern: %s" ), *LabelWithCL );

		TArray< TSharedRef<ISourceControlLabel> > Labels = ISourceControlModule::Get().GetProvider().GetLabels( LabelWithCL );
		if( Labels.Num() > 0 )
		{
			const int32 LabelIndex = 0;
			CrashInfo.LabelName = Labels[LabelIndex]->GetName();;
		
			// If we found more than one label, warn about it and just use the first one
			if( Labels.Num() > 1 )
			{
				UE_LOG( LogCrashDebugHelper, Warning, TEXT( "More than one build label found, using label: %s" ), *LabelWithCL, *CrashInfo.LabelName );
			}
			else
			{
				UE_LOG( LogCrashDebugHelper, Log, TEXT( "Using label: %s" ), *CrashInfo.LabelName );
			}		
		}
	}
}
