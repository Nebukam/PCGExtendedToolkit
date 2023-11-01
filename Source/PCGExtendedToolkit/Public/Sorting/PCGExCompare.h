// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#define PCGEX_COMPARE_1(FIELD) \
Result = Compare(A.FIELD, B.FIELD, Tolerance, Comp); \
if(Result != 0){return Result;}
#define PCGEX_COMPARE_2(FIELD_A, FIELD_B) \
PCGEX_COMPARE_1(FIELD_A) \
PCGEX_COMPARE_1(FIELD_B)
#define PCGEX_COMPARE_3(FIELD_A, FIELD_B, FIELD_C) \
PCGEX_COMPARE_2(FIELD_A, FIELD_B) \
PCGEX_COMPARE_1(FIELD_C)

UENUM(BlueprintType)
enum class ESortComponent : uint8
{
	X UMETA(DisplayName = "X"),
	Y UMETA(DisplayName = "Y (← X)"),
	Z UMETA(DisplayName = "Z (← Y)"),
	W UMETA(DisplayName = "W (← Z)"),
	XYZ UMETA(DisplayName = "X → Y → Z"),
	XZY UMETA(DisplayName = "X → Z → Y"),
	YXZ UMETA(DisplayName = "Y → X → Z"),
	YZX UMETA(DisplayName = "Y → Z → X"),
	ZXY UMETA(DisplayName = "Z → X → Y"),
	ZYX UMETA(DisplayName = "Z → Y → X"),
	Length UMETA(DisplayName = "length"),
};

class PCGEXTENDEDTOOLKIT_API UPCGExCompare
{
	
public:
	
	template <typename T, typename dummy = void>
	static int Compare(const T& A, const T& B, const double Tolerance = 0.0001f, ESortComponent Comp = ESortComponent::X)
	{
		return FMath::IsNearlyEqual(static_cast<double>(A), static_cast<double>(B), Tolerance) ? 0 : A < B ? -1 : 1;
	}

	/*
	template <typename dummy = void>
	static int Compare(const int32& A, const int32& B, const double Tolerance, ESortComponent Comp)
	{
		return A == B ? 0 : A < B ? -1 : 1;
	}

	template <typename dummy = void>
	static int Compare(const int64& A, const int64& B, const double Tolerance, ESortComponent Comp)
	{
		return A == B ? 0 : A < B ? -1 : 1;
	}
	*/

	template <typename dummy = void>
	static int Compare(const FVector2D& A, const FVector2D& B, const double Tolerance, ESortComponent Comp)
	{
		int Result = 0;
		switch (Comp)
		{
		case ESortComponent::X:
			PCGEX_COMPARE_1(X)
			break;
		case ESortComponent::Y:
		case ESortComponent::Z:
		case ESortComponent::W:
			PCGEX_COMPARE_1(Y)
			break;
		case ESortComponent::XYZ:
		case ESortComponent::XZY:
		case ESortComponent::ZXY:
			PCGEX_COMPARE_2(X, Y)
			break;
		case ESortComponent::YXZ:
		case ESortComponent::YZX:
		case ESortComponent::ZYX:
			PCGEX_COMPARE_2(Y, X)
			break;
		case ESortComponent::Length:
			PCGEX_COMPARE_1(SquaredLength())
			break;
		default: ;
		}
		return Result;
	}

	template <typename dummy = void>
	static int Compare(const FVector& A, const FVector& B, const double Tolerance, ESortComponent Comp)
	{
		int Result = 0;
		switch (Comp)
		{
		case ESortComponent::X:
			PCGEX_COMPARE_1(X)
			break;
		case ESortComponent::Y:
			PCGEX_COMPARE_1(Y)
			break;
		case ESortComponent::Z:
		case ESortComponent::W:
			PCGEX_COMPARE_1(Z)
			break;
		case ESortComponent::XYZ:
			PCGEX_COMPARE_3(X, Y, Z)
			break;
		case ESortComponent::XZY:
			PCGEX_COMPARE_3(X, Z, Y)
			break;
		case ESortComponent::YXZ:
			PCGEX_COMPARE_3(Y, X, Z)
			break;
		case ESortComponent::YZX:
			PCGEX_COMPARE_3(Y, Z, X)
			break;
		case ESortComponent::ZXY:
			PCGEX_COMPARE_3(Z, X, Y)
			break;
		case ESortComponent::ZYX:
			PCGEX_COMPARE_3(Z, Y, X)
			break;
		case ESortComponent::Length:
			PCGEX_COMPARE_1(SquaredLength())
			break;
		default: ;
		}
		return Result;
	}

	template <typename dummy = void>
	static int Compare(const FVector4& A, const FVector4& B, const double Tolerance, ESortComponent Comp)
	{
		if (Comp == ESortComponent::W) { return Compare(A.W, B.W, Tolerance, Comp); }
		return Compare(FVector{A}, FVector{B}, Tolerance, Comp);
	}

	template <typename dummy = void>
	static int Compare(const FRotator& A, const FRotator& B, const double Tolerance, ESortComponent Comp)
	{
		return Compare(FVector{A.Euler()}, FVector{B.Euler()}, Tolerance, Comp);
	}

	template <typename dummy = void>
	static int Compare(const FQuat& A, const FQuat& B, const double Tolerance, ESortComponent Comp)
	{
		return Compare(FVector{A.Euler()}, FVector{B.Euler()}, Tolerance, Comp);
	}

	template <typename dummy = void>
	static int Compare(const FString& A, const FString& B, const double Tolerance, ESortComponent Comp)
	{
		return A < B ? -1 : A == B ? 0 : 1;
	}

	template <typename dummy = void>
	static int Compare(const FName& A, const FName& B, const double Tolerance, ESortComponent Comp)
	{
		return Compare(A.ToString(), B.ToString(), Tolerance, Comp);
	}

	template <typename dummy = void>
	static int Compare(const FTransform& A, const FTransform& B, const double Tolerance, ESortComponent Comp)
	{
		return Compare(A.GetLocation(), B.GetLocation(), Tolerance, Comp);
	}
	
};

#undef PCGEX_COMPARE_1
#undef PCGEX_COMPARE_2
#undef PCGEX_COMPARE_3
