// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "PCGEx.h"
#include "PCGExMath.h"

namespace PCGExData
{
	enum class EIOSide : uint8;
	class FFacade;
}

namespace PCGEx
{
#pragma region Field helpers

	using FInputSelectorComponentData = TTuple<EPCGExTransformComponent, EPCGMetadataTypes>;
	// Transform component, root type
	static const TMap<FString, FInputSelectorComponentData> STRMAP_TRANSFORM_FIELD = {
		{TEXT("POSITION"), FInputSelectorComponentData{EPCGExTransformComponent::Position, EPCGMetadataTypes::Vector}},
		{TEXT("POS"), FInputSelectorComponentData{EPCGExTransformComponent::Position, EPCGMetadataTypes::Vector}},
		{TEXT("ROTATION"), FInputSelectorComponentData{EPCGExTransformComponent::Rotation, EPCGMetadataTypes::Quaternion}},
		{TEXT("ROT"), FInputSelectorComponentData{EPCGExTransformComponent::Rotation, EPCGMetadataTypes::Quaternion}},
		{TEXT("ORIENT"), FInputSelectorComponentData{EPCGExTransformComponent::Rotation, EPCGMetadataTypes::Quaternion}},
		{TEXT("SCALE"), FInputSelectorComponentData{EPCGExTransformComponent::Scale, EPCGMetadataTypes::Vector}},
	};

	using FInputSelectorFieldData = TTuple<EPCGExSingleField, EPCGMetadataTypes, int32>;
	// Single component, root type
	static const TMap<FString, FInputSelectorFieldData> STRMAP_SINGLE_FIELD = {
		{TEXT("X"), FInputSelectorFieldData{EPCGExSingleField::X, EPCGMetadataTypes::Vector, 0}},
		{TEXT("R"), FInputSelectorFieldData{EPCGExSingleField::X, EPCGMetadataTypes::Quaternion, 0}},
		{TEXT("ROLL"), FInputSelectorFieldData{EPCGExSingleField::X, EPCGMetadataTypes::Quaternion, 0}},
		{TEXT("RX"), FInputSelectorFieldData{EPCGExSingleField::X, EPCGMetadataTypes::Quaternion, 0}},
		{TEXT("Y"), FInputSelectorFieldData{EPCGExSingleField::Y, EPCGMetadataTypes::Vector, 1}},
		{TEXT("G"), FInputSelectorFieldData{EPCGExSingleField::Y, EPCGMetadataTypes::Vector4, 1}},
		{TEXT("YAW"), FInputSelectorFieldData{EPCGExSingleField::Y, EPCGMetadataTypes::Quaternion, 1}},
		{TEXT("RY"), FInputSelectorFieldData{EPCGExSingleField::Y, EPCGMetadataTypes::Quaternion, 1}},
		{TEXT("Z"), FInputSelectorFieldData{EPCGExSingleField::Z, EPCGMetadataTypes::Vector, 2}},
		{TEXT("B"), FInputSelectorFieldData{EPCGExSingleField::Z, EPCGMetadataTypes::Vector4, 2}},
		{TEXT("P"), FInputSelectorFieldData{EPCGExSingleField::Z, EPCGMetadataTypes::Quaternion, 2}},
		{TEXT("PITCH"), FInputSelectorFieldData{EPCGExSingleField::Z, EPCGMetadataTypes::Quaternion, 2}},
		{TEXT("RZ"), FInputSelectorFieldData{EPCGExSingleField::Z, EPCGMetadataTypes::Quaternion, 2}},
		{TEXT("W"), FInputSelectorFieldData{EPCGExSingleField::W, EPCGMetadataTypes::Vector4, 3}},
		{TEXT("A"), FInputSelectorFieldData{EPCGExSingleField::W, EPCGMetadataTypes::Vector4, 3}},
		{TEXT("L"), FInputSelectorFieldData{EPCGExSingleField::Length, EPCGMetadataTypes::Vector, 0}},
		{TEXT("LEN"), FInputSelectorFieldData{EPCGExSingleField::Length, EPCGMetadataTypes::Vector, 0}},
		{TEXT("LENGTH"), FInputSelectorFieldData{EPCGExSingleField::Length, EPCGMetadataTypes::Vector, 0}},
		{TEXT("SQUAREDLENGTH"), FInputSelectorFieldData{EPCGExSingleField::SquaredLength, EPCGMetadataTypes::Vector, 0}},
		{TEXT("LENSQR"), FInputSelectorFieldData{EPCGExSingleField::SquaredLength, EPCGMetadataTypes::Vector, 0}},
		{TEXT("VOL"), FInputSelectorFieldData{EPCGExSingleField::Volume, EPCGMetadataTypes::Vector, 0}},
		{TEXT("VOLUME"), FInputSelectorFieldData{EPCGExSingleField::Volume, EPCGMetadataTypes::Vector, 0}},
		{TEXT("SUM"), FInputSelectorFieldData{EPCGExSingleField::Sum, EPCGMetadataTypes::Vector, 0}},
	};

	using FInputSelectorAxisData = TTuple<EPCGExAxis, EPCGMetadataTypes>;
	// Axis, root type
	static const TMap<FString, FInputSelectorAxisData> STRMAP_AXIS = {
		{TEXT("FORWARD"), FInputSelectorAxisData{EPCGExAxis::Forward, EPCGMetadataTypes::Quaternion}},
		{TEXT("FRONT"), FInputSelectorAxisData{EPCGExAxis::Forward, EPCGMetadataTypes::Quaternion}},
		{TEXT("BACKWARD"), FInputSelectorAxisData{EPCGExAxis::Backward, EPCGMetadataTypes::Quaternion}},
		{TEXT("BACK"), FInputSelectorAxisData{EPCGExAxis::Backward, EPCGMetadataTypes::Quaternion}},
		{TEXT("RIGHT"), FInputSelectorAxisData{EPCGExAxis::Right, EPCGMetadataTypes::Quaternion}},
		{TEXT("LEFT"), FInputSelectorAxisData{EPCGExAxis::Left, EPCGMetadataTypes::Quaternion}},
		{TEXT("UP"), FInputSelectorAxisData{EPCGExAxis::Up, EPCGMetadataTypes::Quaternion}},
		{TEXT("TOP"), FInputSelectorAxisData{EPCGExAxis::Up, EPCGMetadataTypes::Quaternion}},
		{TEXT("DOWN"), FInputSelectorAxisData{EPCGExAxis::Down, EPCGMetadataTypes::Quaternion}},
		{TEXT("BOTTOM"), FInputSelectorAxisData{EPCGExAxis::Down, EPCGMetadataTypes::Quaternion}},
	};

