// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/Blending/PCGExMetadataBlender.h"


#include "PCGExFindPointOnBounds.generated.h"

class FPCGExComputeIOBounds;

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Point On Bounds Output Mode"))
enum class EPCGExPointOnBoundsOutputMode : uint8
{
	Merged     = 0 UMETA(DisplayName = "Merged Points", Tooltip="..."),
	Individual = 1 UMETA(DisplayName = "Per-point dataset", Tooltip="..."),
};

UCLASS(Abstract, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFindPointOnBoundsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindPointOnBounds, "Find point on Bounds", "Find the closest point on the dataset bounds.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscAdd; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Data output mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointOnBoundsOutputMode OutputMode = EPCGExPointOnBoundsOutputMode::Merged;

	/** U Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|UVW", meta=(PCG_Overridable, DisplayName="U"))
	double UConstant = 1;

	/** V Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|UVW", meta=(PCG_Overridable, DisplayName="V"))
	double VConstant = 1;

	/** W Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|UVW", meta=(PCG_Overridable, DisplayName="W"))
	double WConstant = 0;

private:
	friend class FPCGExFindPointOnBoundsElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFindPointOnBoundsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExFindPointOnBoundsElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFindPointOnBoundsElement final : public FPCGExPointsProcessorElement
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExFindPointOnBounds
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExFindPointOnBoundsContext, UPCGExFindPointOnBoundsSettings>
	{
		mutable FRWLock BestIndexLock;

		FVector SearchPosition = FVector::ZeroVector;
		int32 BestIndex = -1;
		double BestDistance = DBL_MAX;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void CompleteWork() override;
	};
}
