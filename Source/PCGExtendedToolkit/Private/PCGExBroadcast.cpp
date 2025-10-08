// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExBroadcast.h"

#include "CoreMinimal.h"
#include "PCGExMath.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"

namespace PCGEx
{
#pragma region conversions

	template <typename T>
	T ConvertFromBoolean(const bool& Value)
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

	template <typename T>
	T ConvertFromInteger32(const int32& Value)
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

	template <typename T>
	T ConvertFromInteger64(const int64& Value)
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

	template <typename T>
	T ConvertFromFloat(const float& Value)
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

	template <typename T>
	T ConvertFromDouble(const double& Value)
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

	template <typename T>
	T ConvertFromVector2(const FVector2D& Value)
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

	template <typename T>
	T ConvertFromVector(const FVector& Value)
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

	template <typename T>
	T ConvertFromVector4(const FVector4& Value)
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

	template <typename T>
	T ConvertFromQuaternion(const FQuat& Value)
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

	template <typename T>
	T ConvertFromRotator(const FRotator& Value)
	{
		if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
		else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(FVector(Value.Pitch, Value.Roll, Value.Yaw)); }

		else if constexpr (std::is_same_v<T, bool>) { return Value.Euler().SquaredLength() > 0; }
		else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return static_cast<T>(Value.Pitch); }
		else if constexpr (std::is_same_v<T, FVector2D>) { return ConvertFromQuaternion<FVector2D>(Value.Quaternion()); }
		else if constexpr (std::is_same_v<T, FVector>) { return ConvertFromQuaternion<FVector>(Value.Quaternion()); }
		else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.Euler(), 0); /* TODO : Handle axis */ }
		else if constexpr (std::is_same_v<T, FQuat>) { return Value.Quaternion(); }
		else if constexpr (std::is_same_v<T, FRotator>) { return Value; }
		else if constexpr (std::is_same_v<T, FTransform>) { return FTransform(Value.Quaternion(), FVector::ZeroVector, FVector::OneVector); }
		else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
		else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
		else { return T{}; }
	}

	template <typename T>
	T ConvertFromTransform(const FTransform& Value)
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
			return ConvertFromVector<T>(Value.GetLocation());
		}
		else if constexpr (std::is_same_v<T, FTransform>) { return Value; }
		else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
		else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
		else { return T{}; }
	}

	template <typename T>
	T ConvertFromString(const FString& Value)
	{
		if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
		else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

		else if constexpr (std::is_same_v<T, FString>) { return Value; }
		else if constexpr (std::is_same_v<T, FName>) { return FName(Value); }
		else if constexpr (std::is_same_v<T, FSoftClassPath>) { return FSoftClassPath(Value); }
		else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return FSoftObjectPath(Value); }
		else { return T{}; }
	}

	template <typename T>
	T ConvertFromName(const FName& Value)
	{
		if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
		else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

		else if constexpr (std::is_same_v<T, FString>) { return Value.ToString(); }
		else if constexpr (std::is_same_v<T, FName>) { return Value; }
		else if constexpr (std::is_same_v<T, FSoftClassPath>) { return FSoftClassPath(Value.ToString()); }
		else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return FSoftObjectPath(Value.ToString()); }
		else { return T{}; }
	}

	template <typename T>
	T ConvertFromSoftClassPath(const FSoftClassPath& Value)
	{
		if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
		else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

		else if constexpr (std::is_same_v<T, FString>) { return Value.ToString(); }
		else if constexpr (std::is_same_v<T, FName>) { return FName(Value.ToString()); }
		else if constexpr (std::is_same_v<T, FSoftClassPath>) { return Value; }
		else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return FSoftObjectPath(Value.ToString()); }
		else { return T{}; }
	}

	template <typename T>
	T ConvertFromSoftObjectPath(const FSoftObjectPath& Value)
	{
		if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
		else if constexpr (std::is_same_v<T, PCGExTypeHash>) { return GetTypeHash(Value); }

		else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
		else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
		else if constexpr (std::is_same_v<T, FSoftClassPath>) { return FSoftClassPath(Value.ToString()); }
		else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return Value; }
		else { return T{}; }
	}

#define PCGEX_TPL(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) \
template PCGEXTENDEDTOOLKIT_API _TYPE_B ConvertFrom##_NAME_A<_TYPE_B>(const _TYPE_A& Value);
	PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_TPL)