	bool GetComponentSelection(const TArray<FString>& Names, FInputSelectorComponentData& OutSelection);
	bool GetFieldSelection(const TArray<FString>& Names, FInputSelectorFieldData& OutSelection);
	bool GetAxisSelection(const TArray<FString>& Names, FInputSelectorAxisData& OutSelection);

#pragma endregion

	template <typename T_VALUE, typename T>
	inline static T Convert(const T_VALUE& Value)
	{
#pragma region Convert from bool

		if constexpr (std::is_same_v<T_VALUE, bool>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return Value; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return Value ? 1 : 0; }
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value ? 1 : 0); }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value ? 1 : 0); }
			else if constexpr (std::is_same_v<T, FVector4>)
			{
				const double D = Value ? 1 : 0;
				return FVector4(D, D, D, D);
			}
			else if constexpr (std::is_same_v<T, FQuat>)
			{
				const double D = Value ? 180 : 0;
				return FRotator(D, D, D).Quaternion();
			}
			else if constexpr (std::is_same_v<T, FRotator>)
			{
				const double D = Value ? 180 : 0;
				return FRotator(D, D, D);
			}
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
			else if constexpr (std::is_same_v<T, FString>) { return FString::Printf(TEXT("%s"), Value ? TEXT("true") : TEXT("false")); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(FString::Printf(TEXT("%s"), Value ? TEXT("true") : TEXT("false"))); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from Integer32

		else if constexpr (std::is_same_v<T_VALUE, int32>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return Value > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return static_cast<T>(Value); }
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value); }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value, Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value, Value, Value).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
			else if constexpr (std::is_same_v<T, FString>) { return FString::Printf(TEXT("%d"), Value); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(FString::Printf(TEXT("%d"), Value)); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from Integer64

		else if constexpr (std::is_same_v<T_VALUE, int64>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return Value > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return static_cast<T>(Value); }
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value); }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value, Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value, Value, Value).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
			else if constexpr (std::is_same_v<T, FString>) { return FString::Printf(TEXT("%lld"), Value); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(FString::Printf(TEXT("(%lld)"), Value)); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from Float

		else if constexpr (std::is_same_v<T_VALUE, float>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return Value > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return static_cast<T>(Value); }
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value); }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value, Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value, Value, Value).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
			else if constexpr (std::is_same_v<T, FString>) { return FString::Printf(TEXT("%f"), Value); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(FString::Printf(TEXT("(%f)"), Value)); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from Double

		else if constexpr (std::is_same_v<T_VALUE, double>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return Value > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return static_cast<T>(Value); }
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value); }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value, Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value, Value, Value).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
			else if constexpr (std::is_same_v<T, FString>) { return FString::Printf(TEXT("%f"), Value); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(FString::Printf(TEXT("(%f)"), Value)); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FVector2D

		else if constexpr (std::is_same_v<T_VALUE, FVector2D>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return Value.SquaredLength() > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return static_cast<T>(Value.X); }
			else if constexpr (std::is_same_v<T, FVector2D>) { return Value; }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value.X, Value.Y, 0); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.X, Value.Y, 0, 0); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value.X, Value.Y, 0).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value.X, Value.Y, 0); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FVector

		else if constexpr (std::is_same_v<T_VALUE, FVector>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return Value.SquaredLength() > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return static_cast<T>(Value.X); }
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value.X, Value.Y); }
			else if constexpr (std::is_same_v<T, FVector>) { return Value; }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.X, Value.Y, Value.Z, 0); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value.X, Value.Y, Value.Z).Quaternion(); /* TODO : Handle axis selection */ }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value.X, Value.Y, Value.Z); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; /* TODO : Handle axis selection */ }
			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FVector4

		else if constexpr (std::is_same_v<T_VALUE, FVector4>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return FVector(Value.X * Value.Y * Value.Z).SquaredLength() > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return static_cast<T>(Value.X); }
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value.X, Value.Y); }
			else if constexpr (std::is_same_v<T, FVector>) { return Value; }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.X, Value.Y, Value.Z, Value.W); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value.X, Value.Y, Value.Z).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value.X, Value.Y, Value.Z); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; /* TODO : Handle axis */ }
			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FQuat

		else if constexpr (std::is_same_v<T_VALUE, FQuat>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return Value.Euler().SquaredLength() > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return static_cast<T>(Value.X); }
			else if constexpr (std::is_same_v<T, FVector2D>)
			{
				const FVector Euler = Value.Euler();
				return FVector2D(Euler.X, Euler.Y);
			}
			else if constexpr (std::is_same_v<T, FVector>) { return Value.Euler(); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.X, Value.Y, Value.Z, Value.W); }
			else if constexpr (std::is_same_v<T, FQuat>) { return Value; }
			else if constexpr (std::is_same_v<T, FRotator>) { return Value.Rotator(); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform(Value, FVector::ZeroVector, FVector::OneVector); }
			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FRotator

		else if constexpr (std::is_same_v<T_VALUE, FRotator>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(FVector(Value.Pitch, Value.Roll, Value.Yaw)); }

			else if constexpr (std::is_same_v<T, bool>) { return Value.Euler().SquaredLength() > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return static_cast<T>(Value.Pitch); }
			else if constexpr (std::is_same_v<T, FVector2D>) { return Convert<FQuat, FVector2D>(Value.Quaternion()); }
			else if constexpr (std::is_same_v<T, FVector>) { return Convert<FQuat, FVector>(Value.Quaternion()); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.Euler(), 0); /* TODO : Handle axis */ }
			else if constexpr (std::is_same_v<T, FQuat>) { return Value.Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return Value; }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform(Value.Quaternion(), FVector::ZeroVector, FVector::OneVector); }
			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FTransform

		else if constexpr (std::is_same_v<T_VALUE, FTransform>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (
				std::is_same_v<T, bool> ||
				std::is_same_v<T, int32> ||
				std::is_same_v<T, int64> ||
				std::is_same_v<T, float> ||
				std::is_same_v<T, double> ||
				std::is_same_v<T, FVector2D> ||
				std::is_same_v<T, FVector> ||
				std::is_same_v<T, FVector4> ||
				std::is_same_v<T, FQuat> ||
				std::is_same_v<T, FRotator>)
			{
				return Convert<FVector, T>(Value.GetLocation());
			}
			else if constexpr (std::is_same_v<T, FTransform>) { return Value; }
			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FString

		else if constexpr (std::is_same_v<T_VALUE, FString>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, FString>) { return Value; }
			else if constexpr (std::is_same_v<T, FName>) { return FName(Value); }
			else if constexpr (std::is_same_v<T, FSoftClassPath>) { return FSoftClassPath(Value); }
			else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return FSoftObjectPath(Value); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FName

		else if constexpr (std::is_same_v<T_VALUE, FName>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, FString>) { return Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return Value; }
			else if constexpr (std::is_same_v<T, FSoftClassPath>) { return FSoftClassPath(Value.ToString()); }
			else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return FSoftObjectPath(Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FSoftClassPath

		else if constexpr (std::is_same_v<T_VALUE, FSoftClassPath>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, FString>) { return Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(Value.ToString()); }
			else if constexpr (std::is_same_v<T, FSoftClassPath>) { return Value; }
			else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return FSoftObjectPath(Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FSoftObjectPath

		else if constexpr (std::is_same_v<T_VALUE, FSoftObjectPath>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else if constexpr (std::is_same_v<T, FSoftClassPath>) { return FSoftClassPath(Value.ToString()); }
			else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return Value; }
			else { return T{}; }
		}

#pragma endregion
		else { return T{}; }
	}

#define PCGEX_CONVERT_CONVERT_DECL(_TYPE, _ID, ...) template <typename T> inline static T Convert(const _TYPE& Value) { return Convert<_TYPE, T>(Value); }
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_CONVERT_CONVERT_DECL)
#undef PCGEX_CONVERT_CONVERT_DECL

	struct PCGEXTENDEDTOOLKIT_API FSubSelection
	{
		bool bIsValid = false;
		bool bIsAxisSet = false;
		bool bIsFieldSet = false;
		bool bIsComponentSet = false;

		EPCGExTransformComponent Component = EPCGExTransformComponent::Position;
		EPCGExAxis Axis = EPCGExAxis::Forward;
		EPCGExSingleField Field = EPCGExSingleField::X;
		EPCGMetadataTypes PossibleSourceType = EPCGMetadataTypes::Unknown;
		int32 FieldIndex = 0;

		FSubSelection() = default;
		explicit FSubSelection(const TArray<FString>& ExtraNames);
		explicit FSubSelection(const FPCGAttributePropertyInputSelector& InSelector);
		explicit FSubSelection(const FString& Path, const UPCGData* InData = nullptr);

		EPCGMetadataTypes GetSubType(EPCGMetadataTypes Fallback = EPCGMetadataTypes::Unknown) const;
		void SetComponent(const EPCGExTransformComponent InComponent);
		bool SetFieldIndex(const int32 InFieldIndex);

	protected:
		void Init(const TArray<FString>& ExtraNames);

	public:
		void Update();

		template <typename T_VALUE, typename T>
		T Get(const T_VALUE& Value) const
		{
#pragma region Convert from bool

			if constexpr (std::is_same_v<T_VALUE, bool>)
			{
				if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
				else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

				else if constexpr (std::is_same_v<T, bool>) { return Value; }
				else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return Value ? 1 : 0; }
				else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value ? 1 : 0); }
				else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value ? 1 : 0); }
				else if constexpr (std::is_same_v<T, FVector4>)
				{
					const double D = Value ? 1 : 0;
					return FVector4(D, D, D, D);
				}
				else if constexpr (std::is_same_v<T, FQuat>)
				{
					const double D = Value ? 180 : 0;
					return FRotator(D, D, D).Quaternion();
				}
				else if constexpr (std::is_same_v<T, FRotator>)
				{
					const double D = Value ? 180 : 0;
					return FRotator(D, D, D);
				}
				else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
				else if constexpr (std::is_same_v<T, FString>) { return FString::Printf(TEXT("%s"), Value ? TEXT("true") : TEXT("false")); }
				else if constexpr (std::is_same_v<T, FName>) { return FName(FString::Printf(TEXT("%s"), Value ? TEXT("true") : TEXT("false"))); }
				else { return T{}; }
			}

