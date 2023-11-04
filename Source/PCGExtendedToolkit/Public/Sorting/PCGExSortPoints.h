// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGExCompare.h"
#include "PCGSettings.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExSortPoints.generated.h"

namespace PCGExSortPoints
{
	extern const FName SourceLabel;
}

UENUM(BlueprintType)
enum class EPCGExSortDirection : uint8
{
	Ascending UMETA(DisplayName = "Ascending"),
	Descending UMETA(DisplayName = "Descending")
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSortRule : public FPCGExSelectorSettingsBase
{
	GENERATED_BODY()

	FPCGExSortRule(): FPCGExSelectorSettingsBase()
	{
	}

	template <typename T>
	FPCGExSortRule(const FPCGExSortRule& Other): FPCGExSelectorSettingsBase(Other)
	{
		Tolerance = Other.Tolerance;
	}

public:
	/** Equality tolerance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float Tolerance = 0.0001f;
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExSortPointsSettings : public UPCGSettings
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
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;

	/** Ordered list of attribute to check to sort over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TArray<FPCGExSortRule> Rules = {};

private:
	friend class FPCGExSortPointsElement;
};

class FPCGExSortPointsElement : public FPCGPointProcessingElementBase
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

	template <typename T>
	static int Compare(const T& A, const T& B, const FPCGExSortRule& Settings)
	{
		return FPCGExCompare::Compare(A, B, Settings.Tolerance, Settings.ComponentSelection);
	}
};
