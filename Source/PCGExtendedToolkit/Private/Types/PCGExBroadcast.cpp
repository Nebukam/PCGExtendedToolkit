// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Types/PCGExBroadcast.h"

#include "CoreMinimal.h"
#include "PCGExMath.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"

namespace PCGEx
{
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
		case ETransformPart::Scale: return EPCGMetadataTypes::Vector;
		case ETransformPart::Rotation: return EPCGMetadataTypes::Quaternion;
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
		case ESingleField::X: FieldIndex = 0;
			break;
		case ESingleField::Y: FieldIndex = 1;
			break;
		case ESingleField::Z: FieldIndex = 2;
			break;
		case ESingleField::W: FieldIndex = 3;
			break;
		case ESingleField::Length:
		case ESingleField::SquaredLength:
		case ESingleField::Volume:
		case ESingleField::Sum: FieldIndex = 0;
			break;
		}
	}

	//
	// Type-erased GetVoid implementation
	// Dispatches to templated Get<T_VALUE, T> based on runtime types
	//
	void FSubSelection::GetVoid(EPCGMetadataTypes SourceType, const void* Source,
	                            EPCGMetadataTypes WorkingType, void* Target) const
	{
		// Use PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS to generate all combinations
		// The macro provides (_TYPE_A, _NAME_A, _TYPE_B, _NAME_B)
		// We want: Get<SourceType, WorkingType>(Source) -> Target
		// So _TYPE_A = Working (output), _TYPE_B = Source (input)
		
#define PCGEX_DISPATCH_GET(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) \
		if (SourceType == EPCGMetadataTypes::_NAME_B && WorkingType == EPCGMetadataTypes::_NAME_A) \
		{ \
			*static_cast<_TYPE_A*>(Target) = Get<_TYPE_B, _TYPE_A>(*static_cast<const _TYPE_B*>(Source)); \
			return; \
		}
		PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_DISPATCH_GET)
#undef PCGEX_DISPATCH_GET
	}

	//
	// Type-erased SetVoid implementation
	// Dispatches to templated Set<T, T_VALUE> based on runtime types
	//
	void FSubSelection::SetVoid(EPCGMetadataTypes TargetType, void* Target,
	                            EPCGMetadataTypes SourceType, const void* Source) const
	{
		// Use PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS to generate all combinations
		// The macro provides (_TYPE_A, _NAME_A, _TYPE_B, _NAME_B)
		// We want: Set<TargetType, SourceType>(Target, Source)
		// So _TYPE_A = Target, _TYPE_B = Source
		
#define PCGEX_DISPATCH_SET(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) \
		if (TargetType == EPCGMetadataTypes::_NAME_A && SourceType == EPCGMetadataTypes::_NAME_B) \
		{ \
			Set<_TYPE_A, _TYPE_B>(*static_cast<_TYPE_A*>(Target), *static_cast<const _TYPE_B*>(Source)); \
			return; \
		}
		PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_DISPATCH_SET)