#pragma endregion

#pragma region Convert from Integer32

			else if constexpr (
				std::is_same_v<T_VALUE, int32> ||
				std::is_same_v<T_VALUE, int64> ||
				std::is_same_v<T_VALUE, float> ||
				std::is_same_v<T_VALUE, double>)
			{
				if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
				else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

				else if constexpr (std::is_same_v<T, bool>) { return Value > 0; }
				else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return static_cast<T>(Value); }
				else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value); }
				else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value); }
				else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value, Value, Value, Value); }
				else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value, Value, Value).Quaternion(); }
				else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value, Value, Value); }
				else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
				else if constexpr (std::is_same_v<T, FString>)
				{
					if constexpr (std::is_same_v<T_VALUE, int32>) { return FString::Printf(TEXT("%d"), Value); }
					else if constexpr (std::is_same_v<T_VALUE, int64>) { return FString::Printf(TEXT("%lld"), Value); }
					else if constexpr (std::is_same_v<T_VALUE, float>) { return FString::Printf(TEXT("%f"), Value); }
					else if constexpr (std::is_same_v<T_VALUE, double>) { return FString::Printf(TEXT("%f"), Value); }
					else { return T{}; }
				}
				else if constexpr (std::is_same_v<T, FName>)
				{
					if constexpr (std::is_same_v<T_VALUE, int32>) { return FName(FString::Printf(TEXT("%d"), Value)); }
					else if constexpr (std::is_same_v<T_VALUE, int64>) { return FName(FString::Printf(TEXT("%lld"), Value)); }
					else if constexpr (std::is_same_v<T_VALUE, float>) { return FName(FString::Printf(TEXT("%f"), Value)); }
					else if constexpr (std::is_same_v<T_VALUE, double>) { return FName(FString::Printf(TEXT("%f"), Value)); }
					else { return T{}; }
				}
				else { return T{}; }
			}

