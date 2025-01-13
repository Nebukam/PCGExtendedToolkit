// Copyright 2024 Timothé Lapetite and contributors
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
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPathCrossingsSettings : public UPCGExPathProcessorSettings
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

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** If enabled, crossings are only computed per path, against themselves only. Note: this ignores the "bEnableSelfIntersection" from details below. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	bool bSelfIntersectionOnly = false;

	/** Filter entire dataset. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bSelfIntersectionOnly"))
	FName CanBeCutTag = NAME_None;

	/** Filter entire dataset. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bSelfIntersectionOnly"))
	FName CanCutTag = NAME_None;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExPathEdgeIntersectionDetails IntersectionDetails;

	/** Blending applied on intersecting points along the path prev and next point. This is different from inheriting from external properties. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault))
	TObjectPtr<UPCGExSubPointsBlendOperation> Blending;

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


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIfHasCrossing = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIfHasCrossing"))
	FString HasCrossingsTag = TEXT("HasCrossings");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIfHasNoCrossings = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIfHasNoCrossings"))
	FString HasNoCrossingsTag = TEXT("HasNoCrossings");
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathCrossingsContext final : FPCGExPathProcessorContext
{
	friend class FPCGExPathCrossingsElement;

	FString CanCutTag = TEXT("");
	FString CanBeCutTag = TEXT("");

	TArray<TObjectPtr<const UPCGExFilterFactoryData>> CanCutFilterFactories;
	TArray<TObjectPtr<const UPCGExFilterFactoryData>> CanBeCutFilterFactories;

	UPCGExSubPointsBlendOperation* Blending = nullptr;

	TSharedPtr<PCGExDetails::FDistances> Distances;
	FPCGExBlendingDetails CrossingBlending;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathCrossingsElement final : public FPCGExPathProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExPathCrossings
{
	struct /*PCGEXTENDEDTOOLKIT_API*/ FCrossing
	{
		int32 Index = -1;

		TArray<uint64> Crossings; // Point Index | IO Index
		TArray<FVector> Positions;
		TArray<double> Alphas;
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

		TArray<int8> CanCut;
		TArray<int8> CanBeCut;

		TSet<FName> ProtectedAttributes;
		UPCGExSubPointsBlendOperation* Blending = nullptr;

		TSet<int32> CrossIOIndices;
		TSharedPtr<PCGExData::FUnionMetadata> UnionMetadata;
		TSharedPtr<PCGExDataBlending::FUnionBlender> UnionBlender;

		FPCGExPathEdgeIntersectionDetails Details;

		TSharedPtr<PCGExData::TBuffer<bool>> FlagWriter;
		TSharedPtr<PCGExData::TBuffer<double>> AlphaWriter;
		TSharedPtr<PCGExData::TBuffer<FVector>> CrossWriter;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TPointsProcessor(InPointDataFacade)
		{
		}

		virtual bool IsTrivial() const override { return false; } // Force non-trivial because this shit is expensive

		const PCGExPaths::FPathEdgeOctree* GetEdgeOctree() const { return Path->GetEdgeOctree(); }

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;
		void CollapseCrossing(const int32 Index);
		void CrossBlendPoint(const int32 Index);
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
