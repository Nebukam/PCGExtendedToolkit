// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExValencyLog.h"

#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogValency, Log, All);

// Static member initialization
EPCGExValencyLogVerbosity FPCGExValencyLog::CategoryVerbosity[static_cast<uint8>(EPCGExValencyLogCategory::MAX)] = {};
TArray<FString> FPCGExValencyLog::ReportLines;
bool FPCGExValencyLog::bReportActive = false;
FCriticalSection FPCGExValencyLog::LogMutex;

// Category names
static const TCHAR* GCategoryNames[] = {
	TEXT("Building"),
	TEXT("Compilation"),
	TEXT("Solver"),
	TEXT("Staging"),
	TEXT("EditorMode"),
	TEXT("Cages"),
	TEXT("Mirror"),
};
static_assert(UE_ARRAY_COUNT(GCategoryNames) == static_cast<uint8>(EPCGExValencyLogCategory::MAX), "Category names must match enum count");

// Verbosity names
static const TCHAR* GVerbosityNames[] = {
	TEXT("Off"),
	TEXT("Error"),
	TEXT("Warning"),
	TEXT("Info"),
	TEXT("Verbose"),
};

const TCHAR* FPCGExValencyLog::GetCategoryName(EPCGExValencyLogCategory Category)
{
	const uint8 Index = static_cast<uint8>(Category);
	if (Index < UE_ARRAY_COUNT(GCategoryNames))
	{
		return GCategoryNames[Index];
	}
	return TEXT("Unknown");
}

const TCHAR* FPCGExValencyLog::GetVerbosityName(EPCGExValencyLogVerbosity Verbosity)
{
	const uint8 Index = static_cast<uint8>(Verbosity);
	if (Index < UE_ARRAY_COUNT(GVerbosityNames))
	{
		return GVerbosityNames[Index];
	}
	return TEXT("Unknown");
}

void FPCGExValencyLog::Log(EPCGExValencyLogCategory Category, EPCGExValencyLogVerbosity Verbosity, const FString& Message)
{
	const uint8 CategoryIndex = static_cast<uint8>(Category);
	if (CategoryIndex >= static_cast<uint8>(EPCGExValencyLogCategory::MAX))
	{
		return;
	}

	// Check verbosity threshold
	const EPCGExValencyLogVerbosity CurrentVerbosity = CategoryVerbosity[CategoryIndex];
	if (static_cast<uint8>(Verbosity) > static_cast<uint8>(CurrentVerbosity))
	{
		return;
	}

	// Format the message with category and verbosity prefix
	const FString FormattedMessage = FString::Printf(TEXT("[%s][%s] %s"),
		GetCategoryName(Category),
		GetVerbosityName(Verbosity),
		*Message);

	// Output to UE_LOG based on verbosity
	switch (Verbosity)
	{
	case EPCGExValencyLogVerbosity::Error:
		UE_LOG(LogValency, Error, TEXT("%s"), *FormattedMessage);
		break;
	case EPCGExValencyLogVerbosity::Warning:
		UE_LOG(LogValency, Warning, TEXT("%s"), *FormattedMessage);
		break;
	case EPCGExValencyLogVerbosity::Info:
		UE_LOG(LogValency, Log, TEXT("%s"), *FormattedMessage);
		break;
	case EPCGExValencyLogVerbosity::Verbose:
		UE_LOG(LogValency, Log, TEXT("%s"), *FormattedMessage);  // Use Log instead of Verbose so it actually shows
		break;
	default:
		break;
	}

	// Accumulate to report if active
	if (bReportActive)
	{
		FScopeLock Lock(&LogMutex);
		ReportLines.Add(FormattedMessage);
	}
}

void FPCGExValencyLog::SetVerbosity(EPCGExValencyLogCategory Category, EPCGExValencyLogVerbosity Verbosity)
{
	const uint8 CategoryIndex = static_cast<uint8>(Category);
	if (CategoryIndex < static_cast<uint8>(EPCGExValencyLogCategory::MAX))
	{
		CategoryVerbosity[CategoryIndex] = Verbosity;
		UE_LOG(LogValency, Log, TEXT("Valency log verbosity for '%s' set to '%s'"),
			GetCategoryName(Category), GetVerbosityName(Verbosity));
	}
}