#pragma endregion

#pragma region Convert from FVector2D

			else if constexpr (std::is_same_v<T_VALUE, FVector2D>)
			{
				if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
				else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

				else if constexpr (std::is_same_v<T, bool>)
				{
					switch (Field)
					{
					default:
					case EPCGExSingleField::X:
						return Value.X > 0;
					case EPCGExSingleField::Y:
					case EPCGExSingleField::Z:
					case EPCGExSingleField::W:
						return Value.Y > 0;
					case EPCGExSingleField::Length:
					case EPCGExSingleField::SquaredLength:
						return Value.SquaredLength() > 0;
					case EPCGExSingleField::Volume:
						return (Value.X * Value.Y) > 0;
					case EPCGExSingleField::Sum:
						return (Value.X * Value.Y) > 0;
					}
				}
				else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
				{
					switch (Field)
					{
					default:
					case EPCGExSingleField::X:
						return Value.X;
					case EPCGExSingleField::Y:
					case EPCGExSingleField::Z:
					case EPCGExSingleField::W:
						return Value.Y;
					case EPCGExSingleField::Length:
						return Value.Length();
					case EPCGExSingleField::SquaredLength:
						return Value.SquaredLength();
					case EPCGExSingleField::Volume:
						return Value.X * Value.Y;
					case EPCGExSingleField::Sum:
						return Value.X + Value.Y;
					}
				}
				else if constexpr (std::is_same_v<T, FVector2D>) { return Value; }
				else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value.X, Value.Y, 0); }
				else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.X, Value.Y, 0, 0); }
				else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value.X, Value.Y, 0).Quaternion(); }
				else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value.X, Value.Y, 0); }
				else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
				else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
				else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
				else { return T{}; }
			}

#pragma endregion

#pragma region Convert from FVector

			else if constexpr (std::is_same_v<T_VALUE, FVector>)
			{
				if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
				else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

				else if constexpr (std::is_same_v<T, bool>)
				{
					switch (Field)
					{
					default:
					case EPCGExSingleField::X:
						return Value.X > 0;
					case EPCGExSingleField::Y:
						return Value.Y > 0;
					case EPCGExSingleField::Z:
					case EPCGExSingleField::W:
						return Value.Z > 0;
					case EPCGExSingleField::Length:
					case EPCGExSingleField::SquaredLength:
						return Value.SquaredLength() > 0;
					case EPCGExSingleField::Volume:
						return (Value.X * Value.Y * Value.Z) > 0;
					case EPCGExSingleField::Sum:
						return (Value.X + Value.Y + Value.Z) > 0;
					}
				}
				else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
				{
					switch (Field)
					{
					default:
					case EPCGExSingleField::X:
						return Value.X;
					case EPCGExSingleField::Y:
						return Value.Y;
					case EPCGExSingleField::Z:
					case EPCGExSingleField::W:
						return Value.Z;
					case EPCGExSingleField::Length:
						return Value.Length();
					case EPCGExSingleField::SquaredLength:
						return Value.SquaredLength();
					case EPCGExSingleField::Volume:
						return Value.X * Value.Y * Value.Z;
					case EPCGExSingleField::Sum:
						return Value.X + Value.Y + Value.Z;
					}
				}
				else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value.X, Value.Y); }
				else if constexpr (std::is_same_v<T, FVector>) { return Value; }
				else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.X, Value.Y, Value.Z, 0); }
				else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value.X, Value.Y, Value.Z).Quaternion(); /* TODO : Handle axis selection */ }
				else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value.X, Value.Y, Value.Z); }
				else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; /* TODO : Handle axis selection */ }
				else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
				else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
				else { return T{}; }
			}

#pragma endregion

