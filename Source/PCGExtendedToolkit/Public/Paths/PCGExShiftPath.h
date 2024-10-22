// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"
#include "PCGExPaths.h"
#include "PCGExPointsProcessor.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Data/Blending/PCGExMetadataBlender.h"

#include "PCGExShiftPath.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Shift Path Mode"))
enum class EPCGExShiftPathMode : uint8
{
	Discrete    = 0 UMETA(DisplayName = "Index", ToolTip="Shift point is selected using a specific index value"),
	Relative = 1 UMETA(DisplayName = "Relative", ToolTip="Shift point is selected using a relative index value"),
	Filter   = 2 UMETA(DisplayName = "Filter", ToolTip="Shift point using the first point that passes the provided filters"),
};

/**
 * 
 */
UCLASS(Abstract, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExShiftPathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExShiftPathSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ShiftPath, "Path : Shift", "Shift path points");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	PCGEX_NODE_POINT_FILTER(Mode == EPCGExShiftPathMode::Filter ? PCGExPaths::SourceShiftFilters : NAME_None, "Filters used to find the shift starting point.", PCGExFactories::PointFilters, Mode == EPCGExShiftPathMode::Filter)
	//~End UPCGExPointsProcessorSettings

	/** NOT IMPLEMENTED YET */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExShiftPathMode Mode = EPCGExShiftPathMode::Relative;
	
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExShiftPathContext final : FPCGExPathProcessorContext
{
	friend class FPCGExShiftPathElement;
	FPCGExBlendingDetails BlendingSettings;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExShiftPathElement final : public FPCGExPathProcessorElement
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

namespace PCGExShiftPath
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExShiftPathContext, UPCGExShiftPathSettings>
	{
		int32 MaxIndex = 0;
		bool bInvertOrientation = false;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void CompleteWork() override;
	};
}
