// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#define DBL_INTERSECTION_TOLERANCE 0.1
#define DBL_COLLOCATION_TOLERANCE 0.1
#define DBL_COMPARE_TOLERANCE 0.01
#define MIN_dbl_neg MAX_dbl *-1

#include "CoreMinimal.h"
#include "PCGExCommon.generated.h"

struct FPCGTaggedData;
class UPCGData;
class IPCGAttributeAccessorKeys;

UENUM()
enum class EPCGExAsyncPriority : uint8
{
	Default          = 0 UMETA(DisplayName = "Default", ToolTip="..."),
	Normal           = 1 UMETA(DisplayName = "Normal", ToolTip="..."),
	High             = 2 UMETA(DisplayName = "High", ToolTip="..."),
	BackgroundHigh   = 3 UMETA(DisplayName = "BackgroundHigh", ToolTip="..."),
	BackgroundNormal = 4 UMETA(DisplayName = "BackgroundNormal", ToolTip="..."),
	BackgroundLow    = 5 UMETA(DisplayName = "BackgroundLow", ToolTip="..."),
};

UENUM()
enum class EPCGExInputValueType : uint8
{
	Constant  = 0 UMETA(DisplayName = "Constant", Tooltip="Constant."),
	Attribute = 1 UMETA(DisplayName = "Attribute", Tooltip="Attribute."),
};

UENUM()
enum class EPCGExDataInputValueType : uint8
{
	Constant  = 0 UMETA(DisplayName = "Constant", Tooltip="Constant."),
	Attribute = 1 UMETA(DisplayName = "@Data", Tooltip="Attribute. Can only read from @Data domain."),
};

UENUM()
enum class EPCGExPointBoundsSource : uint8
{
	ScaledBounds  = 0 UMETA(DisplayName = "Scaled Bounds", ToolTip="Scaled Bounds"),
	DensityBounds = 1 UMETA(DisplayName = "Density Bounds", ToolTip="Density Bounds (scaled + steepness)"),
	Bounds        = 2 UMETA(DisplayName = "Bounds", ToolTip="Unscaled Bounds (why?)"),
	Center        = 3 UMETA(DisplayName = "Center", ToolTip="A tiny size 1 box.")
};

UENUM()
enum class EPCGExSplineMeshAxis : uint8
{
	Default = 0 UMETA(Hidden),
	X       = 1 UMETA(DisplayName = "X", ToolTip="X Axis"),
	Y       = 2 UMETA(DisplayName = "Y", ToolTip="Y Axis"),
	Z       = 3 UMETA(DisplayName = "Z", ToolTip="Z Axis"),
};


namespace PCGExData
{
	class FTags;
	template <typename T>
	class TDataValue;

	enum class EIOSide : uint8
	{
		In,
		Out
	};

	struct FTaggedData
	{
		const UPCGData* Data = nullptr;
		TWeakPtr<FTags> Tags;
		TSharedPtr<IPCGAttributeAccessorKeys> Keys = nullptr;

		FTaggedData() = default;
		FTaggedData(const UPCGData* InData, const TSharedPtr<FTags>& InTags, const TSharedPtr<IPCGAttributeAccessorKeys>& InKeys);
		TSharedPtr<FTags> GetTags() const;
		void Dump(FPCGTaggedData& InOut) const;
	};
}

namespace PCGExCommon
{
	using DataIDType = TSharedPtr<PCGExData::TDataValue<int32>>;
	using ContextState = uint64;

	const FString PCGExPrefix = TEXT("PCGEx/");

#define PCGEX_CTX_STATE(_NAME) const PCGExCommon::ContextState _NAME = GetTypeHash(FName(#_NAME));

	PCGEX_CTX_STATE(State_Preparation)
	PCGEX_CTX_STATE(State_LoadingAssetDependencies)
	PCGEX_CTX_STATE(State_AsyncPreparation)
	PCGEX_CTX_STATE(State_FacadePreloading)

	PCGEX_CTX_STATE(State_InitialExecution)
	PCGEX_CTX_STATE(State_ReadyForNextPoints)
	PCGEX_CTX_STATE(State_ProcessingPoints)

	PCGEX_CTX_STATE(State_WaitingOnAsyncWork)
	PCGEX_CTX_STATE(State_Done)

	PCGEX_CTX_STATE(State_Processing)
	PCGEX_CTX_STATE(State_Completing)
	PCGEX_CTX_STATE(State_Writing)

	PCGEX_CTX_STATE(State_UnionWriting)
}
