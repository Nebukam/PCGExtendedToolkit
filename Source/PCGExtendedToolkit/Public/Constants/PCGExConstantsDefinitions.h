// Copyright 2024 Edward Boucher and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "CoreMinimal.h"

// An enum used to identify the constants in the namespace below. This is perhaps a slightly messy way of doing things,
// but it means we can use PCGMetadataElementCommon::FillPreconfiguredSettingsInfoFromEnum to create different nodes
// for each.
UENUM(BlueprintType)
enum class EPCGExConstantListID : uint8
{
	// Numeric constants
	// These go before the others, as there's more of them, and that way we can just access them by index.
	ZeroAndOne = 0 UMETA(DisplayName="0 and 1"),
	MinusOne UMETA(DisplayName="-1"),
	Twos UMETA(DisplayName="0.5 and 2"),
	Tens UMETA(DisplayName="Powers of 10"),
	Irrationals,
	Angles,
	Zero UMETA(DisplayName="0"),
	One UMETA(DisplayName="1"),

	// Vectors (for now just the axes)
	Vectors,

	// Booleans
	Booleans UMETA(DisplayName="True and False"),
};

ENUM_CLASS_FLAGS(EPCGExConstantListID)

namespace PCGExConstants
{
	template <typename T>
	struct TDescriptor
	{
		FName Name;
		T Value;
	};

	template <typename T>
	struct TDescriptorList
	{
		FName GroupName;
		TArray<TDescriptor<T>> Constants;
	};

	template <typename T>
	struct TDescriptorListGroup
	{
		FName DisplayName; // Not currently needed, but helps identify each in the namespace below
		TArray<TDescriptorList<T>> ExportedConstants;
	};

	inline TDescriptorListGroup<double> Numbers = {
		"Numeric Constants",
		{
			{
				"Zero and One",

				{
					{"0", 0.0},
					{"1", 1.0}
				}
			},
			{
				"Minus One",
				{{"-1", -1.0}}
			},
			{
				"Twos",
				{
					{"Half", 0.5},
					{"2", 2.0}
				}
			},
			{
				"Tens",
				{
					{"10", 10.0},
					{"100", 100.0},
					{"1000", 1000.0},
				}
			},
			{
				"Irrationals",
				{
					{"Pi", DOUBLE_PI},
					{"Tau", DOUBLE_PI * 2.0},
					{"E", DOUBLE_EULERS_NUMBER},
					{"Root 2", DOUBLE_UE_SQRT_2},
					{"Golden Ratio", UE_DOUBLE_GOLDEN_RATIO}
				}
			},
			{
				"Angles",

				{
					{"90", 90.0},
					{"180", 180.0},
					{"270", 270.0},
					{"360", 360.0},
					{"45", 45.0}
				}
			},
			{
				"Zero",
				{{"0", 0.0}}
			},
			{
				"One",
				{{"1", 1.0}}
			}


		}
	};

	inline TDescriptorListGroup<FVector> Vectors{
		"Vector Constants",
		{
			{
				"Axes",

				{
					{"Up", FVector::UpVector},
					{"Right", FVector::RightVector},
					{"Forward", FVector::ForwardVector}
				}
			}
		}
	};

	inline TArray<TDescriptor<bool>> Booleans = {
		{"True", true},
		{"False", false}
	};
}
