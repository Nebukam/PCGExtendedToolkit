// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGAttributePropertySelector.h"
//#include "PCGExLocalAttributeReader.generated.h"

namespace PCGEx
{
#pragma region Local Attribute Inputs

	template <typename T>
	struct PCGEXTENDEDTOOLKIT_API FLocalAttributeInput
	{
	public:
		virtual ~FLocalAttributeInput() = default;

		bool bEnabled = true;
		bool bValid = false;

		FPCGExInputSelector Descriptor;
		FPCGAttributePropertyInputSelector Selector;

		/**
		 * Build and validate a property/attribute accessor for the selected
		 * @param PointData 
		 */
		bool PrepareForPointData(const UPCGPointData* PointData)
		{
			bValid = false;
			if (!bEnabled) { return false; }
			if (Descriptor.Validate(PointData)) { bValid = ValidateInternal(); }
			Selector = Descriptor.Selector;
			return bValid;
		}

		virtual T GetValue(const FPCGPoint& Point) const
		{
			if (!bValid || !bEnabled) { return GetDefaultValue(); }

			switch (Selector.GetSelection())
			{
			case EPCGAttributePropertySelection::Attribute:
				return PCGMetadataAttribute::CallbackWithRightType(
					Descriptor.Attribute->GetTypeId(),
					[this, &Point](auto DummyValue) -> T
					{
						using AttributeType = decltype(DummyValue);
						FPCGMetadataAttribute<AttributeType>* Attribute = static_cast<FPCGMetadataAttribute<AttributeType>*>(Descriptor.Attribute);
						return Convert(Attribute->GetValueFromItemKey(Point.MetadataEntry));
					});
#define PCGEX_GET_BY_ACCESSOR(_ENUM, _ACCESSOR) case _ENUM: return Convert(Point._ACCESSOR);
			case EPCGAttributePropertySelection::PointProperty:
				switch (Selector.GetPointProperty())
				{
				PCGEX_FOREACH_POINTPROPERTY(PCGEX_GET_BY_ACCESSOR)
				}
				break;
			case EPCGAttributePropertySelection::ExtraProperty:
				switch (Selector.GetExtraProperty())
				{
				PCGEX_FOREACH_POINTEXTRAPROPERTY(PCGEX_GET_BY_ACCESSOR)
				}
				break;
			}

			return GetDefaultValue();
#undef PCGEX_GET_BY_ACCESSOR
		}

	protected:
		virtual bool ValidateInternal() const { return true; }
		virtual T GetDefaultValue() const = 0;

#define  PCGEX_PRINT_VIRTUAL(_TYPE, _NAME) virtual T Convert(const _TYPE Value) const = 0;\
		
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_PRINT_VIRTUAL)
		
	};


#define PCGEX_SINGLE(_NAME, _TYPE)\
struct PCGEXTENDEDTOOLKIT_API FLocal ## _NAME ## Input : public FLocalAttributeInput<_TYPE>	{\
protected: \
virtual _TYPE GetDefaultValue() const { return 0; }\
virtual _TYPE Convert(const int32 Value) const override { return static_cast<_TYPE>(Value); } \
virtual _TYPE Convert(const int64 Value) const override { return static_cast<_TYPE>(Value); }\
virtual _TYPE Convert(const float Value) const override { return static_cast<_TYPE>(Value); }\
virtual _TYPE Convert(const double Value) const override { return static_cast<_TYPE>(Value); }\
virtual _TYPE Convert(const FVector2D Value) const override { return static_cast<_TYPE>(Value.Length()); }\
virtual _TYPE Convert(const FVector Value) const override { return static_cast<_TYPE>(Value.Length()); }\
virtual _TYPE Convert(const FVector4 Value) const override { return static_cast<_TYPE>(FVector(Value).Length()); }\
virtual _TYPE Convert(const FQuat Value) const override { return static_cast<_TYPE>(Value.GetForwardVector().Length()); }\
virtual _TYPE Convert(const FTransform Value) const override { return static_cast<_TYPE>(Value.GetLocation().Length()); }\
virtual _TYPE Convert(const bool Value) const override { return static_cast<_TYPE>(Value); }\
virtual _TYPE Convert(const FRotator Value) const override { return static_cast<_TYPE>(Value.Euler().Length()); }\
virtual _TYPE Convert(const FString Value) const override { return static_cast<_TYPE>(GetTypeHash(Value)); }\
virtual _TYPE Convert(const FName Value) const override { return static_cast<_TYPE>(GetTypeHash(Value)); }\
};

	PCGEX_SINGLE(Integer32, int32)

	PCGEX_SINGLE(Integer64, int64)

	PCGEX_SINGLE(Float, float)

	PCGEX_SINGLE(Double, double)

	PCGEX_SINGLE(Boolean, bool)

