// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMatchingCommon.h"

struct FPCGExContext;
class FPCGExMatchRuleOperation;
class UPCGData;

namespace PCGExData
{
	class FTags;
	class FPointIO;
	class FFacade;
	struct FConstPoint;
}

struct FPCGExTaggedData;
struct FPCGExMatchingDetails;

namespace PCGExMatching
{
	struct PCGEXMATCHING_API FScope
	{
	private:
		int32 NumCandidates = 0;
		int32 Counter = 0;
		int8 Valid = true;

	public:
		FScope() = default;
		explicit FScope(const int32 InNumCandidates, const bool bUnlimited = false);

		void RegisterMatch();
		FORCEINLINE int32 GetNumCandidates() const { return NumCandidates; }
		FORCEINLINE int32 GetCounter() const { return FPlatformAtomics::AtomicRead(&Counter); }
		FORCEINLINE bool IsValid() const { return static_cast<bool>(FPlatformAtomics::AtomicRead(&Valid)); }

		void Invalidate();
	};

	class PCGEXMATCHING_API FDataMatcher : public TSharedFromThis<FDataMatcher>
	{
	protected:
		const FPCGExMatchingDetails* Details = nullptr;

		int32 NumSources = 0;

		TSharedPtr<TArray<FPCGExTaggedData>> MatchableSources;
		TSharedPtr<TArray<PCGExData::FConstPoint>> MatchableSourceFirstElements;
		TMap<const UPCGData*, int32> MatchableSourcesMap;
		TArray<TSharedPtr<FPCGExMatchRuleOperation>> Operations;

		TArray<TSharedPtr<FPCGExMatchRuleOperation>> RequiredOperations;
		TArray<TSharedPtr<FPCGExMatchRuleOperation>> OptionalOperations;

	public:
		EPCGExMapMatchMode MatchMode = EPCGExMapMatchMode::Disabled;

		FDataMatcher();

		FORCEINLINE int32 GetNumSources() const { return NumSources; }
		bool FindIndex(const UPCGData* InData, int32& OutIndex) const;

		void SetDetails(const FPCGExMatchingDetails* InDetails);

		bool Init(FPCGExContext* InContext, const TArray<const UPCGData*>& InMatchableSources, const TArray<TSharedPtr<PCGExData::FTags>>& InTags, const bool bThrowError);
		bool Init(FPCGExContext* InContext, const TArray<TSharedRef<PCGExData::FFacade>>& InMatchableSources, const bool bThrowError);
		bool Init(FPCGExContext* InContext, const TArray<TSharedPtr<PCGExData::FFacade>>& InMatchableSources, const bool bThrowError);
		bool Init(FPCGExContext* InContext, const TArray<FPCGExTaggedData>& InMatchableSources, const bool bThrowError);
		bool Init(FPCGExContext* InContext, const TSharedPtr<FDataMatcher>& InOtherDataMatcher, const FName InFactoriesLabel, const bool bThrowError);

		bool Test(const UPCGData* InMatchableSource, const FPCGExTaggedData& InDataCandidate, FScope& InMatchingScope) const;
		bool Test(const PCGExData::FConstPoint& InInMatchableElement, const FPCGExTaggedData& InDataCandidate, FScope& InMatchingScope) const;

		bool PopulateIgnoreList(const FPCGExTaggedData& InDataCandidate, FScope& InMatchingScope, TSet<const UPCGData*>& OutIgnoreList) const;

		int32 GetMatchingSourcesIndices(const FPCGExTaggedData& InDataCandidate, FScope& InMatchingScope, TArray<int32>& OutMatches, const TSet<int32>* InExcludedSources = nullptr) const;

		bool HandleUnmatchedOutput(const TSharedPtr<PCGExData::FFacade>& InFacade, const bool bForward = true) const;

	protected:
		int32 GetMatchLimitFor(const FPCGExTaggedData& InDataCandidate) const;
		void RegisterTaggedData(FPCGExContext* InContext, const FPCGExTaggedData& InTaggedData);
		bool InitInternal(FPCGExContext* InContext, const FName InFactoriesLabel);
	};
}
