// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGExMath.h"
#include "Details/PCGExSettingsMacros.h"
#include "Data/PCGExDataFilter.h"
#include "Sampling/PCGExSampling.h"
#include "PCGExMeshGrammar.generated.h"

struct FPCGExAssetStagingData;
class UPCGExMeshCollection;
struct FPCGExMeshCollectionEntry;
struct FPCGSubdivisionSubmodule;
class UPCGExAssetCollection;

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
	Min     = 3 UMETA(DisplayName = "Min", Tooltip="Min size", ActionIcon="MinSize"),
	Max     = 4 UMETA(DisplayName = "Max", Tooltip="Max size.", ActionIcon="MaxSize"),
	Average = 5 UMETA(DisplayName = "Average", Tooltip="Average of all axes.", ActionIcon="AverageSize"),
};

UENUM()
enum class EPCGExCollectionGrammarSize : uint8
{
	Fixed   = 0 UMETA(DisplayName = "Fixed", Tooltip="Fixed size.", ActionIcon="Constant"),
	Min     = 1 UMETA(DisplayName = "Min", Tooltip="Uses the smallest size found within the collection entries.", ActionIcon="MinSize"),
	Max     = 2 UMETA(DisplayName = "Max", Tooltip="Uses the largest size found within the collection entries.", ActionIcon="MaxSize"),
	Average = 3 UMETA(DisplayName = "Average", Tooltip="Uses an average of the sizes of all the collection entries.", ActionIcon="AverageSize"),
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Mesh Grammar Details")
struct PCGEXTENDEDTOOLKIT_API FPCGExMeshGrammarDetails
{
	GENERATED_BODY()

	FPCGExMeshGrammarDetails() = default;
	
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

	void Fix(const FBox& InBounds, FPCGSubdivisionSubmodule& OutSubmodule) const;
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Mesh Collection Grammar Details")
struct PCGEXTENDEDTOOLKIT_API FPCGExMeshCollectionGrammarDetails
{
	GENERATED_BODY()

	FPCGExMeshCollectionGrammarDetails() = default;

	/** If enabled, items within that collection will be flattened into their parent context. Note that hoisting is not recursive. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bFlatten = false;
	
	/** Symbol for the grammar. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="!bFlatten", EditConditionHides))
	FName Symbol = NAME_None;

	/** If the volume can be scaled to fit the remaining space or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="!bFlatten", EditConditionHides))
	EPCGExGrammarScaleMode ScaleMode = EPCGExGrammarScaleMode::Fixed;
	
	/** How to define the size of this collection "as a grammar module"*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayAfter="ScaleMode", EditCondition="!bFlatten", EditConditionHides))
	EPCGExCollectionGrammarSize SizeMode = EPCGExCollectionGrammarSize::Min;

	/** Fixed size */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayAfter="SizeMode", EditCondition="!bFlatten && SizeMode==EPCGExCollectionGrammarSize::Fixed", EditConditionHides))
	double Size = 100;

	/** For easier debugging, using Point color in conjunction with PCG Debug Color Material. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="!bFlatten", EditConditionHides))
	FLinearColor DebugColor = FLinearColor::White;

	void Fix(const UPCGExMeshCollection* InCollection, FPCGSubdivisionSubmodule& OutSubmodule) const;
};

namespace PCGExMeshGrammar
{
	PCGEXTENDEDTOOLKIT_API
	void FixModule(const UPCGExMeshCollection* Collection, FPCGSubdivisionSubmodule& OutSubmodule);
	
	PCGEXTENDEDTOOLKIT_API
	void FixModule(const FPCGExMeshCollectionEntry* Entry, const UPCGExMeshCollection* Collection, FPCGSubdivisionSubmodule& OutSubmodule);
}
