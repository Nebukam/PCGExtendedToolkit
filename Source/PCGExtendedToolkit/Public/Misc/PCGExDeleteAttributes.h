// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExDeleteAttributes.generated.h"


UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Delete Filter"))
enum class EPCGExDeleteFilter : uint8
{
	Keep UMETA(DisplayName = "Keep", ToolTip="Keep items from the list and remove all others"),
	Remove UMETA(DisplayName = "Remove", ToolTip="Remove items from the list"),
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExDeleteAttributesSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DeleteAttributes, "Delete Attributes", "Delete the specified list of attributes.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscRemove; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** List of attributes to delete. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDeleteFilter Mode = EPCGExDeleteFilter::Remove;

	/** List of attributes to delete. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSet<FName> AttributeNames;

	/** Additional list of attributes to delete, separated by a comma. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FString CommaSeparatedNames;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExDeleteAttributesContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExDeleteAttributesElement;

	TSet<FName> Targets;

	virtual ~FPCGExDeleteAttributesContext() override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExDeleteAttributesElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