EPCGExValencyLogVerbosity FPCGExValencyLog::GetVerbosity(EPCGExValencyLogCategory Category)
{
	const uint8 CategoryIndex = static_cast<uint8>(Category);
	if (CategoryIndex < static_cast<uint8>(EPCGExValencyLogCategory::MAX))
	{
		return CategoryVerbosity[CategoryIndex];
	}
	return EPCGExValencyLogVerbosity::Off;
}

void FPCGExValencyLog::SetAllVerbosity(EPCGExValencyLogVerbosity Verbosity)
{
	for (uint8 i = 0; i < static_cast<uint8>(EPCGExValencyLogCategory::MAX); ++i)
	{
		CategoryVerbosity[i] = Verbosity;
	}
	UE_LOG(LogValency, Log, TEXT("Valency log verbosity for ALL categories set to '%s'"), GetVerbosityName(Verbosity));
}

bool FPCGExValencyLog::WouldLog(EPCGExValencyLogCategory Category, EPCGExValencyLogVerbosity Verbosity)
{
	const uint8 CategoryIndex = static_cast<uint8>(Category);
	if (CategoryIndex >= static_cast<uint8>(EPCGExValencyLogCategory::MAX))
	{
		return false;
	}

	const EPCGExValencyLogVerbosity CurrentVerbosity = CategoryVerbosity[CategoryIndex];
	return static_cast<uint8>(Verbosity) <= static_cast<uint8>(CurrentVerbosity);
}

void FPCGExValencyLog::BeginReport()
{
	FScopeLock Lock(&LogMutex);
	ReportLines.Empty();
	bReportActive = true;
}

FString FPCGExValencyLog::EndReport()
{
	FScopeLock Lock(&LogMutex);
	bReportActive = false;

	FString Result = FString::Join(ReportLines, TEXT("\n"));
	ReportLines.Empty();
	return Result;
}

bool FPCGExValencyLog::IsReportActive()
{
	return bReportActive;
}

void FPCGExValencyLog::ClearReport()
{
	FScopeLock Lock(&LogMutex);
	ReportLines.Empty();
}

FString FPCGExValencyLog::GetCurrentReport()
{
	FScopeLock Lock(&LogMutex);
	return FString::Join(ReportLines, TEXT("\n"));
}

//~ Console Commands

