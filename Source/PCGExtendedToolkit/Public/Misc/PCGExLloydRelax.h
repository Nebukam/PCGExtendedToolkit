// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSettings.h"
#include "Geometry/PCGExGeo.h"
#include "PCGExLloydRelax.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExLloydRelaxSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(LloydRelax, "Lloyd Relax 3D", "Applies Lloyd relaxation to the input points.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMisc; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=1))
	int32 Iterations = 5;

	/** Influence Settings*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExInfluenceSettings InfluenceSettings;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExLloydRelaxContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExLloydRelaxElement;

	virtual ~FPCGExLloydRelaxContext() override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExLloydRelaxElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExLloydRelax
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		friend class FLloydRelaxTask;

		double ConstantInfluence = 1;

		PCGEx::FLocalSingleFieldGetter* InfluenceGetter = nullptr;
		TArray<FVector> ActivePositions;
		bool bProgressiveInfluence = false;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point) override;
		virtual void CompleteWork() override;
	};

	class PCGEXTENDEDTOOLKIT_API FLloydRelaxTask final : public PCGExMT::FPCGExTask
	{
	public:
		FLloydRelaxTask(PCGExData::FPointIO* InPointIO,
		                FProcessor* InProcessor,
		                const FPCGExInfluenceSettings* InInfluenceSettings,
		                const int32 InNumIterations) :
			FPCGExTask(InPointIO),
			Processor(InProcessor),
			InfluenceSettings(InInfluenceSettings),
			NumIterations(InNumIterations)
		{
		}

		FProcessor* Processor = nullptr;
		const FPCGExInfluenceSettings* InfluenceSettings = nullptr;
		int32 NumIterations = 0;

		virtual bool ExecuteTask() override;
	};
}
