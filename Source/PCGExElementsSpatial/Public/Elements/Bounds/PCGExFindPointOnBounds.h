// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExPointsProcessor.h"
#include "Data/Utils/PCGExDataFilterDetails.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExBoundsCommon.h"
#include "Math/PCGExMathAxis.h"

#include "PCGExFindPointOnBounds.generated.h"

class FPCGExComputeIOBounds;


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="misc/find-point-on-bounds"))
class UPCGExFindPointOnBoundsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindPointOnBounds, "Find point on Bounds", "Find the closest point on the dataset bounds.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_OPTIN_NAME(MiscAdd); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Data output mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointOnBoundsOutputMode OutputMode = EPCGExPointOnBoundsOutputMode::Merged;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bBestFitBounds = false;

	/** Whether to use best fit plane bounds, and which axis ordering should be used. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName="Use Best Fit bounds axis", EditCondition="bBestFitBounds"))
	EPCGExAxisOrder AxisOrder = EPCGExAxisOrder::YXZ;

	/** Type of UVW value source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType UVWInput = EPCGExInputValueType::Constant;

	/** Fetch the UVW value from a @Data attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="UVW (Attr)", EditCondition="UVWInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector LocalUVW;

	/** UVW position of the target within bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="UVW", EditCondition="UVWInput == EPCGExInputValueType::Constant", EditConditionHides))
	FVector UVW = FVector::OneVector;

	PCGEX_SETTING_DATA_VALUE_DECL(UVW, FVector)

	/** Offset to apply to the closest point, away from the bounds center. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double Offset = 1;

	/** Meta filter settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Carry Over Settings"))
	FPCGExCarryOverDetails CarryOverDetails;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bQuietAttributeMismatchWarning = false;

private:
	friend class FPCGExFindPointOnBoundsElement;
};

struct FPCGExFindPointOnBoundsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExFindPointOnBoundsElement;

	FPCGExCarryOverDetails CarryOverDetails;

	TArray<int32> BestIndices;
	TSharedPtr<PCGExData::FPointIO> MergedOut;
	TSharedPtr<PCGExData::FAttributesInfos> MergedAttributesInfos;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExFindPointOnBoundsElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(FindPointOnBounds)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExFindPointOnBounds
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExFindPointOnBoundsContext, UPCGExFindPointOnBoundsSettings>
	{
		mutable FRWLock BestIndexLock;

		double BestDistance = MAX_dbl;
		FVector SearchPosition = FVector::ZeroVector;
		FVector BestPosition = FVector::ZeroVector;
		int32 BestIndex = -1;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