#pragma region Convert from FVector4

			else if constexpr (std::is_same_v<T_VALUE, FVector4>)
			{
				if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
				else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

				else if constexpr (std::is_same_v<T, bool>)
				{
					switch (Field)
					{
					default:
					case EPCGExSingleField::X:
						return Value.X > 0;
					case EPCGExSingleField::Y:
						return Value.Y > 0;
					case EPCGExSingleField::Z:
						return Value.Z > 0;
					case EPCGExSingleField::W:
						return Value.W > 0;
					case EPCGExSingleField::Length:
					case EPCGExSingleField::SquaredLength:
						return FVector(Value).SquaredLength() > 0;
					case EPCGExSingleField::Volume:
						return (Value.X * Value.Y * Value.Z * Value.W) > 0;
					case EPCGExSingleField::Sum:
						return (Value.X + Value.Y + Value.Z + Value.W) > 0;
					}
				}
				else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
				{
					switch (Field)
					{
					default:
					case EPCGExSingleField::X:
						return Value.X;
					case EPCGExSingleField::Y:
						return Value.Y;
					case EPCGExSingleField::Z:
						return Value.Z;
					case EPCGExSingleField::W:
						return Value.W;
					case EPCGExSingleField::Length:
						return FVector(Value).Length();
					case EPCGExSingleField::SquaredLength:
						return FVector(Value).SquaredLength();
					case EPCGExSingleField::Volume:
						return Value.X * Value.Y * Value.Z * Value.W;
					case EPCGExSingleField::Sum:
						return Value.X + Value.Y + Value.Z + Value.W;
					}
				}
				else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value.X, Value.Y); }
				else if constexpr (std::is_same_v<T, FVector>) { return Value; }
				else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.X, Value.Y, Value.Z, Value.W); }
				else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value.X, Value.Y, Value.Z).Quaternion(); }
				else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value.X, Value.Y, Value.Z); }
				else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; /* TODO : Handle axis */ }
				else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
				else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
				else { return T{}; }
			}

#pragma endregion

#pragma region Convert from FQuat

			else if constexpr (std::is_same_v<T_VALUE, FQuat>)
			{
				if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
				else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

				else if constexpr (std::is_same_v<T, bool>)
				{
					const FVector Dir = PCGExMath::GetDirection(Value, Axis);
					switch (Field)
					{
					default:
					case EPCGExSingleField::X:
						return Dir.X > 0;
					case EPCGExSingleField::Y:
						return Dir.Y > 0;
					case EPCGExSingleField::Z:
					case EPCGExSingleField::W:
						return Dir.Z > 0;
					case EPCGExSingleField::Length:
					case EPCGExSingleField::SquaredLength:
					case EPCGExSingleField::Volume:
					case EPCGExSingleField::Sum:
						return Dir.SquaredLength() > 0;
					}
				}
				else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
				{
					const FVector Dir = PCGExMath::GetDirection(Value, Axis);
					switch (Field)
					{
					default:
					case EPCGExSingleField::X:
						return Dir.X;
					case EPCGExSingleField::Y:
						return Dir.Y;
					case EPCGExSingleField::Z:
					case EPCGExSingleField::W:
						return Dir.Z;
					case EPCGExSingleField::Length:
						return Dir.Length();
					case EPCGExSingleField::SquaredLength:
					case EPCGExSingleField::Volume:
						return Dir.SquaredLength();
					case EPCGExSingleField::Sum:
						return Dir.X + Dir.Y + Dir.Z;
					}
				}
				else if constexpr (std::is_same_v<T, FVector2D>)
				{
					const FVector Dir = PCGExMath::GetDirection(Value, Axis);
					return FVector2D(Dir.X, Dir.Y);
				}
				else if constexpr (std::is_same_v<T, FVector>) { return PCGExMath::GetDirection(Value, Axis); }
				else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(PCGExMath::GetDirection(Value, Axis), 0); }
				else if constexpr (std::is_same_v<T, FQuat>) { return Value; }
				else if constexpr (std::is_same_v<T, FRotator>) { return Value.Rotator(); }
				else if constexpr (std::is_same_v<T, FTransform>) { return FTransform(Value, FVector::ZeroVector, FVector::OneVector); }
				else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
				else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
				else { return T{}; }
			}

#pragma endregion

#pragma region Convert from FRotator

			else if constexpr (std::is_same_v<T_VALUE, FRotator>)
			{
				if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
				else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(FVector(Value.Pitch, Value.Roll, Value.Yaw)); }

				else if constexpr (std::is_same_v<T, bool>)
				{
					switch (Field)
					{
					default:
					case EPCGExSingleField::X:
						return Value.Pitch > 0;
					case EPCGExSingleField::Y:
						return Value.Yaw > 0;
					case EPCGExSingleField::Z:
					case EPCGExSingleField::W:
						return Value.Roll > 0;
					case EPCGExSingleField::Length:
					case EPCGExSingleField::SquaredLength:
					case EPCGExSingleField::Volume:
					case EPCGExSingleField::Sum:
						return Value.Euler().SquaredLength() > 0;
					}
				}
				else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
				{
					switch (Field)
					{
					default:
					case EPCGExSingleField::X:
						return Value.Pitch > 0;
					case EPCGExSingleField::Y:
						return Value.Yaw > 0;
					case EPCGExSingleField::Z:
					case EPCGExSingleField::W:
						return Value.Roll > 0;
					case EPCGExSingleField::Length:
						return Value.Euler().Length();
					case EPCGExSingleField::SquaredLength:
					case EPCGExSingleField::Volume:
						return Value.Euler().SquaredLength();
					case EPCGExSingleField::Sum:
						return Value.Pitch + Value.Yaw + Value.Roll;
					}
				}
				else if constexpr (std::is_same_v<T, FVector2D>) { return Get<FVector2D>(Value.Quaternion()); }
				else if constexpr (std::is_same_v<T, FVector>) { return Get<FVector>(Value.Quaternion()); }
				else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.Euler(), 0); /* TODO : Handle axis */ }
				else if constexpr (std::is_same_v<T, FQuat>) { return Value.Quaternion(); }
				else if constexpr (std::is_same_v<T, FRotator>) { return Value; }
				else if constexpr (std::is_same_v<T, FTransform>) { return FTransform(Value.Quaternion(), FVector::ZeroVector, FVector::OneVector); }
				else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
				else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
				else { return T{}; }
			}

#pragma endregion