#undef PCGEX_SINGLE

#define PCGEX_VECTOR_CAST(_NAME, _TYPE, VECTOR2D)\
struct PCGEXTENDEDTOOLKIT_API FLocal ## _NAME ## Input : public FLocalAttributeInput<_TYPE>	{\
protected: \
virtual _TYPE GetDefaultValue() const { return _TYPE(0); }\
virtual _TYPE Convert(const int32 Value) const override { return _TYPE(Value); } \
virtual _TYPE Convert(const int64 Value) const override { return _TYPE(Value); }\
virtual _TYPE Convert(const float Value) const override { return _TYPE(Value); }\
virtual _TYPE Convert(const double Value) const override { return _TYPE(Value); }\
virtual _TYPE Convert(const FVector2D Value) const VECTOR2D \
virtual _TYPE Convert(const FVector Value) const override { return _TYPE(Value); }\
virtual _TYPE Convert(const FVector4 Value) const override { return _TYPE(Value); }\
virtual _TYPE Convert(const FQuat Value) const override { return _TYPE(Value.GetForwardVector()); }\
virtual _TYPE Convert(const FTransform Value) const override { return _TYPE(Value.GetLocation()); }\
virtual _TYPE Convert(const bool Value) const override { return _TYPE(Value); }\
virtual _TYPE Convert(const FRotator Value) const override { return _TYPE(Value.Vector()); }\
};

	PCGEX_VECTOR_CAST(Vector2, FVector2D, { return Value;})

	PCGEX_VECTOR_CAST(Vector, FVector, { return FVector(Value.X, Value.Y, 0);})

	PCGEX_VECTOR_CAST(Vector4, FVector4, { return FVector4(Value.X, Value.Y, 0, 0);})

#undef PCGEX_VECTOR_CAST

#define PCGEX_LITERAL_CAST(_NAME, _TYPE)\
struct PCGEXTENDEDTOOLKIT_API FLocal ## _NAME ## Input : public FLocalAttributeInput<_TYPE>	{\
protected: \
virtual _TYPE GetDefaultValue() const { return _TYPE(""); }\
virtual _TYPE Convert(const int32 Value) const override { return _TYPE(FString::FromInt(Value)); } \
virtual _TYPE Convert(const int64 Value) const override { return _TYPE(FString::FromInt(Value)); }\
virtual _TYPE Convert(const float Value) const override { return _TYPE(FString::SanitizeFloat(Value)); }\
virtual _TYPE Convert(const double Value) const override { return _TYPE(""); }\
virtual _TYPE Convert(const FVector2D Value) const override { return _TYPE(Value.ToString()); } \
virtual _TYPE Convert(const FVector Value) const override { return _TYPE(Value.ToString()); }\
virtual _TYPE Convert(const FVector4 Value) const override { return _TYPE(Value.ToString()); }\
virtual _TYPE Convert(const FQuat Value) const override { return _TYPE(Value.ToString()); }\
virtual _TYPE Convert(const FTransform Value) const override { return _TYPE(Value.ToString()); }\
virtual _TYPE Convert(const bool Value) const override { return _TYPE(FString::FromInt(Value)); }\
virtual _TYPE Convert(const FRotator Value) const override { return _TYPE(Value.ToString()); }\
virtual _TYPE Convert(const FString Value) const override { return _TYPE(Value); }\
virtual _TYPE Convert(const FName Value) const override { return _TYPE(Value.ToString()); }\
};

	PCGEX_LITERAL_CAST(String, FString)

	PCGEX_LITERAL_CAST(Name, FName)

#undef PCGEX_LITERAL_CAST

#pragma endregion

