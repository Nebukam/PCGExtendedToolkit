// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExValencyLog.generated.h"

/**
 * Log categories for different Valency subsystems.
 */
UENUM(BlueprintType)
enum class EPCGExValencyLogCategory : uint8
{
	/** Rule building from cages */
	Building,
	/** BondingRules compilation */
	Compilation,
	/** Solver operations (candidate selection, constraints) */
	Solver,
	/** Staging node (state building, output writing) */
	Staging,
	/** Editor mode operations */
	EditorMode,
	/** Cage detection and connections */
	Cages,
	/** Mirror source resolution and asset inheritance */
	Mirror,
	/** Rebuild flow and dirty state cascade */
	Rebuild,

	MAX UMETA(Hidden)
};

/**
 * Verbosity levels for logging.
 */
UENUM(BlueprintType)
enum class EPCGExValencyLogVerbosity : uint8
{
	/** No logging */
	Off = 0,
	/** Errors only */
	Error,
	/** Errors + warnings */
	Warning,
	/** Errors + warnings + general info */
	Info,
	/** Everything including detailed debug info */
	Verbose
};

/**
 * Static logger for the Valency system.
 *
 * Provides per-category verbosity control and optional report accumulation.
 * All categories are Off by default.
 *
 * Usage:
 *   VALENCY_LOG(Building, Info, "Found %d cages", CageCount);
 *   VALENCY_LOG(Solver, Verbose, "Module[%d] mask=0x%llX", Index, Mask);
 *
 * For user reports:
 *   FPCGExValencyLog::BeginReport();
 *   // ... operations ...
 *   FString Report = FPCGExValencyLog::EndReport();
 */
class PCGEXELEMENTSVALENCY_API FPCGExValencyLog
{
public:
	/**
	 * Log a message with the specified category and verbosity.
	 * Message is only output if category verbosity >= message verbosity.
	 */
	static void Log(EPCGExValencyLogCategory Category, EPCGExValencyLogVerbosity Verbosity, const FString& Message);

	/** Set verbosity for a specific category */
	static void SetVerbosity(EPCGExValencyLogCategory Category, EPCGExValencyLogVerbosity Verbosity);

	/** Get current verbosity for a category */
	static EPCGExValencyLogVerbosity GetVerbosity(EPCGExValencyLogCategory Category);

	/** Set verbosity for all categories at once */
	static void SetAllVerbosity(EPCGExValencyLogVerbosity Verbosity);

	/** Check if a message would be logged (useful for expensive string formatting) */
	static bool WouldLog(EPCGExValencyLogCategory Category, EPCGExValencyLogVerbosity Verbosity);

	/** Get category name as string */
	static const TCHAR* GetCategoryName(EPCGExValencyLogCategory Category);

	/** Get verbosity name as string */
	static const TCHAR* GetVerbosityName(EPCGExValencyLogVerbosity Verbosity);

	//~ Report Accumulation

	/**
	 * Begin accumulating log messages into a report.
	 * While active, all logged messages are also stored for later retrieval.
	 */
	static void BeginReport();

	/**
	 * End report accumulation and return the accumulated messages.
	 * @return All messages logged since BeginReport(), formatted as a multi-line string
	 */
	static FString EndReport();

	/** Check if report accumulation is currently active */
	static bool IsReportActive();

	/** Clear current report without ending accumulation */
	static void ClearReport();

	/** Get current report contents without ending accumulation */
	static FString GetCurrentReport();

	//~ Convenience methods

	static void Error(EPCGExValencyLogCategory Category, const FString& Message)
	{
		Log(Category, EPCGExValencyLogVerbosity::Error, Message);
	}

	static void Warning(EPCGExValencyLogCategory Category, const FString& Message)
	{
		Log(Category, EPCGExValencyLogVerbosity::Warning, Message);
	}

	static void Info(EPCGExValencyLogCategory Category, const FString& Message)
	{
		Log(Category, EPCGExValencyLogVerbosity::Info, Message);
	}

	static void Verbose(EPCGExValencyLogCategory Category, const FString& Message)
	{
		Log(Category, EPCGExValencyLogVerbosity::Verbose, Message);
	}

private:
	static EPCGExValencyLogVerbosity CategoryVerbosity[static_cast<uint8>(EPCGExValencyLogCategory::MAX)];
	static TArray<FString> ReportLines;
	static bool bReportActive;
	static FCriticalSection LogMutex;
};

//~ Logging Macros

/** Main logging macro - VALENCY_LOG(Category, Verbosity, Format, ...) */
#define PCGEX_VALENCY_LOG(Category, Verbosity, Format, ...) \
	do { \
		if (FPCGExValencyLog::WouldLog(EPCGExValencyLogCategory::Category, EPCGExValencyLogVerbosity::Verbosity)) \
		{ \
			FPCGExValencyLog::Log(EPCGExValencyLogCategory::Category, EPCGExValencyLogVerbosity::Verbosity, FString::Printf(TEXT(Format), ##__VA_ARGS__)); \
		} \
	} while (0)

/** Convenience macros for common verbosity levels */
#define PCGEX_VALENCY_ERROR(Category, Format, ...) PCGEX_VALENCY_LOG(Category, Error, Format, ##__VA_ARGS__)
#define PCGEX_VALENCY_WARNING(Category, Format, ...) PCGEX_VALENCY_LOG(Category, Warning, Format, ##__VA_ARGS__)
#define PCGEX_VALENCY_INFO(Category, Format, ...) PCGEX_VALENCY_LOG(Category, Info, Format, ##__VA_ARGS__)
#define PCGEX_VALENCY_VERBOSE(Category, Format, ...) PCGEX_VALENCY_LOG(Category, Verbose, Format, ##__VA_ARGS__)

/** Section markers for report readability */
#define VALENCY_LOG_SECTION(Category, SectionName) \
	do { \
		if (FPCGExValencyLog::WouldLog(EPCGExValencyLogCategory::Category, EPCGExValencyLogVerbosity::Info)) \
		{ \
			FPCGExValencyLog::Log(EPCGExValencyLogCategory::Category, EPCGExValencyLogVerbosity::Info, FString(TEXT("=== ")) + TEXT(SectionName) + TEXT(" ===")); \
		} \
	} while (0)

#define VALENCY_LOG_SUBSECTION(Category, SubsectionName) \
	do { \
		if (FPCGExValencyLog::WouldLog(EPCGExValencyLogCategory::Category, EPCGExValencyLogVerbosity::Info)) \
		{ \
			FPCGExValencyLog::Log(EPCGExValencyLogCategory::Category, EPCGExValencyLogVerbosity::Info, FString(TEXT("--- ")) + TEXT(SubsectionName) + TEXT(" ---")); \
		} \
	} while (0)