#undef PCGEX_DISPATCH_SET
	}

	//
	// Original templated Get implementation
	//
	template <typename T_VALUE, typename T>
	T FSubSelection::Get(const T_VALUE& Value) const
	{
#pragma region Convert from bool

		if constexpr (std::is_same_v<T_VALUE, bool>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExValueHash>) { return GetTypeHash(Value); }

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

#pragma region Convert from numeric (int32, int64, float, double)

		else if constexpr (std::is_same_v<T_VALUE, int32> || std::is_same_v<T_VALUE, int64> || std::is_same_v<T_VALUE, float> || std::is_same_v<T_VALUE, double>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExValueHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return Value > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return static_cast<T>(Value); }
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value); }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value, Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value, Value, Value).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform(FRotator(Value, Value, Value), FVector(Value), FVector(Value)); }
			else if constexpr (std::is_same_v<T, FString>) { return FString::Printf(TEXT("%f"), static_cast<double>(Value)); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(FString::Printf(TEXT("%f"), static_cast<double>(Value))); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FVector2D

		else if constexpr (std::is_same_v<T_VALUE, FVector2D>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExValueHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return Value.X > 0 || Value.Y > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				switch (Field)
				{
				default:
				case ESingleField::X: return static_cast<T>(Value.X);
				case ESingleField::Y: return static_cast<T>(Value.Y);
				case ESingleField::Z:
				case ESingleField::W: return static_cast<T>(0);
				case ESingleField::Length: return static_cast<T>(Value.Length());
				case ESingleField::SquaredLength: return static_cast<T>(Value.SquaredLength());
				case ESingleField::Volume: return static_cast<T>(Value.X * Value.Y);
				case ESingleField::Sum: return static_cast<T>(Value.X + Value.Y);
				}
			}
			else if constexpr (std::is_same_v<T, FVector2D>) { return Value; }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value.X, Value.Y, 0); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.X, Value.Y, 0, 0); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value.X, Value.Y, 0).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value.X, Value.Y, 0); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform(FRotator::ZeroRotator, FVector(Value.X, Value.Y, 0), FVector::OneVector); }
			else if constexpr (std::is_same_v<T, FString>) { return Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FVector

		else if constexpr (std::is_same_v<T_VALUE, FVector>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExValueHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return Value.X > 0 || Value.Y > 0 || Value.Z > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				switch (Field)
				{
				default:
				case ESingleField::X: return static_cast<T>(Value.X);
				case ESingleField::Y: return static_cast<T>(Value.Y);
				case ESingleField::Z: return static_cast<T>(Value.Z);
				case ESingleField::W: return static_cast<T>(0);
				case ESingleField::Length: return static_cast<T>(Value.Length());
				case ESingleField::SquaredLength: return static_cast<T>(Value.SquaredLength());
				case ESingleField::Volume: return static_cast<T>(Value.X * Value.Y * Value.Z);
				case ESingleField::Sum: return static_cast<T>(Value.X + Value.Y + Value.Z);
				}
			}
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value.X, Value.Y); }
			else if constexpr (std::is_same_v<T, FVector>) { return Value; }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.X, Value.Y, Value.Z, 0); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value.X, Value.Y, Value.Z).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value.X, Value.Y, Value.Z); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform(FRotator::ZeroRotator, Value, FVector::OneVector); }
			else if constexpr (std::is_same_v<T, FString>) { return Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FVector4

		else if constexpr (std::is_same_v<T_VALUE, FVector4>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExValueHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return Value.X > 0 || Value.Y > 0 || Value.Z > 0 || Value.W > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				switch (Field)
				{
				default:
				case ESingleField::X: return static_cast<T>(Value.X);
				case ESingleField::Y: return static_cast<T>(Value.Y);
				case ESingleField::Z: return static_cast<T>(Value.Z);
				case ESingleField::W: return static_cast<T>(Value.W);
				case ESingleField::Length: return static_cast<T>(FVector(Value.X, Value.Y, Value.Z).Length());
				case ESingleField::SquaredLength: return static_cast<T>(FVector(Value.X, Value.Y, Value.Z).SquaredLength());
				case ESingleField::Volume: return static_cast<T>(Value.X * Value.Y * Value.Z * Value.W);
				case ESingleField::Sum: return static_cast<T>(Value.X + Value.Y + Value.Z + Value.W);
				}
			}
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value.X, Value.Y); }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value.X, Value.Y, Value.Z); }
			else if constexpr (std::is_same_v<T, FVector4>) { return Value; }
			else if constexpr (std::is_same_v<T, FQuat>) { return FQuat(Value.X, Value.Y, Value.Z, Value.W); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value.X, Value.Y, Value.Z); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform(FRotator(Value.X, Value.Y, Value.Z), FVector(Value.X, Value.Y, Value.Z), FVector::OneVector); }
			else if constexpr (std::is_same_v<T, FString>) { return Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FQuat

		else if constexpr (std::is_same_v<T_VALUE, FQuat>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExValueHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return !Value.IsIdentity(); }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				const FRotator R = Value.Rotator();
				switch (Field)
				{
				default:
				case ESingleField::X: return static_cast<T>(R.Roll);
				case ESingleField::Y: return static_cast<T>(R.Yaw);
				case ESingleField::Z: return static_cast<T>(R.Pitch);
				case ESingleField::W: return static_cast<T>(0);
				case ESingleField::Length:
				case ESingleField::SquaredLength:
				case ESingleField::Volume:
				case ESingleField::Sum: return static_cast<T>(0);
				}
			}
			else if constexpr (std::is_same_v<T, FVector2D>)
			{
				if (bIsAxisSet) { return FVector2D(PCGExMath::GetDirection(Value, Axis)); }
				const FRotator R = Value.Rotator();
				return FVector2D(R.Roll, R.Pitch);
			}
			else if constexpr (std::is_same_v<T, FVector>)
			{
				if (bIsAxisSet) { return PCGExMath::GetDirection(Value, Axis); }
				const FRotator R = Value.Rotator();
				return FVector(R.Roll, R.Pitch, R.Yaw);
			}
			else if constexpr (std::is_same_v<T, FVector4>)
			{
				if (bIsAxisSet)
				{
					const FVector Dir = PCGExMath::GetDirection(Value, Axis);
					return FVector4(Dir.X, Dir.Y, Dir.Z, 0);
				}
				return FVector4(Value.X, Value.Y, Value.Z, Value.W);
			}
			else if constexpr (std::is_same_v<T, FQuat>) { return Value; }
			else if constexpr (std::is_same_v<T, FRotator>) { return Value.Rotator(); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform(Value, FVector::ZeroVector, FVector::OneVector); }
			else if constexpr (std::is_same_v<T, FString>) { return Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FRotator

		else if constexpr (std::is_same_v<T_VALUE, FRotator>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExValueHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return !Value.IsZero(); }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				switch (Field)
				{
				default:
				case ESingleField::X: return static_cast<T>(Value.Roll);
				case ESingleField::Y: return static_cast<T>(Value.Yaw);
				case ESingleField::Z: return static_cast<T>(Value.Pitch);
				case ESingleField::W: return static_cast<T>(0);
				case ESingleField::Length:
				case ESingleField::SquaredLength:
				case ESingleField::Volume:
				case ESingleField::Sum: return static_cast<T>(0);
				}
			}
			else if constexpr (std::is_same_v<T, FVector2D>)
			{
				if (bIsAxisSet) { return FVector2D(PCGExMath::GetDirection(Value.Quaternion(), Axis)); }
				return FVector2D(Value.Roll, Value.Pitch);
			}
			else if constexpr (std::is_same_v<T, FVector>)
			{
				if (bIsAxisSet) { return PCGExMath::GetDirection(Value.Quaternion(), Axis); }
				return FVector(Value.Roll, Value.Pitch, Value.Yaw);
			}
			else if constexpr (std::is_same_v<T, FVector4>)
			{
				if (bIsAxisSet)
				{
					const FVector Dir = PCGExMath::GetDirection(Value.Quaternion(), Axis);
					return FVector4(Dir.X, Dir.Y, Dir.Z, 0);
				}
				return FVector4(Value.Roll, Value.Pitch, Value.Yaw, 0);
			}
			else if constexpr (std::is_same_v<T, FQuat>) { return Value.Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return Value; }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform(Value, FVector::ZeroVector, FVector::OneVector); }
			else if constexpr (std::is_same_v<T, FString>) { return Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FTransform

		else if constexpr (std::is_same_v<T_VALUE, FTransform>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExValueHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return true; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				if (Component == ETransformPart::Position) { return Get<FVector, T>(Value.GetLocation()); }
				if (Component == ETransformPart::Scale) { return Get<FVector, T>(Value.GetScale3D()); }
				return Get<FQuat, T>(Value.GetRotation());
			}
			else if constexpr (std::is_same_v<T, FVector2D>)
			{
				if (Component == ETransformPart::Position) { return Get<FVector, T>(Value.GetLocation()); }
				if (Component == ETransformPart::Scale) { return Get<FVector, T>(Value.GetScale3D()); }
				return Get<FQuat, T>(Value.GetRotation());
			}
			else if constexpr (std::is_same_v<T, FVector>)
			{
				if (Component == ETransformPart::Position) { return Value.GetLocation(); }
				if (Component == ETransformPart::Scale) { return Value.GetScale3D(); }
				return Get<FQuat, T>(Value.GetRotation());
			}
			else if constexpr (std::is_same_v<T, FVector4>)
			{
				if (Component == ETransformPart::Position) { return FVector4(Value.GetLocation(), 0); }
				if (Component == ETransformPart::Scale) { return FVector4(Value.GetScale3D(), 0); }
				return Get<FQuat, T>(Value.GetRotation());
			}
			else if constexpr (std::is_same_v<T, FQuat>) { return Value.GetRotation(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return Value.GetRotation().Rotator(); }
			else if constexpr (std::is_same_v<T, FTransform>) { return Value; }
			else if constexpr (std::is_same_v<T, FString>) { return Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FString

		else if constexpr (std::is_same_v<T_VALUE, FString>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExValueHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, bool>) { return Value.ToBool(); }
			else if constexpr (std::is_same_v<T, int32>) { return FCString::Atoi(*Value); }
			else if constexpr (std::is_same_v<T, int64>) { return FCString::Atoi64(*Value); }
			else if constexpr (std::is_same_v<T, float>) { return FCString::Atof(*Value); }
			else if constexpr (std::is_same_v<T, double>) { return FCString::Atod(*Value); }
			else if constexpr (std::is_same_v<T, FVector2D>) { FVector2D V; V.InitFromString(Value); return V; }
			else if constexpr (std::is_same_v<T, FVector>) { FVector V; V.InitFromString(Value); return V; }
			else if constexpr (std::is_same_v<T, FVector4>) { FVector4 V; V.InitFromString(Value); return V; }
			else if constexpr (std::is_same_v<T, FQuat>) { FQuat V; V.InitFromString(Value); return V; }
			else if constexpr (std::is_same_v<T, FRotator>) { FRotator V; V.InitFromString(Value); return V; }
			else if constexpr (std::is_same_v<T, FTransform>) { FTransform V; V.InitFromString(Value); return V; }
			else if constexpr (std::is_same_v<T, FString>) { return Value; }
			else if constexpr (std::is_same_v<T, FName>) { return FName(Value); }
			else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return FSoftObjectPath(Value); }
			else if constexpr (std::is_same_v<T, FSoftClassPath>) { return FSoftClassPath(Value); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FName

		else if constexpr (std::is_same_v<T_VALUE, FName>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExValueHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, FString>) { return Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return Value; }
			else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return FSoftObjectPath(Value.ToString()); }
			else if constexpr (std::is_same_v<T, FSoftClassPath>) { return FSoftClassPath(Value.ToString()); }
			else { return Get<FString, T>(Value.ToString()); }
		}

#pragma endregion

#pragma region Convert from FSoftObjectPath / FSoftClassPath

		else if constexpr (std::is_same_v<T_VALUE, FSoftObjectPath> || std::is_same_v<T_VALUE, FSoftClassPath>)
		{
			if constexpr (std::is_same_v<T, decltype(Value)>) { return Value; }
			else if constexpr (std::is_same_v<T, PCGExValueHash>) { return GetTypeHash(Value); }

			else if constexpr (std::is_same_v<T, FString>) { return Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(Value.ToString()); }
			else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return FSoftObjectPath(Value.ToString()); }
			else if constexpr (std::is_same_v<T, FSoftClassPath>) { return FSoftClassPath(Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

		else
		{
			return T{};
		}
	}

	//
	// Original templated Set implementation
	//
	template <typename T, typename T_VALUE>
	void FSubSelection::Set(T& Target, const T_VALUE& Value) const
	{
		// Scalar target type -- can only set from the first component of multi-component sources
		if constexpr (std::is_same_v<T, bool>)
		{
			if constexpr (std::is_same_v<T_VALUE, PCGExValueHash>) { Target = Value > 0; }
			else if constexpr (std::is_same_v<T_VALUE, bool>) { Target = Value; }
			else if constexpr (std::is_same_v<T_VALUE, int32> || std::is_same_v<T_VALUE, int64> || std::is_same_v<T_VALUE, float> || std::is_same_v<T_VALUE, double>)
			{
				Target = static_cast<bool>(Value);
			}
			else if constexpr (std::is_same_v<T_VALUE, FVector2D> || std::is_same_v<T_VALUE, FVector> || std::is_same_v<T_VALUE, FVector4>)
			{
				Target = static_cast<bool>(Value[0]);
			}
			else if constexpr (std::is_same_v<T_VALUE, FQuat>)
			{
				Target = static_cast<bool>(Value.X);
			}
			else if constexpr (std::is_same_v<T_VALUE, FRotator>)
			{
				Target = static_cast<bool>(Value.Pitch);
			}
			else
			{
			}
		}
		else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
		{
			if constexpr (std::is_same_v<T_VALUE, PCGExValueHash>) { Target = static_cast<T>(Value); }
			else if constexpr (std::is_same_v<T_VALUE, bool>) { Target = Value ? 1 : 0; }
			else if constexpr (std::is_same_v<T_VALUE, int32> || std::is_same_v<T_VALUE, int64> || std::is_same_v<T_VALUE, float> || std::is_same_v<T_VALUE, double>)
			{
				Target = static_cast<T>(Value);
			}
			else if constexpr (std::is_same_v<T_VALUE, FVector2D> || std::is_same_v<T_VALUE, FVector> || std::is_same_v<T_VALUE, FVector4>)
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
			else
			{
			}
		}
		// NAry target type -- can set components by index
		else if constexpr (std::is_same_v<T, FVector2D> || std::is_same_v<T, FVector> || std::is_same_v<T, FVector4> || std::is_same_v<T, FRotator> || std::is_same_v<T, FQuat>)
		{
			double V = 1;

			if constexpr (std::is_same_v<T_VALUE, PCGExValueHash>) { V = GetTypeHash(Value); }
			else if constexpr (std::is_same_v<T_VALUE, bool>) { V = Value ? 1 : 0; }
			else if constexpr (std::is_same_v<T_VALUE, int32> || std::is_same_v<T_VALUE, int64> || std::is_same_v<T_VALUE, float> || std::is_same_v<T_VALUE, double>)
			{
				V = static_cast<double>(Value);
			}
			else if constexpr (std::is_same_v<T_VALUE, FVector2D> || std::is_same_v<T_VALUE, FVector> || std::is_same_v<T_VALUE, FVector4>)
			{
				V = static_cast<double>(Value[0]);
			}
			else if constexpr (std::is_same_v<T_VALUE, FQuat>)
			{
				V = static_cast<double>(Value.X);
			}
			else if constexpr (std::is_same_v<T_VALUE, FRotator>)
			{
				V = static_cast<double>(Value.Pitch);
			}
			else
			{
			}

			if constexpr (std::is_same_v<T, FVector2D> || std::is_same_v<T, FVector> || std::is_same_v<T, FVector4>)
			{
				switch (Field)
				{
				case ESingleField::X: Target[0] = V;
					break;
				case ESingleField::Y: Target[1] = V;
					break;
				case ESingleField::Z: if constexpr (!std::is_same_v<T, FVector2D>) { Target[2] = V; }
					break;
				case ESingleField::W: if constexpr (std::is_same_v<T, FVector4>) { Target[3] = V; }
					break;
				case ESingleField::Length: if constexpr (std::is_same_v<T, FVector4>) { Target = FVector4(FVector(Target.X, Target.Y, Target.Z).GetSafeNormal() * V, Target.W); }
					else { Target = Target.GetSafeNormal() * V; }
					break;
				case ESingleField::SquaredLength: if constexpr (std::is_same_v<T, FVector4>) { Target = FVector4(FVector(Target.X, Target.Y, Target.Z).GetSafeNormal() * FMath::Sqrt(V), Target.W); }
					else { Target = Target.GetSafeNormal() * FMath::Sqrt(V); }
					break;
				case ESingleField::Volume:
				case ESingleField::Sum: return;
				}
			}
			else if constexpr (std::is_same_v<T, FRotator>)
			{
				switch (Field)
				{
				case ESingleField::X: Target.Pitch = V;
					break;
				case ESingleField::Y: Target.Yaw = V;
					break;
				case ESingleField::Z: Target.Roll = V;
					break;
				case ESingleField::W: return;
				case ESingleField::Length: Target = Target.GetNormalized() * V;
					break;
				case ESingleField::SquaredLength: Target = Target.GetNormalized() * FMath::Sqrt(V);
					break;
				case ESingleField::Volume:
				case ESingleField::Sum: return;
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
		}
		else if constexpr (std::is_same_v<T, FName>)
		{
			if constexpr (std::is_same_v<T_VALUE, FString>) { Target = FName(Value); }
			else if constexpr (std::is_same_v<T_VALUE, FName>) { Target = Value; }
			else if constexpr (std::is_same_v<T_VALUE, FSoftClassPath>) { Target = FName(Value.ToString()); }
			else if constexpr (std::is_same_v<T_VALUE, FSoftObjectPath>) { Target = FName(Value.ToString()); }
		}
		else if constexpr (std::is_same_v<T, FSoftClassPath>)
		{
			if constexpr (std::is_same_v<T_VALUE, FString>) { Target = FSoftClassPath(Value); }
			else if constexpr (std::is_same_v<T_VALUE, FName>) { Target = FSoftClassPath(Value.ToString()); }
			else if constexpr (std::is_same_v<T_VALUE, FSoftClassPath>) { Target = Value; }
			else if constexpr (std::is_same_v<T_VALUE, FSoftObjectPath>) { Target = FSoftClassPath(Value.ToString()); }
		}
		else if constexpr (std::is_same_v<T, FSoftObjectPath>)
		{
			if constexpr (std::is_same_v<T_VALUE, FString>) { Target = FSoftObjectPath(Value); }
			else if constexpr (std::is_same_v<T_VALUE, FName>) { Target = FSoftObjectPath(Value.ToString()); }
			else if constexpr (std::is_same_v<T_VALUE, FSoftClassPath>) { Target = FSoftObjectPath(Value.ToString()); }
			else if constexpr (std::is_same_v<T_VALUE, FSoftObjectPath>) { Target = Value; }
		}
	}

	// Explicit template instantiations
#define PCGEX_TPL(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) \
	template PCGEXTENDEDTOOLKIT_API _TYPE_A FSubSelection::Get<_TYPE_B, _TYPE_A>(const _TYPE_B& Value) const; \
	template PCGEXTENDEDTOOLKIT_API void FSubSelection::Set<_TYPE_A, _TYPE_B>(_TYPE_A& Target, const _TYPE_B& Value) const;
	PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_TPL)
#undef PCGEX_TPL

	template <typename T>
	template <typename T_VALUE>
	void TValueBuffer<T>::Set(const FSubSelection& SubSelection, const int32 Index, const T_VALUE& Value)
	{
		*(Values->GetData() + Index) = SubSelection.Get<T_VALUE, T>(Value);
	}

	template <typename T>
	template <typename T_VALUE>
	T_VALUE TValueBuffer<T>::Get(const FSubSelection& SubSelection, const int32 Index) const
	{
		return SubSelection.Get<T, T_VALUE>(*(Values->GetData() + Index));
	}

#define PCGEX_TPL(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) \
	template PCGEXTENDEDTOOLKIT_API void TValueBuffer<_TYPE_A>::Set<_TYPE_B>(const FSubSelection& SubSelection, const int32 Index, const _TYPE_B& Value); \
	template PCGEXTENDEDTOOLKIT_API _TYPE_B TValueBuffer<_TYPE_A>::Get(const FSubSelection& SubSelection, const int32 Index) const;
	PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_TPL)
#undef PCGEX_TPL

	bool TryGetType(const FPCGAttributePropertyInputSelector& InputSelector, const UPCGData* InData, EPCGMetadataTypes& OutType)
	{
		OutType = EPCGMetadataTypes::Unknown;

		if (!IsValid(InData)) { return false; }

		const FPCGAttributePropertyInputSelector FixedSelector = InputSelector.CopyAndFixLast(InData);
		if (!FixedSelector.IsValid()) { return false; }

		if (FixedSelector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			if (!InData->Metadata) { return false; }
			if (const FPCGMetadataAttributeBase* AttributeBase = InData->Metadata->GetConstAttribute(GetAttributeIdentifier(FixedSelector, InData)))
			{
				OutType = static_cast<EPCGMetadataTypes>(AttributeBase->GetTypeId());
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

	bool TryGetTypeAndSource(const FPCGAttributePropertyInputSelector& InputSelector, const TSharedPtr<PCGExData::FFacade>& InDataFacade, EPCGMetadataTypes& OutType, PCGExData::EIOSide& InOutSide)
	{
		OutType = EPCGMetadataTypes::Unknown;
		if (InOutSide == PCGExData::EIOSide::In)
		{
			if (!TryGetType(InputSelector, InDataFacade->GetIn(), OutType))
			{
				if (TryGetType(InputSelector, InDataFacade->GetOut(), OutType)) { InOutSide = PCGExData::EIOSide::Out; }
			}
		}
		else
		{
			if (!TryGetType(InputSelector, InDataFacade->GetOut(), OutType))
			{
				if (TryGetType(InputSelector, InDataFacade->GetIn(), OutType)) { InOutSide = PCGExData::EIOSide::In; }
			}
		}

		return OutType != EPCGMetadataTypes::Unknown;
	}

	bool TryGetTypeAndSource(const FName AttributeName, const TSharedPtr<PCGExData::FFacade>& InDataFacade, EPCGMetadataTypes& OutType, PCGExData::EIOSide& InOutSource)
	{
		FPCGAttributePropertyInputSelector Selector;
		Selector.SetAttributeName(AttributeName);
		return TryGetTypeAndSource(Selector, InDataFacade, OutType, InOutSource);
	}
}