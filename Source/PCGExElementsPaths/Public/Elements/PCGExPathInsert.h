// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExH.h"

#include "Core/PCGExPathProcessor.h"
#include "Data/Utils/PCGExDataFilterDetails.h"
#include "Details/PCGExInputShorthandsDetails.h"
#include "Details/PCGExMatchingDetails.h"
#include "PCGExPathInsert.generated.h"

class UPCGExSubPointsBlendInstancedFactory;
class FPCGExSubPointsBlendOperation;

namespace PCGExMatching
{
	class FTargetsHandler;
}

namespace PCGExPaths
{
	class FPathEdgeLength;
	class FPath;
}

namespace PCGExDetails
{
	template <typename T>
	class TSettingValue;
}


/**
 *
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/insert"))
class UPCGExPathInsertSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathInsert, "Path : Insert", "Insert target points into paths at their nearest location.");
#endif

#if WITH_EDITORONLY_DATA
	virtual void PostInitProperties() override;
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** If enabled, inserted points will be snapped to the path. Otherwise, they retain their original position. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bSnapToPath = false;

	/** Only insert points that are within a specified range of the path. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bWithinRange = false;

	/** Maximum distance from path for a point to be inserted. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWithinRange", EditConditionHides))
	FPCGExInputShorthandNameDoubleAbs Range = FPCGExInputShorthandNameDoubleAbs(FName("Range"), 100, false);

	/** Blending applied on inserted points using path's prev and next point. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault))
	TObjectPtr<UPCGExSubPointsBlendInstancedFactory> Blending;

	/** Carry over settings for attributes from target sources. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Carry Over Settings"))
	FPCGExCarryOverDetails CarryOverDetails;

	//

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIfHasInserts = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIfHasInserts"))
	FString HasInsertsTag = TEXT("HasInserts");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIfNoInserts = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIfNoInserts"))
	FString NoInsertsTag = TEXT("NoInserts");
};

struct FPCGExPathInsertContext final : FPCGExPathProcessorContext
{
	friend class FPCGExPathInsertElement;

	TSharedPtr<PCGExMatching::FTargetsHandler> TargetsHandler;
	int32 NumMaxTargets = 0;

	UPCGExSubPointsBlendInstancedFactory* Blending = nullptr;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExPathInsertElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(PathInsert)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExPathInsert
{
	struct FInsertCandidate
	{
		int32 TargetIOIndex = -1;
		int32 TargetPointIndex = -1;
		int32 EdgeIndex = -1;
		double Alpha = 0;
		double Distance = 0;
		FVector PathLocation = FVector::ZeroVector;
		FVector OriginalLocation = FVector::ZeroVector;

		FORCEINLINE uint64 GetTargetHash() const { return PCGEx::H64(TargetPointIndex, TargetIOIndex); }

		bool operator<(const FInsertCandidate& Other) const { return Alpha < Other.Alpha; }
	};

	struct FEdgeInserts
	{
		TArray<FInsertCandidate> Inserts;

		void Add(const FInsertCandidate& Candidate) { Inserts.Add(Candidate); }
		void SortByAlpha() { Inserts.Sort(); }
		int32 Num() const { return Inserts.Num(); }
		bool IsEmpty() const { return Inserts.IsEmpty(); }
	};

	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExPathInsertContext, UPCGExPathInsertSettings>
	{
		bool bClosedLoop = false;
		int32 LastIndex = 0;

		TSharedPtr<PCGExPaths::FPath> Path;
		TSharedPtr<PCGExPaths::FPathEdgeLength> PathLength;

		TSharedPtr<PCGExDetails::TSettingValue<double>> RangeGetter;

		// Stage 1: Candidates per edge
		TArray<FEdgeInserts> EdgeInserts;

		// Stage 3: Output indices
		TArray<int32> StartIndices;
		int32 TotalInserts = 0;

		// Blending
		TSet<FName> ProtectedAttributes;
		TSharedPtr<FPCGExSubPointsBlendOperation> SubBlending;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual bool IsTrivial() const override { return false; }

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		virtual void CompleteWork() override;
		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
	};
}
