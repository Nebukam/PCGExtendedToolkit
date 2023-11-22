// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGExPartitionByValues.h"

class PCGEXTENDEDTOOLKIT_API FPCGExFilter
{
public:
	template <typename T, typename dummy = void>
	static int64 Filter(const T& InValue, const PCGExPartition::FRule& Settings)
	{
		const double Upscaled = static_cast<double>(InValue) * Settings.Upscale;
		const double Filtered = (Upscaled - FGenericPlatformMath::Fmod(Upscaled, Settings.FilterSize)) / Settings.FilterSize;
		return static_cast<int64>(Filtered);
	}

	template <typename dummy = void>
	static int64 Filter(const FVector2D& InValue, const PCGExPartition::FRule& Settings)
	{
		int64 Result = 0;
		switch (Settings.Field)
		{
		case EPCGExOrderedFieldSelection::X:
			Result = Filter(InValue.X, Settings);
			break;
		case EPCGExOrderedFieldSelection::Y:
		case EPCGExOrderedFieldSelection::Z:
		case EPCGExOrderedFieldSelection::W:
			Result = Filter(InValue.Y, Settings);
			break;
		case EPCGExOrderedFieldSelection::XYZ:
		case EPCGExOrderedFieldSelection::XZY:
		case EPCGExOrderedFieldSelection::ZXY:
		case EPCGExOrderedFieldSelection::YXZ:
		case EPCGExOrderedFieldSelection::YZX:
		case EPCGExOrderedFieldSelection::ZYX:
		case EPCGExOrderedFieldSelection::Length:
			Result = Filter(InValue.SquaredLength(), Settings);
			break;
		default: ;
		}
		return Result;
	}

	template <typename dummy = void>
	static int64 Filter(const FVector& InValue, const PCGExPartition::FRule& Settings)
	{
		int64 Result = 0;
		switch (Settings.Field)
		{
		case EPCGExOrderedFieldSelection::X:
			Result = Filter(InValue.X, Settings);
			break;
		case EPCGExOrderedFieldSelection::Y:
			Result = Filter(InValue.Y, Settings);
			break;
		case EPCGExOrderedFieldSelection::Z:
		case EPCGExOrderedFieldSelection::W:
			Result = Filter(InValue.Z, Settings);
			break;
		case EPCGExOrderedFieldSelection::XYZ:
		case EPCGExOrderedFieldSelection::XZY:
		case EPCGExOrderedFieldSelection::YXZ:
		case EPCGExOrderedFieldSelection::YZX:
		case EPCGExOrderedFieldSelection::ZXY:
		case EPCGExOrderedFieldSelection::ZYX:
		case EPCGExOrderedFieldSelection::Length:
			Result = Filter(InValue.SquaredLength(), Settings);
			break;
		default: ;
		}
		return Result;
	}

	template <typename dummy = void>
	static int64 Filter(const FVector4& InValue, const PCGExPartition::FRule& Settings)
	{
		if (Settings.Field == EPCGExSingleField::W) { return Filter(InValue.W, Settings); }
		return Filter(FVector{InValue}, Settings);
	}

	template <typename dummy = void>
	static int64 Filter(const FRotator& InValue, const PCGExPartition::FRule& Settings)
	{
		return Filter(FVector{InValue.Euler()}, Settings);
	}

	template <typename dummy = void>
	static int64 Filter(const FQuat& InValue, const PCGExPartition::FRule& Settings)
	{
		return Filter(FVector{InValue.Euler()}, Settings);
	}

	template <typename dummy = void>
	static int64 Filter(const FTransform& InValue, const PCGExPartition::FRule& Settings)
	{
		return Filter(InValue.GetLocation(), Settings);
	}

	template <typename dummy = void>
	static int64 Filter(const FString& InValue, const PCGExPartition::FRule& Settings)
	{
		return GetTypeHash(InValue);
	}

	template <typename dummy = void>
	static int64 Filter(const FName& InValue, const PCGExPartition::FRule& Settings)
	{
		return Filter(InValue.ToString(), Settings);
	}
};
