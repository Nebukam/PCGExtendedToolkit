// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "PCGExDetailsAttributes.generated.h"

struct FPCGExContext;

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeSourceToTargetDetails
{
	GENERATED_BODY()

	FPCGExAttributeSourceToTargetDetails() = default;

	/** Attribute to read on input */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName Source = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bOutputToDifferentName = false;

	/** Attribute to write on output, if different from input */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bOutputToDifferentName"))
	FName Target = NAME_None;

	bool WantsRemappedOutput() const { return (bOutputToDifferentName && Source != GetOutputName()); }

	bool ValidateNames(FPCGExContext* InContext) const;
	bool ValidateNamesOrProperties(FPCGExContext* InContext) const;

	FName GetOutputName() const;

	FPCGAttributePropertyInputSelector GetSourceSelector() const;
	FPCGAttributePropertyInputSelector GetTargetSelector() const;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeSourceToTargetList
{
	GENERATED_BODY()

	FPCGExAttributeSourceToTargetList() = default;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, TitleProperty="{Source}"))
	TArray<FPCGExAttributeSourceToTargetDetails> Attributes;

	bool IsEmpty() const { return Attributes.IsEmpty(); }
	int32 Num() const { return Attributes.Num(); }

	bool ValidateNames(FPCGExContext* InContext) const;
	void GetSources(TArray<FName>& OutNames) const;
};
