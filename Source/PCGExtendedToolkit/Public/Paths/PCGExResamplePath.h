// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"
#include "Shapes/PCGExShapes.h"

#include "PCGExResamplePath.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExResamplePathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ResamplePath, "Path : Resample", "Resample path to enforce equally spaced points.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings


	
	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bPreserveLastPoint = true;

	/** Resolution mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Resolution", meta = (PCG_NotOverridable))
	EPCGExResolutionMode ResolutionMode = EPCGExResolutionMode::Distance;

	/** Resolution input type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Resolution", meta = (PCG_NotOverridable))
	EPCGExInputValueType ResolutionInput = EPCGExInputValueType::Constant;

	/** Resolution Constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Resolution", meta=(PCG_Overridable, EditCondition="ResolutionInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0))
	double ResolutionConstant = 10;

	/** Resolution Attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Resolution", meta=(PCG_Overridable, EditCondition="ResolutionInput == EPCGExInputValueType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector ResolutionAttribute;
	
	
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExResamplePathContext final : FPCGExPathProcessorContext
{
	friend class FPCGExResamplePathElement;

};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExResamplePathElement final : public FPCGExPathProcessorElement
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

namespace PCGExResamplePath
{

	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExResamplePathContext, UPCGExResamplePathSettings>
	{
		
		TSharedPtr<PCGExPaths::FPath> Path;
		TSharedPtr<PCGExPaths::FPathEdgeLength> PathLength;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}
