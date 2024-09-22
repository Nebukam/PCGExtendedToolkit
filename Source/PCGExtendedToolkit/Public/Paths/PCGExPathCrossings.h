// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"
#include "PCGExPaths.h"

#include "PCGExPointsProcessor.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Geometry/PCGExGeo.h"
#include "PCGExPathCrossings.generated.h"

namespace PCGExDataBlending
{
	class FCompoundBlender;
}

class UPCGExSubPointsBlendOperation;
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
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** If enabled, crossings are only computed per path, against themselves only. Note: this ignores the "bEnableSelfIntersection" from details below. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	bool bSelfIntersectionOnly = false;

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

	FPCGExDistanceDetails CrossingBlendingDistance;


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


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bTagIfHasCrossing = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, EditCondition="bTagIfHasCrossing"))
	FString HasCrossingsTag = TEXT("HasCrossings");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bTagIfHasNoCrossings = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, EditCondition="bTagIfHasNoCrossings"))
	FString HasNoCrossingsTag = TEXT("HasNoCrossings");
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathCrossingsContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExPathCrossingsElement;

	virtual ~FPCGExPathCrossingsContext() override;

	TArray<UPCGExFilterFactoryBase*> CanCutFilterFactories;
	TArray<UPCGExFilterFactoryBase*> CanBeCutFilterFactories;

	UPCGExSubPointsBlendOperation* Blending = nullptr;

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

		FCrossing(const int32 InIndex):
			Index(InIndex)
		{
		}
	};

	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		const UPCGExPathCrossingsSettings* LocalSettings = nullptr;
		FPCGExPathCrossingsContext* LocalTypedContext = nullptr;

		bool bClosedLoop = false;
		bool bSelfIntersectionOnly = false;

		int32 NumPoints = 0;
		int32 LastIndex = 0;

		TArray<FVector> Positions;
		TArray<double> Lengths;
		TArray<PCGExPaths::FPathEdge*> Edges;
		TArray<FCrossing*> Crossings;

		PCGExPointFilter::TManager* CanCutFilterManager = nullptr;
		PCGExPointFilter::TManager* CanBeCutFilterManager = nullptr;

		TArray<bool> CanCut;
		TArray<bool> CanBeCut;

		TSet<FName> ProtectedAttributes;
		UPCGExSubPointsBlendOperation* Blending = nullptr;

		TSet<int32> CrossIOIndices;
		PCGExData::FIdxCompoundList* CompoundList = nullptr;
		PCGExDataBlending::FCompoundBlender* CompoundBlender = nullptr;

		using TEdgeOctree = TOctree2<PCGExPaths::FPathEdge*, PCGExPaths::FPathEdgeSemantics>;
		TEdgeOctree* EdgeOctree = nullptr;

		FPCGExPathEdgeIntersectionDetails Details;

		PCGEx::TAttributeWriter<bool>* FlagWriter = nullptr;
		PCGEx::TAttributeWriter<double>* AlphaWriter = nullptr;
		PCGEx::TAttributeWriter<FVector>* CrossWriter = nullptr;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints)
			: FPointsProcessor(InPoints)
		{
		}

		virtual bool IsTrivial() const override { return false; } // Force non-trivial because this shit is expensive

		virtual ~FProcessor() override;
		const TEdgeOctree* GetEdgeOctree() const { return EdgeOctree; }

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount) override;
		void FixPoint(const int32 Index);
		void CrossBlendPoint(const int32 Index);
		void OnSearchComplete();
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
