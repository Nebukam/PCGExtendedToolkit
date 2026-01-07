// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExAssetGrammar.generated.h"

struct FPCGExAssetStagingData;
class UPCGExAssetCollection;
struct FPCGExAssetCollectionEntry;
struct FPCGSubdivisionSubmodule;

UENUM()
enum class EPCGExGrammarScaleMode : uint8
{
	Fixed = 0 UMETA(DisplayName = "Fixed", Tooltip="Fixed size. Will use the bound size of the selected axis.", ActionIcon="Fixed"),
	Flex  = 1 UMETA(DisplayName = "Flexible", Tooltip="Flexible size. Will use the bound size of the selected axis as a base but will be marked scalable.", ActionIcon="Flexible"),
};

UENUM()
enum class EPCGExGrammarSizeReference : uint8
{
	X       = 0 UMETA(DisplayName = "X", Tooltip="X size", ActionIcon="X"),
	Y       = 1 UMETA(DisplayName = "Y", Tooltip="Y size", ActionIcon="Y"),
	Z       = 2 UMETA(DisplayName = "Z", Tooltip="Z axis", ActionIcon="Z"),
	Min     = 3 UMETA(DisplayName = "Smallest", Tooltip="Use smallest axis size", ActionIcon="MinSize"),
	Max     = 4 UMETA(DisplayName = "Largest", Tooltip="Use largest axis size", ActionIcon="MaxSize"),
	Average = 5 UMETA(DisplayName = "Average", Tooltip="Average size of all axes.", ActionIcon="AverageSize"),
};

UENUM()
enum class EPCGExGrammarSubCollectionMode : uint8
{
	Inherit  = 0 UMETA(DisplayName = "Inherit", Tooltip="Inherit the settings from the selected collection."),
	Override = 1 UMETA(DisplayName = "Override", Tooltip="Override the collection internal settings with custom ones."),
	Flatten  = 2 UMETA(DisplayName = "Flatten", Tooltip="Hoist the collection entries as if they were part of this collection."),
};

UENUM()
enum class EPCGExCollectionGrammarSize : uint8
{
	Fixed   = 0 UMETA(DisplayName = "Fixed", Tooltip="Fixed size.", ActionIcon="Constant"),
	Min     = 1 UMETA(DisplayName = "Smallest", Tooltip="Uses the smallest size found within the collection entries.", ActionIcon="MinSize"),
	Max     = 2 UMETA(DisplayName = "Largest", Tooltip="Uses the largest size found within the collection entries.", ActionIcon="MaxSize"),
	Average = 3 UMETA(DisplayName = "Average", Tooltip="Uses an average of the sizes of all the collection entries.", ActionIcon="AverageSize"),
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Mesh Grammar Details")
struct PCGEXCOLLECTIONS_API FPCGExAssetGrammarDetails
{
	GENERATED_BODY()

	FPCGExAssetGrammarDetails() = default;

	FPCGExAssetGrammarDetails(const FName InSymbol)
		: Symbol(InSymbol)
	{
	}

	/** Symbol for the grammar. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName Symbol = NAME_None;

	/** If the volume can be scaled to fit the remaining space or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExGrammarScaleMode ScaleMode = EPCGExGrammarScaleMode::Fixed;

	/** If the volume can be scaled to fit the remaining space or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExGrammarSizeReference Size = EPCGExGrammarSizeReference::X;

	/** For easier debugging, using Point color in conjunction with PCG Debug Color Material. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FLinearColor DebugColor = FLinearColor::White;

	double GetSize(const FBox& InBounds, TMap<const FPCGExAssetCollectionEntry*, double>* SizeCache = nullptr) const;
	void Fix(const FBox& InBounds, FPCGSubdivisionSubmodule& OutSubmodule, TMap<const FPCGExAssetCollectionEntry*, double>* SizeCache = nullptr) const;
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Mesh Collection Grammar Details")
struct PCGEXCOLLECTIONS_API FPCGExCollectionGrammarDetails
{
	GENERATED_BODY()

	FPCGExCollectionGrammarDetails() = default;

	/** Symbol for the grammar. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName Symbol = NAME_None;

	/** If the volume can be scaled to fit the remaining space or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExGrammarScaleMode ScaleMode = EPCGExGrammarScaleMode::Fixed;

	/** How to define the size of this collection "as a grammar module"*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExCollectionGrammarSize SizeMode = EPCGExCollectionGrammarSize::Min;

	/** Fixed size */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="SizeMode == EPCGExCollectionGrammarSize::Fixed", EditConditionHides))
	double Size = 100;

	/** For easier debugging, using Point color in conjunction with PCG Debug Color Material. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FLinearColor DebugColor = FLinearColor::White;

	double GetSize(const UPCGExAssetCollection* InCollection, TMap<const FPCGExAssetCollectionEntry*, double>* SizeCache = nullptr) const;
	void Fix(const UPCGExAssetCollection* InCollection, FPCGSubdivisionSubmodule& OutSubmodule, TMap<const FPCGExAssetCollectionEntry*, double>* SizeCache = nullptr) const;
};
