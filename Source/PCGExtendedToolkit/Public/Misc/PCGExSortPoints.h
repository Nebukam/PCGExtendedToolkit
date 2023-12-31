﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExSortPoints.generated.h"

UENUM(BlueprintType)
enum class EPCGExSortDirection : uint8
{
	Ascending UMETA(DisplayName = "Ascending"),
	Descending UMETA(DisplayName = "Descending")
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSortRule : public FPCGExInputDescriptor
{
	GENERATED_BODY()

	FPCGExSortRule()
	{
	}

	FPCGExSortRule(const FPCGExSortRule& Other)
		: FPCGExInputDescriptor(Other), Tolerance(Other.Tolerance)
	{
	}

public:
	/** Sub-component order, used only for multi-field attributes (FVector, FRotator etc). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayAfter="Selector", ForceInlineRow))
	EPCGExOrderedFieldSelection OrderFieldSelection = EPCGExOrderedFieldSelection::XYZ;

	/** Equality tolerance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float Tolerance = 0.0001f;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExSortPointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SortPoints, "Sort Points", "Sort the source points according to specific rules.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Controls the order in which points will be ordered. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;

	/** Ordered list of attribute to check to sort over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TArray<FPCGExSortRule> Rules = {FPCGExSortRule{}};

private:
	friend class FPCGExSortPointsElement;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSortPointsElement : public FPCGExPointsProcessorElementBase
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
