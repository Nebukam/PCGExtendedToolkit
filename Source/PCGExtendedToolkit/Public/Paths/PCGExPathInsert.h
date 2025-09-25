// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"
#include "PCGExPaths.h"

#include "PCGExPointsProcessor.h"
#include "Data/Blending/PCGExUnionBlender.h"
#include "Data/Blending/PCGExDataBlending.h"

#include "PCGExPathInsert.generated.h"

/**
 * 
 */
UCLASS(Hidden, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/insert"))
class UPCGExPathInsertSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathInsert, "Path : Insert", "Insert nearest points into the path using different methods.");
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	// TODO : Insert at closest location on segment
	// TODO : Insert and preserve location
	// TODO : Insert and blend
	// TODO : Insert and don't blend (blend like subdivide)

public:
	/** If enabled, inserted points will be snapped to the path. Otherwise, they retain their original location */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bSnapToPath = false;


	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bWithinRange = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="bWithinRange"))
	EPCGExInputValueType RangeInput = EPCGExInputValueType::Constant;

	/** Max Count Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Range (Attr)", EditCondition="bWithinRange && RangeInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName RangeAttribute = FName("Range");

	/** Max Count Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Range", EditCondition="bWithinRange && RangeInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0))
	double Range = 100;

	PCGEX_SETTING_VALUE_GET(Range, int32, RangeInput, RangeAttribute, Range)


	/** Meta filter settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Carry Over Settings"))
	FPCGExCarryOverDetails CarryOverDetails;
};

struct FPCGExPathInsertContext final : FPCGExPathProcessorContext
{
	friend class FPCGExPathInsertElement;

	FString CanCutTag = TEXT("");
	FString CanBeCutTag = TEXT("");

	TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> CanCutFilterFactories;
	TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> CanBeCutFilterFactories;

	TSharedPtr<PCGExDetails::FDistances> Distances;
	FPCGExBlendingDetails CrossingBlending;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExPathInsertElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(PathInsert)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExPathInsert
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExPathInsertContext, UPCGExPathInsertSettings>
	{
		bool bClosedLoop = false;

		TSharedPtr<PCGExPaths::FPath> Path;
		TSharedPtr<PCGExPaths::FPathEdgeLength> PathLength;
		TSharedPtr<PCGExDetails::TSettingValue<double>> RangeGetter;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual bool IsTrivial() const override { return false; } // Force non-trivial because this shit is expensive

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void CompleteWork() override;
	};
}