#pragma region Convert from FTransform

			else if constexpr (std::is_same_v<T_VALUE, FTransform>)
			{
				if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
				else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

				else if constexpr (
					std::is_same_v<T, bool> ||
					std::is_same_v<T, int32> ||
					std::is_same_v<T, int64> ||
					std::is_same_v<T, float> ||
					std::is_same_v<T, double> ||
					std::is_same_v<T, FVector2D> ||
					std::is_same_v<T, FVector> ||
					std::is_same_v<T, FVector4> ||
					std::is_same_v<T, FQuat> ||
					std::is_same_v<T, FRotator>)
				{
					switch (Component)
					{
					default:
					case EPCGExTransformComponent::Position: return Get<T>(Value.GetLocation());
					case EPCGExTransformComponent::Rotation: return Get<T>(Value.GetRotation());
					case EPCGExTransformComponent::Scale: return Get<T>(Value.GetScale3D());
					}
				}
				else if constexpr (std::is_same_v<T, FTransform>) { return Value; }
				else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
				else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
				else { return T{}; }
			}

#pragma endregion

#pragma region Convert from FString

			else if constexpr (std::is_same_v<T_VALUE, FString>)
			{
				if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
				else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

				else if constexpr (std::is_same_v<T, FString>) { return Value; }
				else if constexpr (std::is_same_v<T, FName>) { return FName(Value); }
				else if constexpr (std::is_same_v<T, FSoftClassPath>) { return FSoftClassPath(Value); }
				else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return FSoftObjectPath(Value); }
				else { return T{}; }
			}

#pragma endregion

#pragma region Convert from FName

			else if constexpr (std::is_same_v<T_VALUE, FName>)
			{
				if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
				else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

				else if constexpr (std::is_same_v<T, FString>) { return Value.ToString(); }
				else if constexpr (std::is_same_v<T, FName>) { return Value; }
				else if constexpr (std::is_same_v<T, FSoftClassPath>) { return FSoftClassPath(Value.ToString()); }
				else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return FSoftObjectPath(Value.ToString()); }
				else { return T{}; }
			}

#pragma endregion

#pragma region Convert from FSoftClassPath

			else if constexpr (std::is_same_v<T_VALUE, FSoftClassPath>)
			{
				if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
				else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

				else if constexpr (std::is_same_v<T, FString>) { return Value.ToString(); }
				else if constexpr (std::is_same_v<T, FName>) { return FName(Value.ToString()); }
				else if constexpr (std::is_same_v<T, FSoftClassPath>) { return Value; }
				else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return FSoftObjectPath(Value.ToString()); }
				else { return T{}; }
			}

#pragma endregion

#pragma region Convert from FSoftObjectPath

			else if constexpr (std::is_same_v<T_VALUE, FSoftObjectPath>)
			{
				if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
				else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

				else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
				else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
				else if constexpr (std::is_same_v<T, FSoftClassPath>) { return FSoftClassPath(Value.ToString()); }
				else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return Value; }
				else { return T{}; }
			}

#pragma endregion
			else { return T{}; }
		}