#undef PCGEX_TPL

	template <typename T_VALUE, typename T>
	T Convert(const T_VALUE& Value)
	{
		if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
		else if constexpr (std::is_same_v<T_VALUE, bool>) { return ConvertFromBoolean<T>(Value); }
		else if constexpr (std::is_same_v<T_VALUE, int32>) { return ConvertFromInteger32<T>(Value); }
		else if constexpr (std::is_same_v<T_VALUE, int64>) { return ConvertFromInteger64<T>(Value); }
		else if constexpr (std::is_same_v<T_VALUE, float>) { return ConvertFromFloat<T>(Value); }
		else if constexpr (std::is_same_v<T_VALUE, double>) { return ConvertFromDouble<T>(Value); }
		else if constexpr (std::is_same_v<T_VALUE, FVector2D>) { return ConvertFromVector2<T>(Value); }
		else if constexpr (std::is_same_v<T_VALUE, FVector>) { return ConvertFromVector<T>(Value); }
		else if constexpr (std::is_same_v<T_VALUE, FVector4>) { return ConvertFromVector4<T>(Value); }
		else if constexpr (std::is_same_v<T_VALUE, FQuat>) { return ConvertFromQuaternion<T>(Value); }
		else if constexpr (std::is_same_v<T_VALUE, FRotator>) { return ConvertFromRotator<T>(Value); }
		else if constexpr (std::is_same_v<T_VALUE, FTransform>) { return ConvertFromTransform<T>(Value); }
		else if constexpr (std::is_same_v<T_VALUE, FString>) { return ConvertFromString<T>(Value); }
		else if constexpr (std::is_same_v<T_VALUE, FName>) { return ConvertFromName<T>(Value); }
		else if constexpr (std::is_same_v<T_VALUE, FSoftClassPath>) { return ConvertFromSoftClassPath<T>(Value); }
		else if constexpr (std::is_same_v<T_VALUE, FSoftObjectPath>) { return ConvertFromSoftObjectPath<T>(Value); }
		else { return T{}; }
	}