static FAutoConsoleCommand GValencyLogSetVerbosityCmd(
	TEXT("PCGEx.Valency.Log.SetVerbosity"),
	TEXT("Set verbosity for a Valency log category. Usage: PCGEx.Valency.Log.SetVerbosity <Category> <Verbosity>\n")
	TEXT("  Categories: Building, Compilation, Solver, Staging, EditorMode, Cages, Mirror, All\n")
	TEXT("  Verbosity: Off, Error, Warning, Info, Verbose"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogValency, Warning, TEXT("Usage: PCGEx.Valency.Log.SetVerbosity <Category> <Verbosity>"));
			UE_LOG(LogValency, Warning, TEXT("  Categories: Building, Compilation, Solver, Staging, EditorMode, Cages, All"));
			UE_LOG(LogValency, Warning, TEXT("  Verbosity: Off, Error, Warning, Info, Verbose"));
			return;
		}

		const FString& CategoryStr = Args[0];
		const FString& VerbosityStr = Args[1];

		// Parse verbosity
		EPCGExValencyLogVerbosity Verbosity = EPCGExValencyLogVerbosity::Off;
		if (VerbosityStr.Equals(TEXT("Off"), ESearchCase::IgnoreCase))
		{
			Verbosity = EPCGExValencyLogVerbosity::Off;
		}
		else if (VerbosityStr.Equals(TEXT("Error"), ESearchCase::IgnoreCase))
		{
			Verbosity = EPCGExValencyLogVerbosity::Error;
		}
		else if (VerbosityStr.Equals(TEXT("Warning"), ESearchCase::IgnoreCase))
		{
			Verbosity = EPCGExValencyLogVerbosity::Warning;
		}
		else if (VerbosityStr.Equals(TEXT("Info"), ESearchCase::IgnoreCase))
		{
			Verbosity = EPCGExValencyLogVerbosity::Info;
		}
		else if (VerbosityStr.Equals(TEXT("Verbose"), ESearchCase::IgnoreCase))
		{
			Verbosity = EPCGExValencyLogVerbosity::Verbose;
		}
		else
		{
			UE_LOG(LogValency, Warning, TEXT("Unknown verbosity: %s"), *VerbosityStr);
			return;
		}

		// Parse category
		if (CategoryStr.Equals(TEXT("All"), ESearchCase::IgnoreCase))
		{
			FPCGExValencyLog::SetAllVerbosity(Verbosity);
		}
		else if (CategoryStr.Equals(TEXT("Building"), ESearchCase::IgnoreCase))
		{
			FPCGExValencyLog::SetVerbosity(EPCGExValencyLogCategory::Building, Verbosity);
		}
		else if (CategoryStr.Equals(TEXT("Compilation"), ESearchCase::IgnoreCase))
		{
			FPCGExValencyLog::SetVerbosity(EPCGExValencyLogCategory::Compilation, Verbosity);
		}
		else if (CategoryStr.Equals(TEXT("Solver"), ESearchCase::IgnoreCase))
		{
			FPCGExValencyLog::SetVerbosity(EPCGExValencyLogCategory::Solver, Verbosity);
		}
		else if (CategoryStr.Equals(TEXT("Staging"), ESearchCase::IgnoreCase))
		{
			FPCGExValencyLog::SetVerbosity(EPCGExValencyLogCategory::Staging, Verbosity);
		}
		else if (CategoryStr.Equals(TEXT("EditorMode"), ESearchCase::IgnoreCase))
		{
			FPCGExValencyLog::SetVerbosity(EPCGExValencyLogCategory::EditorMode, Verbosity);
		}
		else if (CategoryStr.Equals(TEXT("Cages"), ESearchCase::IgnoreCase))
		{
			FPCGExValencyLog::SetVerbosity(EPCGExValencyLogCategory::Cages, Verbosity);
		}
		else if (CategoryStr.Equals(TEXT("Mirror"), ESearchCase::IgnoreCase))
		{
			FPCGExValencyLog::SetVerbosity(EPCGExValencyLogCategory::Mirror, Verbosity);
		}
		else
		{
			UE_LOG(LogValency, Warning, TEXT("Unknown category: %s"), *CategoryStr);
		}
	})
);

static FAutoConsoleCommand GValencyLogShowVerbosityCmd(
	TEXT("PCGEx.Valency.Log.ShowVerbosity"),
	TEXT("Show current verbosity settings for all Valency log categories."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UE_LOG(LogValency, Log, TEXT("Valency Log Verbosity Settings:"));
		for (uint8 i = 0; i < static_cast<uint8>(EPCGExValencyLogCategory::MAX); ++i)
		{
			const EPCGExValencyLogCategory Category = static_cast<EPCGExValencyLogCategory>(i);
			const EPCGExValencyLogVerbosity Verbosity = FPCGExValencyLog::GetVerbosity(Category);
			UE_LOG(LogValency, Log, TEXT("  %s: %s"),
				FPCGExValencyLog::GetCategoryName(Category),
				FPCGExValencyLog::GetVerbosityName(Verbosity));
		}
	})
);

static FAutoConsoleCommand GValencyLogEnableAllCmd(
	TEXT("PCGEx.Valency.Log.EnableAll"),
	TEXT("Enable verbose logging for all Valency categories (shortcut for SetVerbosity All Verbose)."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		FPCGExValencyLog::SetAllVerbosity(EPCGExValencyLogVerbosity::Verbose);
	})
);

static FAutoConsoleCommand GValencyLogDisableAllCmd(
	TEXT("PCGEx.Valency.Log.DisableAll"),
	TEXT("Disable logging for all Valency categories (shortcut for SetVerbosity All Off)."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		FPCGExValencyLog::SetAllVerbosity(EPCGExValencyLogVerbosity::Off);
	})
);
