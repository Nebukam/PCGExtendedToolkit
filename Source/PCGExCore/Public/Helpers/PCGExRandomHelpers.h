// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

#include "PCGExRandomHelpers.generated.h"

class UPCGComponent;
class UPCGSettings;

UENUM(meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Seed Components"))
enum class EPCGExSeedComponents : uint8
{
	None      = 0 UMETA(Hidden),
	Local     = 1 << 1 UMETA(DisplayName = "Local"),
	Settings  = 1 << 2 UMETA(DisplayName = "Settings"),
	Component = 1 << 3 UMETA(DisplayName = "Component"),
};

ENUM_CLASS_FLAGS(EPCGExSeedComponents)
using EPCGExSeedComponentsBitmask = TEnumAsByte<EPCGExSeedComponents>;

namespace PCGExRandomHelpers
{
	FORCEINLINE static double FastRand01(uint32& Seed)
	{
		Seed = Seed * 1664525u + 1013904223u;
		return (Seed & 0x00FFFFFF) / static_cast<double>(0x01000000);
	}

	PCGEXCORE_API int32 GetSeed(const int32 BaseSeed, const uint8 Flags, const int32 Local, const UPCGSettings* Settings = nullptr, const UPCGComponent* Component = nullptr);

	PCGEXCORE_API int32 GetSeed(const int32 BaseSeed, const int32 Local, const UPCGSettings* Settings = nullptr, const UPCGComponent* Component = nullptr);

	PCGEXCORE_API FRandomStream GetRandomStreamFromPoint(const int32 BaseSeed, const int32 Offset, const UPCGSettings* Settings = nullptr, const UPCGComponent* Component = nullptr);

	PCGEXCORE_API int ComputeSpatialSeed(const FVector& Origin, const FVector& Offset = FVector::ZeroVector);
}
