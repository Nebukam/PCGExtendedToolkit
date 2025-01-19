// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExDataFilter.h"
#include "PCGExMetaFilter.generated.h"

UENUM()
enum class EPCGExMetaFilterMode : uint8
{
	Default    = 0 UMETA(DisplayName = "Default", Tooltip="Default mode will filter both tags and attributes."),
	Duplicates = 1 UMETA(DisplayName = "Duplicates", Tooltip="Filter out duplicates based on tags that pass the filter"),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExMetaFilterSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MetaFilter, "Meta Filter", "Filter point collections based on tags & attributes using string queries");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorFilterHub; }
#endif

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	EPCGExMetaFilterMode Mode = EPCGExMetaFilterMode::Default;

	/** Attribute name filters. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Mode!=EPCGExMetaFilterMode::Duplicates", EditConditionHides, ShowOnlyInnerProperties))
	FPCGExNameFiltersDetails Attributes = FPCGExNameFiltersDetails(false);

	/** Tags filters. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExNameFiltersDetails Tags = FPCGExNameFiltersDetails(false);

	/** If enabled, will test full tag value on value tags ('Tag:Value'), otherwise only test the left part. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bTestTagValues = false;

	/** Swap Inside & Outside data */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bSwap = false;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMetaFilterContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExMetaFilterElement;

	FPCGExCarryOverDetails Filters;

	TSharedPtr<PCGExData::FPointIOCollection> Inside;
	TSharedPtr<PCGExData::FPointIOCollection> Outside;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMetaFilterElement final : public FPCGExPointsProcessorElement
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