#define PCGEX_CONVERT_SELECT_DECL(_TYPE, _ID, ...) template <typename T> inline T Get(const _TYPE& Value) const { return Get<_TYPE, T>(Value); }
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_CONVERT_SELECT_DECL)
#undef PCGEX_CONVERT_SELECT_DECL

		// Set component subselection inside Target from provided value
		template <typename T, typename T_VALUE>
		void Set(T& Target, const T_VALUE& Value) const
		{
			// Unary target type -- can't account for component/field
			if constexpr (std::is_same_v<T_VALUE, T>) { Target = Value; }
			else if constexpr (
				std::is_same_v<T, bool> ||
				std::is_same_v<T, int32> ||
				std::is_same_v<T, int64> ||
				std::is_same_v<T, float> ||
				std::is_same_v<T, double>)
			{
				if constexpr (std::is_same_v<T_VALUE, PCGExTypeHash>) { Target = GetTypeHash(Value); }
				else if constexpr (std::is_same_v<T_VALUE, bool>) { Target = Value ? 1 : 0; }
				else if constexpr (
					std::is_same_v<T_VALUE, int32> ||
					std::is_same_v<T_VALUE, int64> ||
					std::is_same_v<T_VALUE, float> ||
					std::is_same_v<T_VALUE, double>)
				{
					Target = static_cast<T>(Value);
				}
				else if constexpr (
					std::is_same_v<T_VALUE, FVector2D> ||
					std::is_same_v<T_VALUE, FVector> ||
					std::is_same_v<T_VALUE, FVector4>)
				{
					Target = static_cast<T>(Value[0]);
				}
				else if constexpr (std::is_same_v<T_VALUE, FQuat>)
				{
					Target = static_cast<T>(Value.X);
				}
				else if constexpr (std::is_same_v<T_VALUE, FRotator>)
				{
					Target = static_cast<T>(Value.Pitch);
				}
				//else if constexpr (std::is_same_v<T_VALUE, FTransform>){ return; // UNSUPPORTED }
				//else if constexpr (std::is_same_v<T_VALUE, FString>){ return; // UNSUPPORTED }
				//else if constexpr (std::is_same_v<T_VALUE, FName>){ return; // UNSUPPORTED }
				else
				{
				}
			}
			// NAry target type -- can set components by index
			else if constexpr (
				std::is_same_v<T, FVector2D> ||
				std::is_same_v<T, FVector> ||
				std::is_same_v<T, FVector4> ||
				std::is_same_v<T, FRotator> ||
				std::is_same_v<T, FQuat>)
			{
				double V = 1;

				if constexpr (std::is_same_v<T_VALUE, PCGExTypeHash>) { V = GetTypeHash(Value); }
				else if constexpr (std::is_same_v<T_VALUE, bool>) { V = Value ? 1 : 0; }
				else if constexpr (
					std::is_same_v<T_VALUE, int32> ||
					std::is_same_v<T_VALUE, int64> ||
					std::is_same_v<T_VALUE, float> ||
					std::is_same_v<T_VALUE, double>)
				{
					V = static_cast<double>(Value);
				}
				else if constexpr (
					std::is_same_v<T_VALUE, FVector2D> ||
					std::is_same_v<T_VALUE, FVector> ||
					std::is_same_v<T_VALUE, FVector4>)
				{
					V = static_cast<double>(Value[0]);
				}
				else if constexpr (std::is_same_v<T_VALUE, FQuat>)
				{
					// TODO : Support setting rotations from axis?
					V = static_cast<double>(Value.X);
				}
				else if constexpr (std::is_same_v<T_VALUE, FRotator>)
				{
					// TODO : Support setting rotations from axis?
					V = static_cast<double>(Value.Pitch);
				}
				//else if constexpr (std::is_same_v<T_VALUE, FTransform>){ return; // UNSUPPORTED }
				//else if constexpr (std::is_same_v<T_VALUE, FString>){ return; // UNSUPPORTED }
				//else if constexpr (std::is_same_v<T_VALUE, FName>){ return; // UNSUPPORTED }
				else
				{
					// UNSUPPORTED
				}

				if constexpr (
					std::is_same_v<T, FVector2D> ||
					std::is_same_v<T, FVector> ||
					std::is_same_v<T, FVector4>)
				{
					switch (Field)
					{
					case EPCGExSingleField::X:
						Target[0] = V;
						break;
					case EPCGExSingleField::Y:
						Target[1] = V;
						break;
					case EPCGExSingleField::Z:
						if constexpr (!std::is_same_v<T, FVector2D>) { Target[2] = V; }
						break;
					case EPCGExSingleField::W:
						if constexpr (std::is_same_v<T, FVector4>) { Target[3] = V; }
						break;
					case EPCGExSingleField::Length:
						if constexpr (std::is_same_v<T, FVector4>) { Target = FVector4(FVector(Target.X, Target.Y, Target.Z).GetSafeNormal() * V, Target.W); }
						else { Target = Target.GetSafeNormal() * V; }
						break;
					case EPCGExSingleField::SquaredLength:
						if constexpr (std::is_same_v<T, FVector4>) { Target = FVector4(FVector(Target.X, Target.Y, Target.Z).GetSafeNormal() * FMath::Sqrt(V), Target.W); }
						else { Target = Target.GetSafeNormal() * FMath::Sqrt(V); }
						break;
					case EPCGExSingleField::Volume:
					case EPCGExSingleField::Sum:
						return;
					}
				}
				else if constexpr (std::is_same_v<T, FRotator>)
				{
					switch (Field)
					{
					case EPCGExSingleField::X:
						Target.Pitch = V;
						break;
					case EPCGExSingleField::Y:
						Target.Yaw = V;
						break;
					case EPCGExSingleField::Z:
						Target.Roll = V;
						break;
					case EPCGExSingleField::W:
						return;
					case EPCGExSingleField::Length:
						Target = Target.GetNormalized() * V;
						break;
					case EPCGExSingleField::SquaredLength:
						Target = Target.GetNormalized() * FMath::Sqrt(V);
						break;
					case EPCGExSingleField::Volume:
					case EPCGExSingleField::Sum:
						return;
					}
				}
				else if constexpr (std::is_same_v<T, FQuat>)
				{
					FRotator R = Target.Rotator();
					Set<FRotator, double>(R, V);
					Target = R.Quaternion();
				}
			}
			// Complex target type that require sub-component handling first
			else if constexpr (std::is_same_v<T, FTransform>)
			{
				if (Component == EPCGExTransformComponent::Position)
				{
					FVector Vector = Target.GetLocation();
					Set<FVector, T_VALUE>(Vector, Value);
					Target.SetLocation(Vector);
				}
				else if (Component == EPCGExTransformComponent::Scale)
				{
					FVector Vector = Target.GetScale3D();
					Set<FVector, T_VALUE>(Vector, Value);
					Target.SetScale3D(Vector);
				}
				else
				{
					FQuat Quat = Target.GetRotation();
					Set<FQuat, T_VALUE>(Quat, Value);
					Target.SetRotation(Quat);
				}
			}
			// Text target types
			else if constexpr (std::is_same_v<T, FString>)
			{
				if constexpr (std::is_same_v<T_VALUE, FString>) { Target = Value; }
				else if constexpr (std::is_same_v<T_VALUE, FName>) { Target = Value.ToString(); }
				else if constexpr (std::is_same_v<T_VALUE, FSoftClassPath>) { Target = Value.ToString(); }
				else if constexpr (std::is_same_v<T_VALUE, FSoftObjectPath>) { Target = Value.ToString(); }
				//else { Target = TEXT("NOT_SUPPORTED"); }
			}
			else if constexpr (std::is_same_v<T, FName>)
			{
				if constexpr (std::is_same_v<T_VALUE, FString>) { Target = FName(Value); }
				else if constexpr (std::is_same_v<T_VALUE, FName>) { Target = Value; }
				else if constexpr (std::is_same_v<T_VALUE, FSoftClassPath>) { Target = FName(Value.ToString()); }
				else if constexpr (std::is_same_v<T_VALUE, FSoftObjectPath>) { Target = FName(Value.ToString()); }
				//else { Target = NAME_None; }
			}
			else if constexpr (std::is_same_v<T, FSoftClassPath>)
			{
				if constexpr (std::is_same_v<T_VALUE, FString>) { Target = FSoftClassPath(Value); }
				else if constexpr (std::is_same_v<T_VALUE, FName>) { Target = FSoftClassPath(Value.ToString()); }
				else if constexpr (std::is_same_v<T_VALUE, FSoftClassPath>) { Target = Value; }
				else if constexpr (std::is_same_v<T_VALUE, FSoftObjectPath>) { Target = FSoftClassPath(Value.ToString()); }
				//else { Target = FSoftClassPath(); }
			}
			else if constexpr (std::is_same_v<T, FSoftObjectPath>)
			{
				if constexpr (std::is_same_v<T_VALUE, FString>) { Target = FSoftObjectPath(Value); }
				else if constexpr (std::is_same_v<T_VALUE, FName>) { Target = FSoftObjectPath(Value.ToString()); }
				else if constexpr (std::is_same_v<T_VALUE, FSoftClassPath>) { Target = FSoftObjectPath(Value.ToString()); }
				else if constexpr (std::is_same_v<T_VALUE, FSoftObjectPath>) { Target = Value; }
				//else { Target = FSoftObjectPath; }
			}
		}

