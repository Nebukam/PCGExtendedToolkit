// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExGlobalSettings.h"

#include "PCGExShapeProcessor.h"


#include "PCGExCreateShapes.generated.h"

class FPCGExComputeIOBounds;


UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExCreateShapesSettings : public UPCGExShapeProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(CreateShapes, "Create Shapes", "Use shape builders to create shapes from input seed points.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorTransform; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	//~End UPCGExPointsProcessorSettings

private:
	friend class FPCGExCreateShapesElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCreateShapesContext final : FPCGExShapeProcessorContext
{
	friend class FPCGExCreateShapesElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCreateShapesElement final : public FPCGExShapeProcessorElement
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExCreateShapes
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExCreateShapesContext, UPCGExCreateShapesSettings>
	{
		TArray<UPCGExShapeBuilderOperation*> Builders;
		TArray<TSharedPtr<PCGExData::FFacade>> PerSeedFacades;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void OnPointsProcessingComplete() override;
		virtual void Output() override;
		virtual void CompleteWork() override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FBuildShape final : public PCGExMT::FPCGExTask
	{
	public:
		FBuildShape(
			const TSharedPtr<PCGExData::FPointIO>& InPointIO,
			bool InSoloShape,
			UPCGExShapeBuilderOperation* InOperation,
			const TSharedRef<PCGExData::FFacade>& InShapeDataFacade,
			const TSharedPtr<PCGExShapes::FShape>& InShape) :
			FPCGExTask(InPointIO),
			bSoloShape(InSoloShape),
			ShapeDataFacade(InShapeDataFacade),
			Operation(InOperation),
			Shape(InShape)
		{
		}

		bool bSoloShape = false;
		TSharedRef<PCGExData::FFacade> ShapeDataFacade;
		UPCGExShapeBuilderOperation* Operation;
		TSharedPtr<PCGExShapes::FShape> Shape;

		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};
}
