﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExGlobalSettings.h"
#include "PCGExShapeProcessor.h"

#include "PCGExCreateShapes.generated.h"

class FPCGExComputeIOBounds;


UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class UPCGExCreateShapesSettings : public UPCGExShapeProcessorSettings
{
	GENERATED_BODY()

	friend class FPCGExCreateShapesElement;

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(CreateShapes, "Create Shapes", "Use shape builders to create shapes from input seed points.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorTransform; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	//~End UPCGExPointsProcessorSettings

	/** Should point have a ShapeID attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bWriteShapeId = false;

	/** Name of the 'int32' attribute to write the ShapeId to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName ShapeIdAttributeName = FName("ShapeId");

	/** Force writing to points, otherwise defaults to @Data (even if unspecified) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName=" └─ Write to points", PCG_NotOverridable, EditCondition="OutputMode == EPCGExShapeOutputMode::PerShape"))
	bool bForceOutputToElement = false;

	/** Don't output shape if they have less points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Pruning", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveBelow = true;

	/** Discarded if point count is less than */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Pruning", meta=(PCG_Overridable, EditCondition="bRemoveBelow", ClampMin=0))
	int32 MinPointCount = 2;

	/** Don't output shape if they have more points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Pruning", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveAbove = false;

	/** Discarded if point count is more than */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Pruning", meta=(PCG_Overridable, EditCondition="bRemoveAbove", ClampMin=0))
	int32 MaxPointCount = 500;
};

struct FPCGExCreateShapesContext final : FPCGExShapeProcessorContext
{
	friend class FPCGExCreateShapesElement;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExCreateShapesElement final : public FPCGExShapeProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(CreateShapes)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExCreateShapes
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExCreateShapesContext, UPCGExCreateShapesSettings>
	{
		TArray<TSharedPtr<FPCGExShapeBuilderOperation>> Builders;
		TArray<TSharedPtr<PCGExData::FFacade>> PerSeedFacades;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;

		virtual void OnPointsProcessingComplete() override;

		virtual void Output() override;
		virtual void CompleteWork() override;

	protected:
		bool IsShapeValid(const TSharedPtr<PCGExShapes::FShape>& Shape) const;
		void OutputPerDataSet();
		void OutputPerSeed();
		void OutputPerShape();
	};

	static void BuildShape(
		FPCGExCreateShapesContext* Context,
		const TSharedRef<PCGExData::FFacade>& ShapeDataFacade,
		const TSharedPtr<FPCGExShapeBuilderOperation>& Operation,
		const TSharedPtr<PCGExShapes::FShape>& Shape);

	class FBuildShape final : public PCGExMT::FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FBuildShape)

		FBuildShape(const TSharedPtr<FPCGExShapeBuilderOperation>& InOperation,
		            const TSharedRef<PCGExData::FFacade>& InShapeDataFacade,
		            const TSharedPtr<PCGExShapes::FShape>& InShape) :
			FTask(),
			ShapeDataFacade(InShapeDataFacade),
			Operation(InOperation),
			Shape(InShape)
		{
		}

		TSharedRef<PCGExData::FFacade> ShapeDataFacade;
		TSharedPtr<FPCGExShapeBuilderOperation> Operation;
		TSharedPtr<PCGExShapes::FShape> Shape;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};
}
