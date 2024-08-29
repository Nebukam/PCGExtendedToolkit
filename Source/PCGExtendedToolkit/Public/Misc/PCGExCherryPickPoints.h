// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExCherryPickPoints.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Cherry Pick Source"))
enum class EPCGExCherryPickSource : uint8
{
	Self UMETA(DisplayName = "Self", ToolTip="Read indices from an attribute on the currently cherry-picked data set."),
	Target UMETA(DisplayName = "Targets", ToolTip="Read indices from a list of target points."),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExCherryPickPointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(CherryPickPoints, "Cherry Pick Points", "Filter points by indices, either read from local attributes or using another list of points.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscRemove; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExCherryPickSource IndicesSource = EPCGExCherryPickSource::Target;

	/** Attribute to read value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector ReadIndexFromAttribute;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExIndexSafety Safety = EPCGExIndexSafety::Ignore;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCherryPickPointsContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExCherryPickPointsElement;

	virtual ~FPCGExCherryPickPointsContext() override;

	bool TryGetUniqueIndices(const PCGExData::FPointIO* InSource, TArray<int32>& OutUniqueIndices, int32 MaxIndex = -1) const;
	TArray<int32> SharedTargetIndices;
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
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		TArray<int32> PickedIndices;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints)
			: FPointsProcessor(InPoints)
		{
		}

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void CompleteWork() override;
	};
}
