// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"


#include "Pickers/PCGExPickerFactoryProvider.h"


#include "PCGExCherryPickPoints.generated.h"

UENUM()
enum class EPCGExCherryPickSource : uint8
{
	Self    = 0 UMETA(DisplayName = "Self", ToolTip="Read indices from an attribute on the currently cherry-picked data set."),
	Sources = 1 UMETA(DisplayName = "Sources", ToolTip="Read indices from a list of sources inputs."),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExCherryPickPointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(CherryPickPoints, "Cherry Pick Points", "Filter points by indices, either read from local attributes or using external sources.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscRemove; }
#endif

	/** Whether to invert the picking (picked indices will be discarded instead or kept) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;

	/** Whether to output discard points to their own dataset */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bOutputDiscardedPoints = false;

	/** Whether to output discard points collections to be empty */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAllowEmptyOutputs = false;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCherryPickPointsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExCherryPickPointsElement;
	TArray<TObjectPtr<const UPCGExPickerFactoryData>> PickerFactories;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCherryPickPointsElement final : public FPCGExPointsProcessorElement
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

namespace PCGExCherryPickPoints
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExCherryPickPointsContext, UPCGExCherryPickPointsSettings>
	{
	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TPointsProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
	};
}
