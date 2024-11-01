// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Shapes/PCGExShapes.h"

#include "PCGExResamplePath.generated.h"

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Path Normal Direction")--E*/)
enum class EPCGExResampleMode : uint8
{
	Sweep        = 0 UMETA(DisplayName = "Sweep", ToolTip="..."),
	Redistribute = 1 UMETA(DisplayName = "Redistribute", ToolTip="..."),
};

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
	EPCGExResampleMode Mode = EPCGExResampleMode::Sweep;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode==EPCGExResampleMode::Sweep"))
	bool bPreserveLastPoint = true;

	/** Resolution mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="Mode==EPCGExResampleMode::Sweep"))
	EPCGExResolutionMode ResolutionMode = EPCGExResolutionMode::Distance;

	/** Resolution Constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode==EPCGExResampleMode::Sweep", EditConditionHides, ClampMin=0))
	double Resolution = 10;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Mode==EPCGExResampleMode::Sweep", EditConditionHides))
	EPCGExTruncateMode Truncate = EPCGExTruncateMode::Round;

	/** Blending settings used to smooth attributes.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExBlendingDetails BlendingSettings = FPCGExBlendingDetails(EPCGExDataBlendingType::Weight, EPCGExDataBlendingType::None);
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
	struct FPointSample
	{
		int32 Start = 0;
		int32 End = 1;
		FVector Location = FVector::ZeroVector;
		double Distance = 0;
	};

	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExResamplePathContext, UPCGExResamplePathSettings>
	{
		int32 NumSamples = 0;
		double SampleLength = 0;
		TArray<FPointSample> Samples;

		TSharedPtr<PCGExDataBlending::FMetadataBlender> MetadataBlender;

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
