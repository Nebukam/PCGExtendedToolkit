// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGSettings.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExSortPoints.generated.h"

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

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSortSettings
{
	GENERATED_BODY()

public:
	/** Name of the attribute to compare */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector Selector;

	/** Equality tolerance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float Tolerance = 0.0001f;

	/** Sub-sorting order, used only for multi-field attributes (FVector, FRotator etc). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	ESortComponent SortComponent = ESortComponent::XYZ;
};

USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExSortSelector : public FPCGExSortSettings
{
	GENERATED_BODY()

public:
	FPCGMetadataAttributeBase* Attribute = nullptr;

	bool IsValid(const UPCGPointData* PointData) const
	{
		const EPCGAttributePropertySelection Sel = Selector.GetSelection();
		if (Sel == EPCGAttributePropertySelection::Attribute && (Attribute == nullptr || !PointData->Metadata->HasAttribute(Selector.GetName()))) { return false; }
		return Selector.IsValid();
	}
};


UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExSortPointsByAttributesSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PCGExSortPoints")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExSortPoints", "NodeTitle", "Sort Points"); }
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
	static FPCGExSortSelector MakeSelectorFromSettings(const FPCGExSortSettings& Settings, const UPCGPointData* InData)
	{
		const FPCGAttributePropertyInputSelector FixedSelector = Settings.Selector.CopyAndFixLast(InData);
		FPCGMetadataAttributeBase* Attribute = FixedSelector.IsValid() ? InData->Metadata->GetMutableAttribute(FixedSelector.GetName()) : nullptr;
		return FPCGExSortSelector{
			FixedSelector,
			Settings.Tolerance,
			Settings.SortComponent,
			Attribute
		};
	}

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
