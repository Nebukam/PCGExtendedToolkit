// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/Bitmasks/PCGExBitmaskDetails.h"
#include "PCGExFilterDetails.generated.h"

class UPCGData;
struct FPCGExContext;

namespace PCGExMT
{
	struct FScope;
}

namespace PCGExData
{
	class FTags;
	class FFacade;
	class FPointIO;

	template <typename T>
	class TBuffer;
}

UENUM()
enum class EPCGExFilterDataAction : uint8
{
	Keep = 0 UMETA(DisplayName = "Keep", ToolTip="Keeps only selected data"),
	Omit = 1 UMETA(DisplayName = "Omit", ToolTip="Omit selected data from output"),
	Tag  = 2 UMETA(DisplayName = "Tag", ToolTip="Keep all and Tag"),
};

UENUM()
enum class EPCGExTagsToDataAction : uint8
{
	Ignore     = 0 UMETA(DisplayName = "Do Nothing", Tooltip="Constant."),
	ToData     = 1 UMETA(DisplayName = "To @Data", Tooltip="Copy tag:value to @Data domain attributes."),
	ToElements = 2 UMETA(DisplayName = "Attribute", Tooltip="Copy tag:value to element domain attributes."),
};

UENUM()
enum class EPCGExResultWriteAction : uint8
{
	Bool    = 0 UMETA(DisplayName = "Boolean", Tooltip="Set a boolean attribute on the points. True when filters pass, False if they don't."),
	Counter = 1 UMETA(DisplayName = "Counter", Tooltip="Mutates a int32 counter with the specified increment/decrement associated with pass/fail. (i.e +1 on pass, -2 on fail)"),
	Bitmask = 2 UMETA(DisplayName = "Bitmask", Tooltip="Mutates a bitmask flag with the operations associated with pass/fail."),
};

USTRUCT(BlueprintType)
struct PCGEXFILTERS_API FPCGExFilterResultDetails
{
	GENERATED_BODY()

	FPCGExFilterResultDetails() = default;
	explicit FPCGExFilterResultDetails(bool bTogglable, bool InEnabled = true);
	virtual ~FPCGExFilterResultDetails() = default;

	UPROPERTY()
	bool bOptional = false;

	/** */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOptional", EditConditionHides, HideEditConditionToggle))
	bool bEnabled = true;

	/** How should the result be used. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExResultWriteAction Action = EPCGExResultWriteAction::Bool;

	/** Name of the attribute to write the result to. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName ResultAttributeName = FName("Result");

	UPROPERTY()
	bool bResultAsIncrement_DEPRECATED = false;

	/** Value added to the counter when filters pass (use minus sign to decrement) */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" ├─ Pass Increment", EditCondition = "Action == EPCGExResultWriteAction::Counter", EditConditionHides))
	double PassIncrement = 1;

	/** Value added to the counter when filters fail (use minus sign to decrement) */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Fail Increment", EditCondition = "Action == EPCGExResultWriteAction::Counter", EditConditionHides))
	double FailIncrement = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Action == EPCGExResultWriteAction::Bitmask", EditConditionHides, InlineEditConditionToggle))
	bool bDoBitmaskOpOnPass = true;

	/** Operations executed on the flag when filters pass */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" ├─ Pass Bitmask", EditCondition="bDoBitmaskOpOnPass && Action == EPCGExResultWriteAction::Bitmask"))
	FPCGExBitmaskWithOperation PassBitmask;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Action == EPCGExResultWriteAction::Bitmask", EditConditionHides, InlineEditConditionToggle))
	bool bDoBitmaskOpOnFail = true;

	/** Operations executed on the flag if when filters fail */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Fail Bitmask", EditCondition="bDoBitmaskOpOnFail && Action == EPCGExResultWriteAction::Bitmask"))
	FPCGExBitmaskWithOperation FailBitmask;

	bool Validate(FPCGExContext* InContext) const;
	void Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade);

	void Write(const int32 Index, bool bPass) const;
	void Write(const PCGExMT::FScope& Scope, const TArray<int8>& Results) const;
	void Write(const PCGExMT::FScope& Scope, const TBitArray<>& Results) const;

#if WITH_EDITOR
	void ApplyDeprecation()
	{
		if (bResultAsIncrement_DEPRECATED) { Action = EPCGExResultWriteAction::Counter; }
	}
#endif

protected:
	TSharedPtr<PCGExData::TBuffer<bool>> BoolBuffer;
	TSharedPtr<PCGExData::TBuffer<double>> IncrementBuffer;
	TSharedPtr<PCGExData::TBuffer<int64>> BitmaskBuffer;
};

namespace PCGEx
{
	PCGEXFILTERS_API void TagsToData(UPCGData* Data, const TSharedPtr<PCGExData::FTags>& Tags, const EPCGExTagsToDataAction Action);

	PCGEXFILTERS_API void TagsToData(const TSharedPtr<PCGExData::FPointIO>& Data, const EPCGExTagsToDataAction Action);
}
