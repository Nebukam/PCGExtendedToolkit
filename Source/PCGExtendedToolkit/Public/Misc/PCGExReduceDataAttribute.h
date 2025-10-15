// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Details/PCGExDetailsAttributes.h"

#include "PCGExReduceDataAttribute.generated.h"

class FPCGMetadataAttributeBase;

UENUM()
enum class EPCGExReduceDataDomainMethod : uint8
{
	Min     = 0 UMETA(DisplayName = "Min", ToolTip=""),
	Max     = 1 UMETA(DisplayName = "Max", ToolTip=""),
	Sum     = 2 UMETA(DisplayName = "Sum", ToolTip=""),
	Average = 3 UMETA(DisplayName = "Average", ToolTip=""),
	Join    = 4 UMETA(DisplayName = "Join", ToolTip=""),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="metadata/reduce-data"))
class UPCGExReduceDataAttributeSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		ReduceDataAttribute, "Reduce Data", "Reduce @Data domain attribute.",
		FName(GetDisplayName()));
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->ColorMiscWrite); }
	virtual TArray<FPCGPreConfiguredSettingsInfo> GetPreconfiguredInfo() const override;
#endif

	virtual void ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo) override;

	virtual FName GetMainInputPin() const override { return PCGPinConstants::DefaultInputLabel; }
	virtual FName GetMainOutputPin() const override { return PCGPinConstants::DefaultOutputLabel; }

protected:
	virtual bool IsInputless() const override { return true; }
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExAttributeSourceToTargetDetails Attributes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExReduceDataDomainMethod Method = EPCGExReduceDataDomainMethod::Min;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bCustomOutputType;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bCustomOutputType"))
	EPCGMetadataTypes OutputType = EPCGMetadataTypes::Integer32;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "Method == EPCGExReduceDataDomainMethod::Join", EditConditionHides))
	FString JoinDelimiter = FString(", ");

#if WITH_EDITOR
	virtual FString GetDisplayName() const;
#endif
};

struct FPCGExReduceDataAttributeContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExReduceDataAttributeElement;
	FPCGAttributeIdentifier WriteIdentifier;
	TArray<const FPCGMetadataAttributeBase*> Attributes;
	EPCGMetadataTypes OutputType = EPCGMetadataTypes::Unknown;
};

class FPCGExReduceDataAttributeElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ReduceDataAttribute)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