#define PCGEX_TPL(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) \
	template PCGEXTENDEDTOOLKIT_API _TYPE_B Convert<_TYPE_A, _TYPE_B>(const _TYPE_A& Value);
	PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	bool GetComponentSelection(const TArray<FString>& Names, FInputSelectorComponentData& OutSelection)
	{
		if (Names.IsEmpty()) { return false; }
		for (const FString& Name : Names)
		{
			if (const FInputSelectorComponentData* Selection = STRMAP_TRANSFORM_FIELD.Find(Name.ToUpper()))
			{
				OutSelection = *Selection;
				return true;
			}
		}
		return false;
	}

	bool GetFieldSelection(const TArray<FString>& Names, FInputSelectorFieldData& OutSelection)
	{
		if (Names.IsEmpty()) { return false; }
		const FString& STR = Names.Num() > 1 ? Names[1].ToUpper() : Names[0].ToUpper();
		if (const FInputSelectorFieldData* Selection = STRMAP_SINGLE_FIELD.Find(STR))
		{
			OutSelection = *Selection;
			return true;
		}
		return false;
	}

	bool GetAxisSelection(const TArray<FString>& Names, FInputSelectorAxisData& OutSelection)
	{
		if (Names.IsEmpty()) { return false; }
		for (const FString& Name : Names)
		{
			if (const FInputSelectorAxisData* Selection = STRMAP_AXIS.Find(Name.ToUpper()))
			{
				OutSelection = *Selection;
				return true;
			}
		}
		return false;
	}

	FSubSelection::FSubSelection(const TArray<FString>& ExtraNames)
	{
		Init(ExtraNames);
	}

	FSubSelection::FSubSelection(const FPCGAttributePropertyInputSelector& InSelector)
	{
		Init(InSelector.GetExtraNames());
	}

	FSubSelection::FSubSelection(const FString& Path, const UPCGData* InData)
	{
		FPCGAttributePropertyInputSelector ProxySelector = FPCGAttributePropertyInputSelector();
		ProxySelector.Update(Path);
		if (InData) { ProxySelector = ProxySelector.CopyAndFixLast(InData); }

		Init(ProxySelector.GetExtraNames());
	}

	EPCGMetadataTypes FSubSelection::GetSubType(const EPCGMetadataTypes Fallback) const
	{
		if (!bIsValid) { return Fallback; }
		if (bIsFieldSet) { return EPCGMetadataTypes::Double; }
		if (bIsAxisSet) { return EPCGMetadataTypes::Vector; }

		switch (Component)
		{
		case ETransformPart::Position:
		case ETransformPart::Scale:
			return EPCGMetadataTypes::Vector;
		case ETransformPart::Rotation:
			return EPCGMetadataTypes::Quaternion;
		}

		return Fallback;
	}

	void FSubSelection::SetComponent(const ETransformPart InComponent)
	{
		bIsValid = true;
		bIsComponentSet = true;
		Component = InComponent;
	}

	bool FSubSelection::SetFieldIndex(const int32 InFieldIndex)
	{
		FieldIndex = InFieldIndex;

		if (FieldIndex < 0 || FieldIndex > 3)
		{
			bIsFieldSet = false;
			return false;
		}

		bIsValid = true;
		bIsFieldSet = true;

		if (InFieldIndex == 0) { Field = ESingleField::X; }
		else if (InFieldIndex == 1) { Field = ESingleField::Y; }
		else if (InFieldIndex == 2) { Field = ESingleField::Z; }
		else if (InFieldIndex == 3) { Field = ESingleField::W; }

		return true;
	}

	void FSubSelection::Init(const TArray<FString>& ExtraNames)
	{
		if (ExtraNames.IsEmpty())
		{
			bIsValid = false;
			return;
		}

		FInputSelectorAxisData AxisIDMapping = FInputSelectorAxisData{EPCGExAxis::Forward, EPCGMetadataTypes::Unknown};
		FInputSelectorComponentData ComponentIDMapping = {ETransformPart::Rotation, EPCGMetadataTypes::Quaternion};
		FInputSelectorFieldData FieldIDMapping = {ESingleField::X, EPCGMetadataTypes::Unknown, 0};

		bIsAxisSet = GetAxisSelection(ExtraNames, AxisIDMapping);
		Axis = AxisIDMapping.Get<0>();

		bIsComponentSet = GetComponentSelection(ExtraNames, ComponentIDMapping);

		if (bIsAxisSet)
		{
			bIsValid = true;
			bIsComponentSet = GetComponentSelection(ExtraNames, ComponentIDMapping);
		}
		else
		{
			bIsValid = bIsComponentSet;
		}

		Component = ComponentIDMapping.Get<0>();
		PossibleSourceType = ComponentIDMapping.Get<1>();

		bIsFieldSet = GetFieldSelection(ExtraNames, FieldIDMapping);
		Field = FieldIDMapping.Get<0>();
		FieldIndex = FieldIDMapping.Get<2>();

		if (bIsFieldSet)
		{
			bIsValid = true;
			if (!bIsComponentSet) { PossibleSourceType = FieldIDMapping.Get<1>(); }
		}

		Update();
	}

	void FSubSelection::Update()
	{
		switch (Field)
		{
		case ESingleField::X:
			FieldIndex = 0;
			break;
		case ESingleField::Y:
			FieldIndex = 1;
			break;
		case ESingleField::Z:
			FieldIndex = 2;
			break;
		case ESingleField::W:
			FieldIndex = 3;
			break;
		case ESingleField::Length:
		case ESingleField::SquaredLength:
		case ESingleField::Volume:
		case ESingleField::Sum:
			FieldIndex = 0;
			break;
		}
	}

	template <typename T_VALUE, typename T>
	T FSubSelection::Get(const T_VALUE& Value) const
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
				case ESingleField::X:
					return Value.X > 0;
				case ESingleField::Y:
				case ESingleField::Z:
				case ESingleField::W:
					return Value.Y > 0;
				case ESingleField::Length:
				case ESingleField::SquaredLength:
					return Value.SquaredLength() > 0;
				case ESingleField::Volume:
					return (Value.X * Value.Y) > 0;
				case ESingleField::Sum:
					return (Value.X * Value.Y) > 0;
				}
			}
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				switch (Field)
				{
				default:
				case ESingleField::X:
					return Value.X;
				case ESingleField::Y:
				case ESingleField::Z:
				case ESingleField::W:
					return Value.Y;
				case ESingleField::Length:
					return Value.Length();
				case ESingleField::SquaredLength:
					return Value.SquaredLength();
				case ESingleField::Volume:
					return Value.X * Value.Y;
				case ESingleField::Sum:
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
				case ESingleField::X:
					return Value.X > 0;
				case ESingleField::Y:
					return Value.Y > 0;
				case ESingleField::Z:
				case ESingleField::W:
					return Value.Z > 0;
				case ESingleField::Length:
				case ESingleField::SquaredLength:
					return Value.SquaredLength() > 0;
				case ESingleField::Volume:
					return (Value.X * Value.Y * Value.Z) > 0;
				case ESingleField::Sum:
					return (Value.X + Value.Y + Value.Z) > 0;
				}
			}
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				switch (Field)
				{
				default:
				case ESingleField::X:
					return Value.X;
				case ESingleField::Y:
					return Value.Y;
				case ESingleField::Z:
				case ESingleField::W:
					return Value.Z;
				case ESingleField::Length:
					return Value.Length();
				case ESingleField::SquaredLength:
					return Value.SquaredLength();
				case ESingleField::Volume:
					return Value.X * Value.Y * Value.Z;
				case ESingleField::Sum:
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
				case ESingleField::X:
					return Value.X > 0;
				case ESingleField::Y:
					return Value.Y > 0;
				case ESingleField::Z:
					return Value.Z > 0;
				case ESingleField::W:
					return Value.W > 0;
				case ESingleField::Length:
				case ESingleField::SquaredLength:
					return FVector(Value).SquaredLength() > 0;
				case ESingleField::Volume:
					return (Value.X * Value.Y * Value.Z * Value.W) > 0;
				case ESingleField::Sum:
					return (Value.X + Value.Y + Value.Z + Value.W) > 0;
				}
			}
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				switch (Field)
				{
				default:
				case ESingleField::X:
					return Value.X;
				case ESingleField::Y:
					return Value.Y;
				case ESingleField::Z:
					return Value.Z;
				case ESingleField::W:
					return Value.W;
				case ESingleField::Length:
					return FVector(Value).Length();
				case ESingleField::SquaredLength:
					return FVector(Value).SquaredLength();
				case ESingleField::Volume:
					return Value.X * Value.Y * Value.Z * Value.W;
				case ESingleField::Sum:
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
				case ESingleField::X:
					return Dir.X > 0;
				case ESingleField::Y:
					return Dir.Y > 0;
				case ESingleField::Z:
				case ESingleField::W:
					return Dir.Z > 0;
				case ESingleField::Length:
				case ESingleField::SquaredLength:
				case ESingleField::Volume:
				case ESingleField::Sum:
					return Dir.SquaredLength() > 0;
				}
			}
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				const FVector Dir = PCGExMath::GetDirection(Value, Axis);
				switch (Field)
				{
				default:
				case ESingleField::X:
					return Dir.X;
				case ESingleField::Y:
					return Dir.Y;
				case ESingleField::Z:
				case ESingleField::W:
					return Dir.Z;
				case ESingleField::Length:
					return Dir.Length();
				case ESingleField::SquaredLength:
				case ESingleField::Volume:
					return Dir.SquaredLength();
				case ESingleField::Sum:
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
				case ESingleField::X:
					return Value.Pitch > 0;
				case ESingleField::Y:
					return Value.Yaw > 0;
				case ESingleField::Z:
				case ESingleField::W:
					return Value.Roll > 0;
				case ESingleField::Length:
				case ESingleField::SquaredLength:
				case ESingleField::Volume:
				case ESingleField::Sum:
					return Value.Euler().SquaredLength() > 0;
				}
			}
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				switch (Field)
				{
				default:
				case ESingleField::X:
					return Value.Pitch > 0;
				case ESingleField::Y:
					return Value.Yaw > 0;
				case ESingleField::Z:
				case ESingleField::W:
					return Value.Roll > 0;
				case ESingleField::Length:
					return Value.Euler().Length();
				case ESingleField::SquaredLength:
				case ESingleField::Volume:
					return Value.Euler().SquaredLength();
				case ESingleField::Sum:
					return Value.Pitch + Value.Yaw + Value.Roll;
				}
			}
			else if constexpr (std::is_same_v<T, FVector2D>) { return Get<FQuat, FVector2D>(Value.Quaternion()); }
			else if constexpr (std::is_same_v<T, FVector>) { return Get<FQuat, FVector>(Value.Quaternion()); }
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
				case ETransformPart::Position: return Get<FVector, T>(Value.GetLocation());
				case ETransformPart::Rotation: return Get<FQuat, T>(Value.GetRotation());
				case ETransformPart::Scale: return Get<FVector, T>(Value.GetScale3D());
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

	template <typename T, typename T_VALUE>
	void FSubSelection::Set(T& Target, const T_VALUE& Value) const
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
				case ESingleField::X:
					Target[0] = V;
					break;
				case ESingleField::Y:
					Target[1] = V;
					break;
				case ESingleField::Z:
					if constexpr (!std::is_same_v<T, FVector2D>) { Target[2] = V; }
					break;
				case ESingleField::W:
					if constexpr (std::is_same_v<T, FVector4>) { Target[3] = V; }
					break;
				case ESingleField::Length:
					if constexpr (std::is_same_v<T, FVector4>) { Target = FVector4(FVector(Target.X, Target.Y, Target.Z).GetSafeNormal() * V, Target.W); }
					else { Target = Target.GetSafeNormal() * V; }
					break;
				case ESingleField::SquaredLength:
					if constexpr (std::is_same_v<T, FVector4>) { Target = FVector4(FVector(Target.X, Target.Y, Target.Z).GetSafeNormal() * FMath::Sqrt(V), Target.W); }
					else { Target = Target.GetSafeNormal() * FMath::Sqrt(V); }
					break;
				case ESingleField::Volume:
				case ESingleField::Sum:
					return;
				}
			}
			else if constexpr (std::is_same_v<T, FRotator>)
			{
				switch (Field)
				{
				case ESingleField::X:
					Target.Pitch = V;
					break;
				case ESingleField::Y:
					Target.Yaw = V;
					break;
				case ESingleField::Z:
					Target.Roll = V;
					break;
				case ESingleField::W:
					return;
				case ESingleField::Length:
					Target = Target.GetNormalized() * V;
					break;
				case ESingleField::SquaredLength:
					Target = Target.GetNormalized() * FMath::Sqrt(V);
					break;
				case ESingleField::Volume:
				case ESingleField::Sum:
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
			if (Component == ETransformPart::Position)
			{
				FVector Vector = Target.GetLocation();
				Set<FVector, T_VALUE>(Vector, Value);
				Target.SetLocation(Vector);
			}
			else if (Component == ETransformPart::Scale)
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

#define PCGEX_TPL(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) \
template PCGEXTENDEDTOOLKIT_API _TYPE_A FSubSelection::Get<_TYPE_B, _TYPE_A>(const _TYPE_B& Value) const; \
template PCGEXTENDEDTOOLKIT_API void FSubSelection::Set<_TYPE_A, _TYPE_B>(_TYPE_A& Target, const _TYPE_B& Value) const;
	PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_TPL)
#undef PCGEX_TPL

	bool TryGetTypeAndSource(
		const FPCGAttributePropertyInputSelector& InputSelector,
		const TSharedPtr<PCGExData::FFacade>& InDataFacade,
		EPCGMetadataTypes& OutType, PCGExData::EIOSide& InOutSide)
	{
		OutType = EPCGMetadataTypes::Unknown;
		const UPCGBasePointData* Data = InOutSide == PCGExData::EIOSide::In ?
			                                InDataFacade->Source->GetInOut(InOutSide) :
			                                InDataFacade->Source->GetOutIn(InOutSide);

		if (!IsValid(Data)) { return false; }

		const FPCGAttributePropertyInputSelector FixedSelector = InputSelector.CopyAndFixLast(Data);
		if (!FixedSelector.IsValid()) { return false; }

		if (FixedSelector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			// TODO : Try the other way around if we don't find it at first 
			if (!Data->Metadata) { return false; }
			if (const FPCGMetadataAttributeBase* AttributeBase = Data->Metadata->GetConstAttribute(GetAttributeIdentifier(FixedSelector, Data)))
			{
				OutType = static_cast<EPCGMetadataTypes>(AttributeBase->GetTypeId());
			}
			else if (const UPCGBasePointData* OutData = InDataFacade->Source->GetOut(); OutData && InOutSide == PCGExData::EIOSide::In)
			{
				// Failed to find attribute on input, try to find it on output if there is one
				if (const FPCGMetadataAttributeBase* OutAttributeBase = OutData->Metadata->GetConstAttribute(GetAttributeIdentifier(FixedSelector, OutData)))
				{
					OutType = static_cast<EPCGMetadataTypes>(OutAttributeBase->GetTypeId());
					InOutSide = PCGExData::EIOSide::Out;
				}
			}
			else if (const UPCGBasePointData* InData = InDataFacade->Source->GetIn(); InData && InOutSide == PCGExData::EIOSide::Out)
			{
				// Failed to find attribute on input, try to find it on output if there is one
				if (const FPCGMetadataAttributeBase* InAttributeBase = InData->Metadata->GetConstAttribute(GetAttributeIdentifier(FixedSelector, InData)))
				{
					OutType = static_cast<EPCGMetadataTypes>(InAttributeBase->GetTypeId());
					InOutSide = PCGExData::EIOSide::In;
				}
			}
		}
		else if (FixedSelector.GetSelection() == EPCGAttributePropertySelection::ExtraProperty)
		{
			OutType = GetPropertyType(FixedSelector.GetExtraProperty());
		}
		else if (FixedSelector.GetSelection() == EPCGAttributePropertySelection::Property)
		{
			OutType = GetPropertyType(FixedSelector.GetPointProperty());
		}

		return OutType != EPCGMetadataTypes::Unknown;
	}
}
