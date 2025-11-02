// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDetailsFiltering.generated.h"

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

	template<typename T>
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

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExFilterResultDetails
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
	
	/** Name of the attribute to write the result to. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName ResultAttributeName = FName("Result");

	/** If enabled, instead of writing the result as a simple bool, the node will add a int value based on whether it's a pass or fail. Very handy to combine multiple refinements without altering the cluster. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	bool bResultAsIncrement = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" ├─ Pass Increment", EditCondition = "bResultAsIncrement", EditConditionHides))
	double PassIncrement = 1;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Fail Increment", EditCondition = "bResultAsIncrement", EditConditionHides))
	double FailIncrement = 0;

	bool Validate(FPCGExContext* InContext) const;
	void Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade);

	void Write(const int32 Index, bool bPass) const;
	void Write(const PCGExMT::FScope& Scope, const TArray<int8>& Results) const;
	void Write(const PCGExMT::FScope& Scope, const TBitArray<>& Results) const;

protected:
	TSharedPtr<PCGExData::TBuffer<bool>> BoolBuffer;
	TSharedPtr<PCGExData::TBuffer<double>> IncrementBuffer;
};

namespace PCGEx
{
	PCGEXTENDEDTOOLKIT_API
	void TagsToData(UPCGData* Data, const TSharedPtr<PCGExData::FTags>& Tags, const EPCGExTagsToDataAction Action);

	PCGEXTENDEDTOOLKIT_API
	void TagsToData(const TSharedPtr<PCGExData::FPointIO>& Data, const EPCGExTagsToDataAction Action);
}
