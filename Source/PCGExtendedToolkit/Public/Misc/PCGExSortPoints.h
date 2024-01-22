// Copyright Timothé Lapetite 2024
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
struct PCGEXTENDEDTOOLKIT_API FPCGExSortRuleDescriptor : public FPCGExInputDescriptor
{
	GENERATED_BODY()

	FPCGExSortRuleDescriptor()
	{
	}

	FPCGExSortRuleDescriptor(const FPCGExSortRuleDescriptor& Other)
		: FPCGExInputDescriptor(Other),
		  Tolerance(Other.Tolerance)
	{
	}

	/** Equality tolerance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Tolerance = 0.0001f;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSortRule : public PCGEx::FLocalSingleFieldGetter
{
	FPCGExSortRule()
	{
	}

	FPCGExSortRule(const FPCGExSortRule& Other)
		: FLocalSingleFieldGetter(Other)
	{
	}

	double Tolerance = 0.0001f;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExSortPointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, TitleProperty="{TitlePropertyName}"))
	TArray<FPCGExSortRuleDescriptor> Rules = {FPCGExSortRuleDescriptor{}};

private:
	friend class FPCGExSortPointsElement;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSortPointsElement : public FPCGExPointsProcessorElementBase
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
