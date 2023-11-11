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
						return GetValueInternal(Attribute->GetValueFromItemKey(Point.MetadataEntry));
					});
#define PCGEX_GET_BY_ACCESSOR(_ENUM, _ACCESSOR) case _ENUM: return GetValueInternal(Point._ACCESSOR);
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

		template <typename InV, typename dummy = void>
		T GetValueInternal(InV Value) const { return GetDefaultValue(); };

		static double ConvertStringToDouble(const FString& StringToConvert)
		{
			const TCHAR* CharArray = *StringToConvert;
			const double Result = FCString::Atod(CharArray);
			return FMath::IsNaN(Result) ? 0 : Result;
		}

		FVector GetDirection(FQuat Quat, EPCGExDirectionSelection Dir) const
		{
			switch (Dir)
			{
			default:
			case EPCGExDirectionSelection::Forward:
				return Quat.GetForwardVector();
			case EPCGExDirectionSelection::Backward:
				return Quat.GetForwardVector() * -1;
			case EPCGExDirectionSelection::Right:
				return Quat.GetRightVector();
			case EPCGExDirectionSelection::Left:
				return Quat.GetRightVector() * -1;
			case EPCGExDirectionSelection::Up:
				return Quat.GetUpVector();
			case EPCGExDirectionSelection::Down:
				return Quat.GetUpVector() * -1;
			}
		}
	};


#define PCGEX_SINGLE(_NAME, _TYPE)\
struct PCGEXTENDEDTOOLKIT_API FLocal ## _NAME ## Input : public FLocalAttributeInput<_TYPE>	{\
protected: \
virtual _TYPE GetDefaultValue() const { return 0; }\
template <typename dummy = void> _TYPE GetValueInternal(int32 Value) const { return Value; } \
template <typename dummy = void> _TYPE GetValueInternal(int64 Value) const { return static_cast<_TYPE>(Value); }\
template <typename dummy = void> _TYPE GetValueInternal(float Value) const { return static_cast<_TYPE>(Value); }\
template <typename dummy = void> _TYPE GetValueInternal(double Value) const { return static_cast<_TYPE>(Value); }\
template <typename dummy = void> _TYPE GetValueInternal(FVector2D Value) const { return static_cast<_TYPE>(Value.Length()); }\
template <typename dummy = void> _TYPE GetValueInternal(FVector Value) const { return static_cast<_TYPE>(Value.Length()); }\
template <typename dummy = void> _TYPE GetValueInternal(FVector4 Value) const { return static_cast<_TYPE>(FVector(Value).Length()); }\
template <typename dummy = void> _TYPE GetValueInternal(FQuat Value) const { return static_cast<_TYPE>(Value.GetForwardVector().Length()); }\
template <typename dummy = void> _TYPE GetValueInternal(FTransform Value) const { return static_cast<_TYPE>(Value.GetLocation().Length()); }\
template <typename dummy = void> _TYPE GetValueInternal(bool Value) const { return static_cast<_TYPE>(Value); }\
template <typename dummy = void> _TYPE GetValueInternal(FRotator Value) const { return static_cast<_TYPE>(Value.Euler().Length()); }\
template <typename dummy = void> _TYPE GetValueInternal(FString Value) const { return static_cast<_TYPE>(GetTypeHash(Value)); }\
template <typename dummy = void> _TYPE GetValueInternal(FName Value) const { return static_cast<_TYPE>(GetTypeHash(Value)); }\
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
template <typename dummy = void> _TYPE GetValueInternal(int32 Value) const { return _TYPE(Value); } \
template <typename dummy = void> _TYPE GetValueInternal(int64 Value) const { return _TYPE(Value); }\
template <typename dummy = void> _TYPE GetValueInternal(float Value) const { return _TYPE(Value); }\
template <typename dummy = void> _TYPE GetValueInternal(double Value) const { return _TYPE(Value); }\
template <typename dummy = void> _TYPE GetValueInternal(FVector2D Value) const VECTOR2D \
template <typename dummy = void> _TYPE GetValueInternal(FVector Value) const { return _TYPE(Value); }\
template <typename dummy = void> _TYPE GetValueInternal(FVector4 Value) const { return _TYPE(Value); }\
template <typename dummy = void> _TYPE GetValueInternal(FQuat Value) const { return _TYPE(Value.GetForwardVector()); }\
template <typename dummy = void> _TYPE GetValueInternal(FTransform Value) const { return _TYPE(Value.GetLocation()); }\
template <typename dummy = void> _TYPE GetValueInternal(bool Value) const { return _TYPE(Value); }\
template <typename dummy = void> _TYPE GetValueInternal(FRotator Value) const { return _TYPE(Value.Vector()); }\
};

	PCGEX_VECTOR_CAST(Vector2, FVector2D, { return Value;})

	PCGEX_VECTOR_CAST(Vector, FVector, { return FVector(Value.X, Value.Y, 0);})

	PCGEX_VECTOR_CAST(Vector4, FVector4, { return FVector4(Value.X, Value.Y, 0, 0);})

#undef PCGEX_VECTOR_CAST

