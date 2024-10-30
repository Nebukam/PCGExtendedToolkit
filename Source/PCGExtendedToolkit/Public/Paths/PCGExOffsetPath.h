// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"

#include "PCGExOffsetPath.generated.h"

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Offset Cleanup Mode")--E*/)
enum class EPCGExOffsetCleanupMode : uint8
{
	Balanced      = 0 UMETA(DisplayName = "Balanced", ToolTip="..."),
	Intersections = 1 UMETA(DisplayName = "Intersections", ToolTip="..."),
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExOffsetPathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathOffset, "Path : Offset", "Offset paths points.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Offset type.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExInputValueType OffsetInput = EPCGExInputValueType::Constant;

	/** Offset size.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="OffsetInput == EPCGExInputValueType::Constant"))
	double OffsetConstant = 1.0;

	/** Fetch the offset size from a local attribute. The regular Size parameter then act as a scale.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="OffsetInput == EPCGExInputValueType::Attribute"))
	FPCGAttributePropertyInputSelector OffsetAttribute;

	/** Up vector used to calculate Offset direction.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="UpVectorType == EPCGExInputValueType::Constant"))
	FVector UpVectorConstant = FVector::UpVector;

	/** Direction Vector type.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExInputValueType DirectionType = EPCGExInputValueType::Constant;

	/** Type of arithmetic path point offset direction.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="DirectionType == EPCGExInputValueType::Constant"))
	EPCGExPathNormalDirection DirectionConstant = EPCGExPathNormalDirection::AverageNormal;

	/** Fetch the direction vector from a local point attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="DirectionType == EPCGExInputValueType::Attribute"))
	FPCGAttributePropertyInputSelector DirectionAttribute;

	/** Removes segments which direction has been flipped due to the offset.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bCleanupPath = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Cleanup", meta = (PCG_NotOverridable, EditCondition="bCleanupPath"))
	EPCGExOffsetCleanupMode CleanupMode = EPCGExOffsetCleanupMode::Balanced;

	/** Even unflipped edges look for upcoming intersections */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Cleanup", meta = (PCG_Overridable, EditCondition="bCleanupPath && CleanupMode==EPCGExOffsetCleanupMode::Balanced", EditConditionHides))
	bool bAdditionalIntersectionCheck = false;

	/** During cleanup, used as a tolerance to consider valid path segments as overlapping or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Cleanup", meta = (PCG_Overridable, EditCondition="bCleanupPath"))
	double IntersectionTolerance = 1;

	/** How many edges forward is acceptable for the cleanup. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Cleanup", meta = (PCG_Overridable, EditCondition="bCleanupPath", ClampMin=1))
	int32 LookupSize = 10;

	/** Attempt to adjust offset on mutated edges .*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Cleanup", meta = (PCG_Overridable, EditCondition="bCleanupPath"))
	bool bFlagMutatedPoints = false;

	/** Name of the 'bool' attribute to flag the nodes that are the result of a mutation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Cleanup", meta=(PCG_Overridable, EditCondition="bCleanupPath && bFlagMutatedPoints"))
	FName MutatedAttributeName = FName("IsMutated");
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExOffsetPathContext final : FPCGExPathProcessorContext
{
	friend class FPCGExOffsetPathElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExOffsetPathElement final : public FPCGExPathProcessorElement
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

namespace PCGExOffsetPath
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExOffsetPathContext, UPCGExOffsetPathSettings>
	{
		TArray<FVector> Positions;

		TSharedPtr<PCGExPaths::FPath> Path;
		TSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>> OffsetDirection;

		TArray<int8> CleanEdge;
		TSharedPtr<PCGExPaths::FPath> DirtyPath;

		double OffsetConstant = 0;
		FVector Up = FVector::UpVector;
		double ToleranceSquared = 1;

		TSharedPtr<PCGExData::TBuffer<double>> OffsetGetter;
		TSharedPtr<PCGExData::TBuffer<FVector>> DirectionGetter;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void OnPointsProcessingComplete() override;
		virtual void CompleteWork() override;

		template <bool bStrictCheck = false>
		bool FindNextIntersection(const PCGExPaths::FPathEdge& FromEdge, int32& NextIteration, FVector& OutIntersection) const
		{
			const FVector E11 = Positions[FromEdge.Start];
			const FVector E12 = Positions[FromEdge.End];

			FVector A = FVector::ZeroVector;
			FVector B = FVector::ZeroVector;

			bool bFound = false;

			DirtyPath->GetEdgeOctree()->FindElementsWithBoundsTest(
				FBoxCenterAndExtent(FromEdge.BSB.Origin, FromEdge.BSB.BoxExtent), [&](const PCGExPaths::FPathEdge* OtherEdge)
				{
					if (OtherEdge->Start <= NextIteration ||
						OtherEdge->Start > (NextIteration + Settings->LookupSize) ||
						OtherEdge->ShareIndices(FromEdge))
					{
						return;
					}

					const PCGExPaths::FPathEdge& E2 = DirtyPath->Edges[OtherEdge->Start];
					const FVector E21 = Positions[E2.Start];
					const FVector E22 = Positions[E2.End];

					FMath::SegmentDistToSegment(E11, E12, E21, E22, A, B);

					if constexpr (bStrictCheck)
					{
						if (A == E11 || A == E12) { return; } // Closest point on current edge is an endpoint
						if (B == E21 || B == E22) { return; } // Closest point on other edge is an endpoint	
					}
					else
					{
						if (A == E11) { return; } // Closest point on current edge is the start
					}

					if (FVector::DistSquared(A, B) > ToleranceSquared) { return; }

					OutIntersection = B;
					bFound = true;
					NextIteration = OtherEdge->Start;
				});

			return bFound;
		}
	};
}