#pragma region Local Attribute Component Reader

	struct PCGEXTENDEDTOOLKIT_API FLocalSingleComponentInput : public FLocalAttributeInput<double>
	{
		FLocalSingleComponentInput()
		{
		}

		FLocalSingleComponentInput(
			EPCGExSingleFieldSelection InFieldSelection,
			EPCGExDirectionSelection InDirection)
		{
			FieldSelection = InFieldSelection;
			Direction = InDirection;
		}

	public:
		EPCGExSingleFieldSelection FieldSelection = EPCGExSingleFieldSelection::X;
		EPCGExDirectionSelection Direction = EPCGExDirectionSelection::Forward;

	protected:
		virtual double GetDefaultValue() const override { return 0; }

		virtual double Convert(const int32 Value) const override { return static_cast<double>(Value); }
		virtual double Convert(const int64 Value) const override { return static_cast<double>(Value); }
		virtual double Convert(const float Value) const override { return static_cast<double>(Value); }
		virtual double Convert(const double Value) const override { return static_cast<double>(Value); }
		virtual double Convert(const FVector2D Value) const override
		{
			switch (FieldSelection)
			{
			default:
			case EPCGExSingleFieldSelection::X:
				return Value.X;
			case EPCGExSingleFieldSelection::Y:
			case EPCGExSingleFieldSelection::Z:
			case EPCGExSingleFieldSelection::W:
				return Value.Y;
			case EPCGExSingleFieldSelection::Length:
				return Value.Length();
			}
		}

		virtual double Convert(const FVector Value) const override
		{
			switch (FieldSelection)
			{
			default:
			case EPCGExSingleFieldSelection::X:
				return Value.X;
			case EPCGExSingleFieldSelection::Y:
				return Value.Y;
			case EPCGExSingleFieldSelection::Z:
			case EPCGExSingleFieldSelection::W:
				return Value.Z;
			case EPCGExSingleFieldSelection::Length:
				return Value.Length();
			}
		}

		virtual double Convert(const FVector4 Value) const override
		{
			switch (FieldSelection)
			{
			default:
			case EPCGExSingleFieldSelection::X:
				return Value.X;
			case EPCGExSingleFieldSelection::Y:
				return Value.Y;
			case EPCGExSingleFieldSelection::Z:
				return Value.Z;
			case EPCGExSingleFieldSelection::W:
				return Value.W;
			case EPCGExSingleFieldSelection::Length:
				return FVector(Value).Length();
			}
		}

		virtual double Convert(const FQuat Value) const override { return Convert(Common::GetDirection(Value, Direction)); }
		virtual double Convert(const FTransform Value) const override { return Convert(Value.GetLocation()); }
		virtual double Convert(const bool Value) const override { return static_cast<double>(Value); }
		virtual double Convert(const FRotator Value) const override { return Convert(Value.Vector()); }
		virtual double Convert(const FString Value) const override { return Common::ConvertStringToDouble(Value); }
		virtual double Convert(const FName Value) const override { return Common::ConvertStringToDouble(Value.ToString()); }
		
	};

	struct PCGEXTENDEDTOOLKIT_API FLocalDirectionInput : public FLocalAttributeInput<FVector>
	{
		FLocalDirectionInput()
		{
		}

		FLocalDirectionInput(
			EPCGExDirectionSelection InDirection)
		{
			Direction = InDirection;
		}

	public:
		EPCGExDirectionSelection Direction = EPCGExDirectionSelection::Forward;

	protected:
		virtual FVector GetDefaultValue() const override { return FVector::ZeroVector; }

		virtual FVector Convert(const bool Value) const override { return GetDefaultValue(); }
		virtual FVector Convert(const int32 Value) const override { return GetDefaultValue(); }
		virtual FVector Convert(const int64 Value) const override { return GetDefaultValue(); }
		virtual FVector Convert(const float Value) const override { return GetDefaultValue(); }
		virtual FVector Convert(const double Value) const override { return GetDefaultValue(); }		
		virtual FVector Convert(const FVector2D Value) const override { return FVector(Value.X, Value.Y, 0); }
		virtual FVector Convert(const FVector Value) const override { return Value; }
		virtual FVector Convert(const FVector4 Value) const override { return FVector(Value); }
		virtual FVector Convert(const FQuat Value) const override { return Common::GetDirection(Value, Direction); }
		virtual FVector Convert(const FTransform Value) const override { return Common::GetDirection(Value.GetRotation(), Direction); }
		virtual FVector Convert(const FRotator Value) const override { return Value.Vector(); }
		virtual FVector Convert(const FString Value) const override { return GetDefaultValue(); }
		virtual FVector Convert(const FName Value) const override { return GetDefaultValue(); }
		
	};

#pragma endregion
}