#define PCGEX_CONVERT_SET_DECL(_TYPE, _ID, ...) template <typename T> inline void Set(_TYPE& Target, const T& Value) const { return Set<_TYPE, T>(Target, Value); }
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_CONVERT_SET_DECL)
#undef PCGEX_CONVERT_SET_DECL
	};

	class FValueBuffer : public TSharedFromThis<FValueBuffer>
	{
	public:
		FValueBuffer() = default;
	};

	template <typename T>
	class TValueBuffer : public FValueBuffer
	{
	public:
		TSharedPtr<TArray<T>> Values;
		TValueBuffer() = default;

		template <typename T_VALUE>
		void Set(const FSubSelection& SubSelection, const int32 Index, const T_VALUE& Value)
		{
			*(Values->GetData() + Index) = SubSelection.Get<T>(Value);
		}

		template <typename T_VALUE>
		T_VALUE Get(const FSubSelection& SubSelection, const int32 Index) { return SubSelection.Get<T_VALUE, T>(*(Values->GetData() + Index)); }
	};

	class FValueBufferMap : public TSharedFromThis<FValueBufferMap>
	{
	public:
		TMap<FString, TSharedPtr<FValueBuffer>> BufferMap;
		FValueBufferMap() = default;

		//TSharedPtr<FValueBuffer> GetBuffer(FString BufferID);
	};

	// Prioritize originally specified source
	PCGEXTENDEDTOOLKIT_API
	bool TryGetTypeAndSource(
		const FPCGAttributePropertyInputSelector& InputSelector,
		const TSharedPtr<PCGExData::FFacade>& InDataFacade,
		EPCGMetadataTypes& OutType, PCGExData::EIOSide& InOutSide);

	PCGEXTENDEDTOOLKIT_API
	bool TryGetTypeAndSource(
		const FName AttributeName,
		const TSharedPtr<PCGExData::FFacade>& InDataFacade,
		EPCGMetadataTypes& OutType, PCGExData::EIOSide& InOutSource);

#define PCGEX_FOREACH_SUPPORTEDFPROPERTY(MACRO)\
MACRO(FBoolProperty, bool) \
MACRO(FIntProperty, int32) \
MACRO(FInt64Property, int64) \
MACRO(FFloatProperty, float) \
MACRO(FDoubleProperty, double) \
MACRO(FStrProperty, FString) \
MACRO(FNameProperty, FName)
	//MACRO(FSoftClassProperty, FSoftClassPath) 
	//MACRO(FSoftObjectProperty, FSoftObjectPath)

#define PCGEX_FOREACH_SUPPORTEDFSTRUCT(MACRO)\
MACRO(FStructProperty, FVector2D) \
MACRO(FStructProperty, FVector) \
MACRO(FStructProperty, FVector4) \
MACRO(FStructProperty, FQuat) \
MACRO(FStructProperty, FRotator) \
MACRO(FStructProperty, FTransform)

	template <typename T>
	static bool TrySetFPropertyValue(void* InContainer, FProperty* InProperty, T InValue)
	{
		FSubSelection S = FSubSelection();
		if (InProperty->IsA<FObjectPropertyBase>())
		{
			// If input type is a soft object path, check if the target property is an object type
			// and resolve the object path
			const FSoftObjectPath Path = S.Get<FSoftObjectPath>(InValue);

			if (UObject* ResolvedObject = Path.TryLoad())
			{
				FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(InProperty);
				if (ResolvedObject->IsA(ObjectProperty->PropertyClass))
				{
					void* PropertyContainer = ObjectProperty->ContainerPtrToValuePtr<void>(InContainer);
					ObjectProperty->SetObjectPropertyValue(PropertyContainer, ResolvedObject);
					return true;
				}
			}
		}

#define PCGEX_TRY_SET_FPROPERTY(_PTYPE, _TYPE) if(InProperty->IsA<_PTYPE>()){ _PTYPE* P = CastField<_PTYPE>(InProperty); P->SetPropertyValue_InContainer(InContainer, S.Get<_TYPE>(InValue)); return true;}
		PCGEX_FOREACH_SUPPORTEDFPROPERTY(PCGEX_TRY_SET_FPROPERTY)
#undef PCGEX_TRY_SET_FPROPERTY

		if (FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
		{
#define PCGEX_TRY_SET_FSTRUCT(_PTYPE, _TYPE) if (StructProperty->Struct == TBaseStructure<_TYPE>::Get()){ void* StructContainer = StructProperty->ContainerPtrToValuePtr<void>(InContainer); *reinterpret_cast<_TYPE*>(StructContainer) = S.Get<_TYPE>(InValue); return true; }
			PCGEX_FOREACH_SUPPORTEDFSTRUCT(PCGEX_TRY_SET_FSTRUCT)
#undef PCGEX_TRY_SET_FSTRUCT

			if (StructProperty->Struct == TBaseStructure<FPCGAttributePropertyInputSelector>::Get())
			{
				FPCGAttributePropertyInputSelector NewSelector = FPCGAttributePropertyInputSelector();
				NewSelector.Update(S.Get<FString>(InValue));
				void* StructContainer = StructProperty->ContainerPtrToValuePtr<void>(InContainer);
				*reinterpret_cast<FPCGAttributePropertyInputSelector*>(StructContainer) = NewSelector;
				return true;
			}
		}

		return false;
	}
}
