// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Geometry/PCGExGeo.h"
#include "PCGExOffsetPath.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExOffsetPathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(OffsetPath, "Path : Offset", "Offset paths points.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** Consider paths to be closed -- processing will wrap between first and last points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bClosedPath = false;

	/** Offset type.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExFetchType OffsetType = EPCGExFetchType::Constant;

	/** Offset size.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="OffsetType == EPCGExFetchType::Constant"))
	double OffsetConstant = 1.0;

	/** Fetch the offset size from a local attribute. The regular Size parameter then act as a scale.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="OffsetType == EPCGExFetchType::Attribute"))
	FPCGAttributePropertyInputSelector OffsetAttribute;

	/** Up Vector type.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExFetchType UpVectorType = EPCGExFetchType::Constant;

	/** Up vector used to calculate Offset direction.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="UpVectorType == EPCGExFetchType::Constant"))
	FVector UpVectorConstant = FVector::UpVector;

	/** Fetch the Up vector from a local point attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="UpVectorType == EPCGExFetchType::Attribute"))
	FPCGAttributePropertyInputSelector UpVectorAttribute;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExOffsetPathContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExOffsetPathElement;

	virtual ~FPCGExOffsetPathContext() override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExOffsetPathElement final : public FPCGExPathProcessorElement
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
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		int32 NumPoints = 0;

		double OffsetConstant = 0;
		FVector UpConstant = FVector::UpVector;

		TArray<FVector> Positions;
		TArray<FVector> Normals;

		PCGExData::FCache<double>* OffsetGetter = nullptr;
		PCGExData::FCache<FVector>* UpGetter = nullptr;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void CompleteWork() override;

	protected:
		FVector NRM(const int32 A, const int32 B, const int32 C) const;
	};
}
