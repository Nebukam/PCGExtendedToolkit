// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExTransform.h"
#include "Data/PCGExDataForward.h"

#include "PCGExBoundsToPoints.generated.h"

class FPCGExComputeIOBounds;

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBoundsToPointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BoundsToPoints, "Bounds To Points", "Generate a point on the surface of the bounds.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorTransform; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Generates a point collections per generated point */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bGeneratePerPointData = false;

	/** Generate points in symmetry */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExMinimalAxis SymmetryAxis = EPCGExMinimalAxis::None;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExUVW UVW;

	/** Generates a point collections per generated point */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bSetExtents = true;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bSetExtents"))
	FVector Extents = FVector(0.5);

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bSetScale = true;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bSetScale"))
	FVector Scale = FVector::OneVector;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bGeneratePerPointData"))
	FPCGExAttributeToTagDetails PointAttributesToOutputTags;

private:
	friend class FPCGExBoundsToPointsElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBoundsToPointsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExBoundsToPointsElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBoundsToPointsElement final : public FPCGExPointsProcessorElement
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExBoundsToPoints
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExBoundsToPointsContext, UPCGExBoundsToPointsSettings>
	{
		int32 NumPoints = 0;
		bool bGeneratePerPointData = false;
		bool bSymmetry = false;

		bool bSetExtents = false;
		FVector Extents = FVector::OneVector;

		bool bSetScale = false;
		FVector Scale = FVector::OneVector;

		EPCGExMinimalAxis Axis = EPCGExMinimalAxis::None;

		FPCGExUVW UVW;
		FPCGExAttributeToTagDetails PointAttributesToOutputTags;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		TArray<TSharedPtr<PCGExData::FPointIO>> NewOutputs;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