#define PCGEX_LITERAL_CAST(_NAME, _TYPE)\
struct PCGEXTENDEDTOOLKIT_API FLocal ## _NAME ## Input : public FLocalAttributeInput<_TYPE>	{\
protected: \
virtual _TYPE GetDefaultValue() const { return _TYPE(""); }\
template <typename dummy = void> _TYPE GetValueInternal(int32 Value) const { return _TYPE(FString::FromInt(Value)); } \
template <typename dummy = void> _TYPE GetValueInternal(int64 Value) const { return _TYPE(FString::FromInt(Value)); }\
template <typename dummy = void> _TYPE GetValueInternal(float Value) const { return _TYPE(FString::SanitizeFloat(Value)); }\
template <typename dummy = void> _TYPE GetValueInternal(double Value) const { return _TYPE(""); }\
template <typename dummy = void> _TYPE GetValueInternal(FVector2D Value) const { return _TYPE(Value.ToString()); } \
template <typename dummy = void> _TYPE GetValueInternal(FVector Value) const { return _TYPE(Value.ToString()); }\
template <typename dummy = void> _TYPE GetValueInternal(FVector4 Value) const { return _TYPE(Value.ToString()); }\
template <typename dummy = void> _TYPE GetValueInternal(FQuat Value) const { return _TYPE(Value.ToString()); }\
template <typename dummy = void> _TYPE GetValueInternal(FTransform Value) const { return _TYPE(Value.ToString()); }\
template <typename dummy = void> _TYPE GetValueInternal(bool Value) const { return _TYPE(FString::FromInt(Value)); }\
template <typename dummy = void> _TYPE GetValueInternal(FRotator Value) const { return _TYPE(Value.ToString()); }\
template <typename dummy = void> _TYPE GetValueInternal(FString Value) const { return _TYPE(Value); }\
template <typename dummy = void> _TYPE GetValueInternal(FName Value) const { return _TYPE(Value.ToString()); }\
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
			EPCGExSingleComponentSelection InComponent,
			EPCGExDirectionSelection InDirection)
		{
			Component = InComponent;
			Direction = InDirection;
		}
		
	public:
		EPCGExSingleComponentSelection Component = EPCGExSingleComponentSelection::X;
		EPCGExDirectionSelection Direction = EPCGExDirectionSelection::Forward;

	protected:
		virtual double GetDefaultValue() const { return 0; }

		template <typename dummy = void>
		double GetValueInternal(int32 Value) const { return Value; }

		template <typename dummy = void>
		double GetValueInternal(int64 Value) const { return static_cast<double>(Value); }

		template <typename dummy = void>
		double GetValueInternal(float Value) const { return static_cast<double>(Value); }

		template <typename dummy = void>
		double GetValueInternal(double Value) const { return static_cast<double>(Value); }

		template <typename dummy = void>
		double GetValueInternal(FVector2D Value) const
		{
			switch (Component)
			{
			default:
			case EPCGExSingleComponentSelection::X:
				return Value.X;
			case EPCGExSingleComponentSelection::Y:
			case EPCGExSingleComponentSelection::Z:
			case EPCGExSingleComponentSelection::W:
				return Value.Y;
			case EPCGExSingleComponentSelection::Length:
				return Value.Length();
			}
		}

		template <typename dummy = void>
		double GetValueInternal(FVector Value) const
		{
			switch (Component)
			{
			default:
			case EPCGExSingleComponentSelection::X:
				return Value.X;
			case EPCGExSingleComponentSelection::Y:
				return Value.Y;
			case EPCGExSingleComponentSelection::Z:
			case EPCGExSingleComponentSelection::W:
				return Value.Z;
			case EPCGExSingleComponentSelection::Length:
				return Value.Length();
			}
		}

		template <typename dummy = void>
		double GetValueInternal(FVector4 Value) const
		{
			switch (Component)
			{
			default:
			case EPCGExSingleComponentSelection::X:
				return Value.X;
			case EPCGExSingleComponentSelection::Y:
				return Value.Y;
			case EPCGExSingleComponentSelection::Z:
				return Value.Z;
			case EPCGExSingleComponentSelection::W:
				return Value.W;
			case EPCGExSingleComponentSelection::Length:
				return FVector(Value).Length();
			}
		}

		template <typename dummy = void>
		double GetValueInternal(FQuat Value) const { return GetValueInternal(GetDirection(Value, Direction)); }

		template <typename dummy = void>
		double GetValueInternal(FTransform Value) const { return GetValueInternal(Value.GetLocation()); }

		template <typename dummy = void>
		double GetValueInternal(bool Value) const { return static_cast<double>(Value); }

		template <typename dummy = void>
		double GetValueInternal(FRotator Value) const { return GetValueInternal(Value.Vector()); }

		template <typename dummy = void>
		double GetValueInternal(FString Value) const { return ConvertStringToDouble(Value); }

		template <typename dummy = void>
		double GetValueInternal(FName Value) const { return ConvertStringToDouble(Value.ToString()); }
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
		virtual FVector GetDefaultValue() const { return FVector::ZeroVector; }

		template <typename dummy = void>
		FVector GetValueInternal(FVector2D Value) const { return FVector(Value.X, Value.Y, 0); }

		template <typename dummy = void>
		FVector GetValueInternal(FVector Value) const { return Value; }

		template <typename dummy = void>
		FVector GetValueInternal(FVector4 Value) const { return FVector(Value); }

		template <typename dummy = void>
		FVector GetValueInternal(FQuat Value) const { return GetDirection(Value, Direction); }

		template <typename dummy = void>
		FVector GetValueInternal(FTransform Value) const { return GetDirection(Value.GetRotation(), Direction); }

		template <typename dummy = void>
		FVector GetValueInternal(FRotator Value) const { return Value.Vector(); }
	};

#pragma endregion
}
