// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGPoint.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExPointSortHelpers.h"
#include "PCGExSortPointsByAttributes.generated.h"

namespace PCGExSortPointsByAttributes
{
	extern const FName SourceLabel;	
}

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExSortPointsByAttributesSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGExSortPointsByAttributesSettings();
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SortPointsByAttributes")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExSortPointsByAttributes", "NodeTitle", "Sort Points by Attributes"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:

	/** Controls the order in which points will be ordered. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	ESortDirection SortDirection = ESortDirection::Ascending;

	/** Ordered list of attribute to check to define sorting order. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TArray<FPCGExSortAttributeDetails> Attributes = {};

protected:
	bool TryGetDetails(const FName Name, FPCGExSortAttributeDetails& OutDetails) const;	

	TArray<FName> UniqueAttributeNames;
	TMap<FName, const FPCGExSortAttributeDetails> UniqueAttributeDetails;

private:
	friend class FPCGExSortPointsByAttributesElement;
	
};

class FPCGExSortPointsByAttributesElement : public FPCGPointProcessingElementBase
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};