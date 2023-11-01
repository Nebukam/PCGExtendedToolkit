// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGSettings.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExSortPointsByAttributes.generated.h"

namespace PCGExSortPointsByAttributes
{
	extern const FName SourceLabel;
}

UENUM(BlueprintType)
enum class ESortDirection : uint8
{
	Ascending UMETA(DisplayName = "Ascending"),
	Descending UMETA(DisplayName = "Descending")
};

USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExSortSelector
{
	GENERATED_BODY()

public:
	FPCGAttributePropertyInputSelector Selector;
	float Tolerance = 0.0001f;
	ESortComponent SortComponent = ESortComponent::XYZ;
	FPCGMetadataAttributeBase* Attribute = nullptr;

	bool IsValid()
	{
		EPCGAttributePropertySelection Sel = Selector.GetSelection();
		if (Sel == EPCGAttributePropertySelection::Attribute)
		{
			return !(Attribute == nullptr || !Selector.IsValid());
		}
		else if (Sel == EPCGAttributePropertySelection::PointProperty)
		{			
			return Selector.GetPointProperty() != EPCGPointProperties::Transform;
		}
		
		return false;
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSortSettings
{
	GENERATED_BODY()

public:
	/** Name of the attribute to compare */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FPCGAttributePropertyInputSelector Selector;

	/** Equality tolerance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Tolerance = 0.0001f;

	/** Sub-sorting order, used only for multi-field attributes (FVector, FRotator etc). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	ESortComponent SortComponent = ESortComponent::XYZ;

	FPCGExSortSelector CopyAndFixLast(const UPCGPointData* InData)
	{
		const FPCGAttributePropertyInputSelector FixedSelector = Selector.CopyAndFixLast(InData);
		FPCGMetadataAttributeBase* Attribute = FixedSelector.IsValid() ? InData->Metadata->GetMutableAttribute(FixedSelector.GetName()) : nullptr;
		return FPCGExSortSelector{
			FixedSelector,
			Tolerance,
			SortComponent,
			Attribute
		};
	}
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExSortPointsByAttributesSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
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

	/** Ordered list of attribute to check to sort over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TArray<FPCGExSortSettings> SortOver = {};

private:
	friend class FPCGExSortPointsByAttributesElement;
};

class FPCGExSortPointsByAttributesElement : public FPCGPointProcessingElementBase
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
