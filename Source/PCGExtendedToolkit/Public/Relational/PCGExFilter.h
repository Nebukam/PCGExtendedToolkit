// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"

class PCGEXTENDEDTOOLKIT_API UPCGExFilter
{
public:
	////////////////

	template <typename T, typename dummy = void>
	static int64 Filter(const T& InValue, const FPCGExBucketSettings& Settings)
	{
		const double Upscaled = static_cast<double>(InValue) * Settings.Upscale;
		const double Filtered = (Upscaled - FGenericPlatformMath::Fmod(Upscaled, Settings.FilterSize)) / Settings.FilterSize;
		return static_cast<int64>(Filtered);
	}

	template <typename dummy = void>
	static int64 Filter(const FVector2D& InValue, const FPCGExBucketSettings& Settings)
	{
		int64 Result = 0;
		switch (Settings.ComponentSelection)
		{
		case EComponentSelection::X:
			Result = Filter(InValue.X, Settings);
			break;
		case EComponentSelection::Y:
		case EComponentSelection::Z:
		case EComponentSelection::W:
			Result = Filter(InValue.Y, Settings);
			break;
		case EComponentSelection::XYZ:
		case EComponentSelection::XZY:
		case EComponentSelection::ZXY:
		case EComponentSelection::YXZ:
		case EComponentSelection::YZX:
		case EComponentSelection::ZYX:
		case EComponentSelection::Length:
			Result = Filter(InValue.SquaredLength(), Settings);
			break;
		default: ;
		}
		return Result;
	}

	template <typename dummy = void>
	static int64 Filter(const FVector& InValue, const FPCGExBucketSettings& Settings)
	{
		int64 Result = 0;
		switch (Settings.ComponentSelection)
		{
		case EComponentSelection::X:
			Result = Filter(InValue.X, Settings);
			break;
		case EComponentSelection::Y:
			Result = Filter(InValue.Y, Settings);
			break;
		case EComponentSelection::Z:
		case EComponentSelection::W:
			Result = Filter(InValue.Z, Settings);
			break;
		case EComponentSelection::XYZ:
		case EComponentSelection::XZY:
		case EComponentSelection::YXZ:
		case EComponentSelection::YZX:
		case EComponentSelection::ZXY:
		case EComponentSelection::ZYX:
		case EComponentSelection::Length:
			Result = Filter(InValue.SquaredLength(), Settings);
			break;
		default: ;
		}
		return Result;
	}

	template <typename dummy = void>
	static int64 Filter(const FVector4& InValue, const FPCGExBucketSettings& Settings)
	{
		if (Settings.ComponentSelection == EComponentSelection::W)
		{
			return Filter(InValue.W, Settings);
		}
		return Filter(FVector{InValue}, Settings);
	}

	template <typename dummy = void>
	static int64 Filter(const FRotator& InValue, const FPCGExBucketSettings& Settings)
	{
		return Filter(FVector{InValue.Euler()}, Settings);
	}

	template <typename dummy = void>
	static int64 Filter(const FQuat& InValue, const FPCGExBucketSettings& Settings)
	{
		return Filter(FVector{InValue.Euler()}, Settings);
	}

	template <typename dummy = void>
	static int64 Filter(const FTransform& InValue, const FPCGExBucketSettings& Settings)
	{
		return Filter(InValue.GetLocation(), Settings);
	}

	template <typename dummy = void>
	static int64 Filter(const FString& InValue, const FPCGExBucketSettings& Settings)
	{
		return GetTypeHash(InValue);
	}

	template <typename dummy = void>
	static int64 Filter(const FName& InValue, const FPCGExBucketSettings& Settings)
	{
		return Filter(InValue.ToString(), Settings);
	}
};
