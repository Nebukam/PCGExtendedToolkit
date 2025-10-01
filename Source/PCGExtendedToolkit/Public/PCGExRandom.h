// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGExMath.h"

class UPCGComponent;
class UPCGSettings;

UENUM(meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Seed Components"))
enum class EPCGExSeedComponents : uint8
{
	None      = 0,
	Local     = 1 << 1 UMETA(DisplayName = "Local"),
	Settings  = 1 << 2 UMETA(DisplayName = "Settings"),
	Component = 1 << 3 UMETA(DisplayName = "Component"),
};

ENUM_CLASS_FLAGS(EPCGExSeedComponents)
using EPCGExSeedComponentsBitmask = TEnumAsByte<EPCGExSeedComponents>;

namespace PCGExRandom
{
	PCGEXTENDEDTOOLKIT_API
	int32 GetSeed(const int32 BaseSeed, const uint8 Flags, const int32 Local, const UPCGSettings* Settings = nullptr, const UPCGComponent* Component = nullptr);

	PCGEXTENDEDTOOLKIT_API
	int32 GetSeed(const int32 BaseSeed, const int32 Local, const UPCGSettings* Settings = nullptr, const UPCGComponent* Component = nullptr);

	PCGEXTENDEDTOOLKIT_API
	FRandomStream GetRandomStreamFromPoint(const int32 BaseSeed, const int32 Offset, const UPCGSettings* Settings = nullptr, const UPCGComponent* Component = nullptr);

	PCGEXTENDEDTOOLKIT_API
	int ComputeSpatialSeed(const FVector& Origin, const FVector& Offset = FVector::ZeroVector);
}
