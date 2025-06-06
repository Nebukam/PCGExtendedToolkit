// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"
#include "PCGExPaths.h"

#include "PCGExPointsProcessor.h"
#include "Data/Blending/PCGExUnionBlender.h"
#include "Data/Blending/PCGExDataBlending.h"


#include "SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"
#include "PCGExPathCrossings.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/crossings"))
class UPCGExPathCrossingsSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathCrossings, "Path × Path Crossings", "Find crossing points between & inside paths.");
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** If enabled, crossings are only computed per path, against themselves only. Note: this ignores the "bEnableSelfIntersection" from details below. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	bool bSelfIntersectionOnly = false;

	/** Filter entire dataset. If any tag is found on these paths, they are considered cut-able. Empty or None will ignore filtering. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bSelfIntersectionOnly"))
	FName CanBeCutTag = NAME_None;

	/** If enabled, the absence of the specified tag considers paths as cut-able. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName=" └─ Invert", PCG_NotOverridable, EditCondition="!bSelfIntersectionOnly", EditConditionHides))
	bool bInvertCanBeCutTag = false;

	/** Filter entire dataset. If any tag is found on these paths, they are considered cutters. Empty or None will ignore filtering. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bSelfIntersectionOnly"))
	FName CanCutTag = NAME_None;

	/** If enabled, the absence of the specified tag considers paths as cutters. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName=" └─ Invert", PCG_NotOverridable, EditCondition="!bSelfIntersectionOnly", EditConditionHides))
	bool bInvertCanCutTag = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExPathEdgeIntersectionDetails IntersectionDetails;

	/** Blending applied on intersecting points along the path prev and next point. This is different from inheriting from external properties. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault))
	TObjectPtr<UPCGExSubPointsBlendInstancedFactory> Blending;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Cross Blending", meta=(PCG_Overridable))
	bool bDoCrossBlending = false;

	/** If enabled, blend in properties & attributes from external sources. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Cross Blending", meta=(PCG_Overridable, EditCondition="bDoCrossBlending"))
	FPCGExCarryOverDetails CrossingCarryOver;

	/** If enabled, blend in properties & attributes from external sources. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Cross Blending", meta=(PCG_Overridable, EditCondition="bDoCrossBlending"))
	FPCGExBlendingDetails CrossingBlending = FPCGExBlendingDetails(EPCGExDataBlendingType::Average, EPCGExDataBlendingType::None);
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteAlpha = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName="Crossing Alpha", EditCondition="bWriteAlpha"))
	FName CrossingAlphaAttributeName = "Alpha";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Default Value", EditCondition="bWriteAlpha", EditConditionHides, HideEditConditionToggle))
	double DefaultAlpha = -1;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bOrientCrossing = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, EditCondition="bOrientCrossing"))
	EPCGExAxis CrossingOrientAxis = EPCGExAxis::Forward;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteCrossDirection = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName="Cross Direction", EditCondition="bWriteCrossDirection"))
	FName CrossDirectionAttributeName = "Cross";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Default Value", EditCondition="bWriteCrossDirection", EditConditionHides, HideEditConditionToggle))
	FVector DefaultCrossDirection = FVector::ZeroVector;

	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteIsPointCrossing = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName="Is Point Crossing", EditCondition="bWriteIsPointCrossing"))
	FName IsPointCrossingAttributeName = "IsPointCrossing";
	

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIfHasCrossing = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIfHasCrossing"))
	FString HasCrossingsTag = TEXT("HasCrossings");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIfHasNoCrossings = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIfHasNoCrossings"))
	FString HasNoCrossingsTag = TEXT("HasNoCrossings");
};

struct FPCGExPathCrossingsContext final : FPCGExPathProcessorContext
{
	friend class FPCGExPathCrossingsElement;

	FString CanCutTag = TEXT("");
	FString CanBeCutTag = TEXT("");

	TArray<TObjectPtr<const UPCGExFilterFactoryData>> CanCutFilterFactories;
	TArray<TObjectPtr<const UPCGExFilterFactoryData>> CanBeCutFilterFactories;

	UPCGExSubPointsBlendInstancedFactory* Blending = nullptr;

	TSharedPtr<PCGExDetails::FDistances> Distances;
	FPCGExBlendingDetails CrossingBlending;
};

class FPCGExPathCrossingsElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(PathCrossings)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExPathCrossings
{
	struct PCGEXTENDEDTOOLKIT_API FCrossing
	{
		int32 Index = -1;

		TArray<uint64> Crossings; // Point Index | IO Index
		TArray<FVector> Positions;
		TArray<double> Alphas;
		TArray<int8> IsPoint;
		TArray<FVector> CrossingDirections;

		explicit FCrossing(const int32 InIndex):
			Index(InIndex)
		{
		}
	};

	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExPathCrossingsContext, UPCGExPathCrossingsSettings>
	{
		bool bClosedLoop = false;
		bool bSelfIntersectionOnly = false;
		bool bCanCut = true;
		bool bCanBeCut = true;

		TSharedPtr<PCGExPaths::FPath> Path;
		TSharedPtr<PCGExPaths::FPathEdgeLength> PathLength;

		TArray<TSharedPtr<FCrossing>> Crossings;

		TSharedPtr<PCGExPointFilter::FManager> CanCutFilterManager;
		TSharedPtr<PCGExPointFilter::FManager> CanBeCutFilterManager;

		TBitArray<> CanCut;
		TBitArray<> CanBeCut;

		TSet<FName> ProtectedAttributes;
		TSharedPtr<FPCGExSubPointsBlendOperation> SubBlending;

		TSet<int32> CrossIOIndices;
		TSharedPtr<PCGExDataBlending::FUnionBlender> UnionBlender;

		FPCGExPathEdgeIntersectionDetails Details;

		TSharedPtr<PCGExData::TBuffer<bool>> FlagWriter;
		TSharedPtr<PCGExData::TBuffer<double>> AlphaWriter;
		TSharedPtr<PCGExData::TBuffer<FVector>> CrossWriter;
		TSharedPtr<PCGExData::TBuffer<bool>> IsPointCrossingWriter;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TPointsProcessor(InPointDataFacade)
		{
		}

		virtual bool IsTrivial() const override { return false; } // Force non-trivial because this shit is expensive

		const PCGExPaths::FPathEdgeOctree* GetEdgeOctree() const { return Path->GetEdgeOctree(); }

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void CompleteWork() override;
		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;

		void CollapseCrossings(const PCGExMT::FScope& Scope);
		void CrossBlend(const PCGExMT::FScope& Scope);

		virtual void Write() override;
	};
}
