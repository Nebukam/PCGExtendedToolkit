// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMatching.h"

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
	class FMatchingScope
	{
		int32 NumCandidates = 0;
		int32 Counter = 0;
		int8 Valid = true;

	public:
		FMatchingScope() = default;
		explicit FMatchingScope(const int32 InNumCandidates, const bool bUnlimited = false);

		void RegisterMatch();
		FORCEINLINE int32 GetNumCandidates() const { return NumCandidates; }
		FORCEINLINE int32 GetCounter() const { return FPlatformAtomics::AtomicRead(&Counter); }
		FORCEINLINE bool IsValid() const { return static_cast<bool>(FPlatformAtomics::AtomicRead(&Valid)); }

		void Invalidate();
	};

	class FDataMatcher : public TSharedFromThis<FDataMatcher>
	{
	protected:
		const FPCGExMatchingDetails* Details = nullptr;

		TSharedPtr<TArray<FPCGExTaggedData>> Targets;
		TSharedPtr<TArray<PCGExData::FConstPoint>> Elements;
		TMap<const UPCGData*, int32> TargetsMap;
		TArray<TSharedPtr<FPCGExMatchRuleOperation>> Operations;

		TArray<TSharedPtr<FPCGExMatchRuleOperation>> RequiredOperations;
		TArray<TSharedPtr<FPCGExMatchRuleOperation>> OptionalOperations;

	public:
		EPCGExMapMatchMode MatchMode = EPCGExMapMatchMode::Disabled;

		FDataMatcher();

		bool FindIndex(const UPCGData* InData, int32& OutIndex) const;

		void SetDetails(const FPCGExMatchingDetails* InDetails);

		bool Init(FPCGExContext* InContext, const TArray<const UPCGData*>& InTargetData, const TArray<TSharedPtr<PCGExData::FTags>>& InTags, const bool bThrowError);
		bool Init(FPCGExContext* InContext, const TArray<TSharedRef<PCGExData::FFacade>>& InTargetFacades, const bool bThrowError);
		bool Init(FPCGExContext* InContext, const TArray<TSharedPtr<PCGExData::FFacade>>& InTargetFacades, const bool bThrowError);
		bool Init(FPCGExContext* InContext, const TArray<FPCGExTaggedData>& InTargetDatas, const bool bThrowError);
		bool Init(FPCGExContext* InContext, const TSharedPtr<FDataMatcher>& InOtherMatcher, const FName InFactoriesLabel, const bool bThrowError);

		bool Test(const UPCGData* InTarget, const TSharedPtr<PCGExData::FPointIO>& InDataCandidate, FMatchingScope& InMatchingScope) const;
		bool Test(const PCGExData::FConstPoint& InTargetElement, const TSharedPtr<PCGExData::FPointIO>& InDataCandidate, FMatchingScope& InMatchingScope) const;

		bool PopulateIgnoreList(const TSharedPtr<PCGExData::FPointIO>& InDataCandidate, FMatchingScope& InMatchingScope, TSet<const UPCGData*>& OutIgnoreList) const;
		int32 GetMatchingTargets(const TSharedPtr<PCGExData::FPointIO>& InDataCandidate, FMatchingScope& InMatchingScope, TArray<int32>& OutMatches) const;

		bool HandleUnmatchedOutput(const TSharedPtr<PCGExData::FFacade>& InFacade, const bool bForward = true) const;

	protected:
		int32 GetMatchLimitFor(const TSharedPtr<PCGExData::FPointIO>& InDataCandidate) const;
		void RegisterTaggedData(FPCGExContext* InContext, const FPCGExTaggedData& InTaggedData);
		bool InitInternal(FPCGExContext* InContext, const FName InFactoriesLabel);
	};
}
