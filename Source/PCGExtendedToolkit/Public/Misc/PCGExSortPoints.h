// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExSortPoints.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Sort Direction"))
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

	/** Invert sorting direction on that rule. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bInvertRule = false;

	/** Compare absolute value. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	//bool bAbsolute = false;
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
	bool bInvertRule = false;
	bool bAbsolute = false;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExSortPointsBaseSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

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

	virtual bool GetSortingRules(const FPCGContext* InContext, TArray<FPCGExSortRuleDescriptor>& OutRules) const;

private:
	friend class FPCGExSortPointsBaseElement;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExSortPointsSettings : public UPCGExSortPointsBaseSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SortPointsStatic, "Sort Points (Static)", "Sort the source points according to specific rules.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExEditorSettings>()->NodeColorMiscWrite; }
#endif

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	/** Ordered list of attribute to check to sort over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, TitleProperty="{TitlePropertyName}"))
	TArray<FPCGExSortRuleDescriptor> Rules = {FPCGExSortRuleDescriptor{}};

	virtual bool GetSortingRules(const FPCGContext* InContext, TArray<FPCGExSortRuleDescriptor>& OutRules) const override;

private:
	friend class FPCGExSortPointsBaseElement;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSortPointsBaseElement : public FPCGExPointsProcessorElementBase
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSortPointIO : public FPCGExNonAbandonableTask
{
public:
	FPCGExSortPointIO(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};
