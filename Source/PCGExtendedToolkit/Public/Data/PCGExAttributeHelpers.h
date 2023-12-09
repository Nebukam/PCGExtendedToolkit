// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "PCGEx.h"
#include "PCGExMath.h"
#include "PCGExPointIO.h"
#include "Metadata/Accessors/PCGAttributeAccessor.h"

#include "PCGExAttributeHelpers.generated.h"

#pragma region Input Descriptors

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputDescriptor
{
	GENERATED_BODY()

	FPCGExInputDescriptor()
	{
		bValidatedAtLeastOnce = false;
	}

	FPCGExInputDescriptor(const FPCGExInputDescriptor& Other)
		: FPCGExInputDescriptor()
	{
		Selector = Other.Selector;
		Attribute = Other.Attribute;
	}

public:
	virtual ~FPCGExInputDescriptor()
	{
		Attribute = nullptr;
	};

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (HideInDetailPanel, Hidden, EditConditionHides, EditCondition="false"))
	FString TitlePropertyName;

	/** Point Attribute or $Property */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayPriority=0))
	FPCGAttributePropertyInputSelector Selector;

	FPCGMetadataAttributeBase* Attribute = nullptr;
	bool bValidatedAtLeastOnce = false;
	int16 UnderlyingType = 0;

	/**
	 * 
	 * @tparam T 
	 * @return 
	 */
	template <typename T>
	FPCGMetadataAttribute<T>* GetTypedAttribute()
	{
		if (Attribute == nullptr) { return nullptr; }
		return static_cast<FPCGMetadataAttribute<T>*>(Attribute);
	}

	FPCGAttributePropertyInputSelector& GetMutableSelector() { return Selector; }

public:
	EPCGAttributePropertySelection GetSelection() const { return Selector.GetSelection(); }
	FName GetName() const { return Selector.GetName(); }
#if WITH_EDITOR
	virtual FString GetDisplayName() const;
	void UpdateUserFacingInfos();
#endif
	/**
	 * Validate & cache the current selector for a given UPCGPointData
	 * @param InData 
	 * @return 
	 */
	bool Validate(const UPCGPointData* InData);
	FString ToString() const { return GetName().ToString(); }
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputDescriptorGeneric : public FPCGExInputDescriptor
{
	GENERATED_BODY()

	FPCGExInputDescriptorGeneric()
		: FPCGExInputDescriptor()
	{
	}

	FPCGExInputDescriptorGeneric(const FPCGExInputDescriptorGeneric& Other)
		: FPCGExInputDescriptor(Other),
		  Type(Other.Type),
		  Axis(Other.Axis),
		  Field(Other.Field)
	{
	}

public:
	/** How to interpret the data */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayAfter="Selector"))
	EPCGExSelectorType Type = EPCGExSelectorType::SingleField;

	/** Direction to sample on relevant data types (FQuat are transformed to a direction first, from which the single component is selected) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName=" └─ Axis", PCG_Overridable, DisplayAfter="Type"))
	EPCGExAxis Axis = EPCGExAxis::Forward;

	/** Single field selection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName=" └─ Field", PCG_Overridable, DisplayAfter="Axis", EditCondition="Type==EPCGExSelectorType::SingleField", EditConditionHides))
	EPCGExSingleField Field = EPCGExSingleField::X;

	virtual ~FPCGExInputDescriptorGeneric() override
	{
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputDescriptorWithDirection : public FPCGExInputDescriptor
{
	GENERATED_BODY()

	FPCGExInputDescriptorWithDirection()
		: FPCGExInputDescriptor()
	{
	}

	FPCGExInputDescriptorWithDirection(const FPCGExInputDescriptorWithDirection& Other)
		: FPCGExInputDescriptor(Other),
		  Axis(Other.Axis)
	{
	}

public:
	/** Sub-component order, used only for multi-field attributes (FVector, FRotator etc). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName=" └─ Axis", PCG_Overridable, DisplayAfter="Selector"))
	EPCGExAxis Axis = EPCGExAxis::Forward;

	virtual ~FPCGExInputDescriptorWithDirection() override
	{
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputDescriptorWithSingleField : public FPCGExInputDescriptor
{
	GENERATED_BODY()

	FPCGExInputDescriptorWithSingleField(): FPCGExInputDescriptor()
	{
	}

	FPCGExInputDescriptorWithSingleField(const FPCGExInputDescriptorWithSingleField& Other)
		: FPCGExInputDescriptor(Other),
		  Axis(Other.Axis),
		  Field(Other.Field)
	{
	}

public:
	/** Direction to sample on relevant data types (FQuat are transformed to a direction first, from which the single component is selected) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName=" └─ Axis", PCG_Overridable, DisplayAfter="Selector"))
	EPCGExAxis Axis = EPCGExAxis::Forward;

	/** Single field selection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName=" └─ Field", PCG_Overridable, DisplayAfter="Axis"))
	EPCGExSingleField Field = EPCGExSingleField::X;

	virtual ~FPCGExInputDescriptorWithSingleField() override
	{
	}
};

#pragma endregion

namespace PCGEx
{
	struct PCGEXTENDEDTOOLKIT_API FAttributeIdentity
	{
		FName Name = NAME_None;
		EPCGMetadataTypes UnderlyingType = EPCGMetadataTypes::Unknown;

		FString GetDisplayName() const { return FString(Name.ToString() + FString::Printf(TEXT("( %d )"), UnderlyingType)); }
		bool operator==(const FAttributeIdentity& Other) const { return Name == Other.Name; }

		static void Get(const UPCGPointData* InData, TArray<FAttributeIdentity>& OutIdentities);
		static void Get(const UPCGPointData* InData, TArray<FName>& OutNames, TMap<FName, FAttributeIdentity>& OutIdentities);
	};

#pragma region Accessors

#define PCGEX_AAFLAG EPCGAttributeAccessorFlags::StrictType

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FAttributeAccessorBase
	{
	protected:
		FPCGMetadataAttribute<T>* Attribute = nullptr;
		int32 NumEntries = -1;
		TUniquePtr<FPCGAttributeAccessor<T>> Accessor;
		FPCGAttributeAccessorKeysPoints* InternalKeys = nullptr;
		IPCGAttributeAccessorKeys* Keys = nullptr;

		void Flush()
		{
			if (Accessor) { Accessor.Reset(); }
			PCGEX_DELETE(InternalKeys)
			Keys = nullptr;
			Attribute = nullptr;
		}

	public:
		FAttributeAccessorBase(const UPCGPointData* InData, FPCGMetadataAttributeBase* InAttribute, FPCGAttributeAccessorKeysPoints* InKeys)
		{
			Flush();
			Attribute = static_cast<FPCGMetadataAttribute<T>*>(InAttribute);
			Accessor = MakeUnique<FPCGAttributeAccessor<T>>(Attribute, InData->Metadata);
			NumEntries = InKeys->GetNum();
			Keys = InKeys;
		}

		T GetDefaultValue() { return Attribute->GetValue(PCGInvalidEntryKey); }

		int32 GetNum() const { return NumEntries; }

		virtual T Get(const int32 Index)
		{
			if (T OutValue; Get(OutValue, Index)) { return OutValue; }
			return GetDefaultValue();
		}

		bool Get(T& OutValue, int32 Index) const
		{
			TArrayView<T> Temp(&OutValue, 1);
			return GetRange(TArrayView<T>(&OutValue, 1), Index);
		}

		bool GetRange(TArrayView<T> OutValues, int32 Index = 0, FPCGAttributeAccessorKeysPoints* InKeys = nullptr) const
		{
			return Accessor->GetRange(OutValues, Index, InKeys ? *InKeys : *Keys, PCGEX_AAFLAG);
		}
		
		bool GetRange(TArray<T>& OutValues, const int32 Index = 0, FPCGAttributeAccessorKeysPoints* InKeys = nullptr, int32 Count = -1) const
		{
			OutValues.SetNumUninitialized( Count == -1 ? NumEntries - Index : Count, true);
			TArrayView<T> View(OutValues);
			return Accessor->GetRange(View, Index, InKeys ? *InKeys : *Keys, PCGEX_AAFLAG);
		}

		bool Set(const T& InValue, const int32 Index) { return SetRange(TArrayView<const T>(&InValue, 1), Index); }


		bool SetRange(TArrayView<const T> InValues, int32 Index = 0, FPCGAttributeAccessorKeysPoints* InKeys = nullptr)
		{
			return Accessor->SetRange(InValues, Index, InKeys ? *InKeys : *Keys, PCGEX_AAFLAG);
		}

		bool SetRange(TArray<T>& InValues, int32 Index = 0, FPCGAttributeAccessorKeysPoints* InKeys = nullptr)
		{
			TArrayView<const T> View(InValues);
			return Accessor->SetRange(View, Index, InKeys ? *InKeys : *Keys, PCGEX_AAFLAG);
		}

		virtual ~FAttributeAccessorBase()
		{
			Flush();
		}
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FAttributeAccessor : public FAttributeAccessorBase<T>
	{
	public:
		FAttributeAccessor(const UPCGPointData* InData, FPCGMetadataAttributeBase* InAttribute, FPCGAttributeAccessorKeysPoints* InKeys)
			: FAttributeAccessorBase<T>(InData, InAttribute, InKeys)
		{
		}

		FAttributeAccessor(UPCGPointData* InData, FPCGMetadataAttributeBase* InAttribute): FAttributeAccessorBase<T>()
		{
			this->Flush();
			this->Attribute = static_cast<FPCGMetadataAttribute<T>*>(InAttribute);
			this->Accessor = MakeUnique<FPCGAttributeAccessor<T>>(this->Attribute, InData->Metadata);

			const TArrayView<FPCGPoint> View(InData->GetMutablePoints());
			this->InternalKeys = new FPCGAttributeAccessorKeysPoints(View);

			this->NumEntries = this->InternalKeys->GetNum();
			this->Keys = this->InternalKeys;
		}


		static FAttributeAccessor* FindOrCreate(
			UPCGPointData* InData, FName AttributeName,
			const T& DefaultValue = T{}, bool bAllowsInterpolation = true, bool bOverrideParent = true, bool bOverwriteIfTypeMismatch = true)
		{
			FPCGMetadataAttribute<T>* InAttribute = InData->Metadata->FindOrCreateAttribute(
				AttributeName, DefaultValue,
				bAllowsInterpolation, bOverrideParent, bOverwriteIfTypeMismatch);
			
			return new FAttributeAccessor<T>(InData, InAttribute);
		}

		static FAttributeAccessor* FindOrCreate(
			UPCGPointData* InData, FName AttributeName, FPCGAttributeAccessorKeysPoints* InKeys,
			const T& DefaultValue = T{}, bool bAllowsInterpolation = true, bool bOverrideParent = true, bool bOverwriteIfTypeMismatch = true)
		{
			FPCGMetadataAttribute<T>* InAttribute = InData->Metadata->FindOrCreateAttribute(
				AttributeName, DefaultValue,
				bAllowsInterpolation, bOverrideParent, bOverwriteIfTypeMismatch);
			
			return new FAttributeAccessor<T>(InData, InAttribute, InKeys);
		}

		static FAttributeAccessor* FindOrCreate(
			PCGExData::FPointIO& InPointIO, FName AttributeName,
			const T& DefaultValue = T{}, bool bAllowsInterpolation = true, bool bOverrideParent = true, bool bOverwriteIfTypeMismatch = true)
		{
			UPCGPointData* InData = InPointIO.GetOut();
			FPCGMetadataAttribute<T>* InAttribute = InData->Metadata->FindOrCreateAttribute(
				AttributeName, DefaultValue,
				bAllowsInterpolation, bOverrideParent, bOverwriteIfTypeMismatch);
			
			return new FAttributeAccessor<T>(InData, InAttribute, InPointIO.GetOutKeys());
		}
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FConstAttributeAccessor : public FAttributeAccessorBase<T>
	{
	public:
		FConstAttributeAccessor(const UPCGPointData* InData, FPCGMetadataAttributeBase* InAttribute, FPCGAttributeAccessorKeysPoints* InKeys)
			: FAttributeAccessorBase<T>(InData, InAttribute, InKeys)
		{
		}

		FConstAttributeAccessor(const UPCGPointData* InData, FPCGMetadataAttributeBase* InAttribute): FAttributeAccessorBase<T>()
		{
			this->Flush();
			this->Attribute = static_cast<FPCGMetadataAttribute<T>*>(InAttribute);
			this->Accessor = MakeUnique<FPCGAttributeAccessor<T>>(this->Attribute, InData->Metadata);

			this->InternalKeys = new FPCGAttributeAccessorKeysPoints(InData->GetPoints());

			this->NumEntries = this->InternalKeys->GetNum();
			this->Keys = this->InternalKeys;
		}
	};

#undef PCGEX_AAFLAG

#pragma endregion

	template <typename T>
	static FPCGMetadataAttribute<T>* TryGetAttribute(UPCGSpatialData* InData, FName Name, bool bEnabled, T defaultValue = T{})
	{
		if (!InData->Metadata) { return nullptr; }
		if (!bEnabled || !FPCGMetadataAttributeBase::IsValidName(Name)) { return nullptr; }
		return InData->Metadata->FindOrCreateAttribute<T>(Name, defaultValue);
	}

#pragma region Local Attribute Inputs

	template <typename T>
	struct PCGEXTENDEDTOOLKIT_API FAttributeGetter
	{
	public:
		virtual ~FAttributeGetter() = default;

		bool bEnabled = true;
		bool bValid = false;

		FPCGExInputDescriptor Descriptor;

		/**
		 * Build and validate a property/attribute accessor for the selected
		 * @param PointData 
		 */
		bool Validate(const UPCGPointData* PointData)
		{
			bValid = false;
			if (!bEnabled) { return false; }
			bValid = Descriptor.Validate(PointData);
			return bValid;
		}

		bool ValidateOrCreate(const UPCGPointData* PointData)
		{
			bValid = false;
			if (!bEnabled) { return false; }
			bValid = Descriptor.Validate(PointData);

			if (!bValid && Descriptor.GetSelection() == EPCGAttributePropertySelection::Attribute)
			{
				PointData->Metadata->FindOrCreateAttribute<T>(Descriptor.GetName(), GetDefaultValue());
				bValid = Descriptor.Validate(PointData);
			}

			return bValid;
		}

		virtual T GetValueSafe(const FPCGPoint& Point, T fallback) const
		{
			if (!bValid || !bEnabled) { return fallback; }
			return GetValue(Point);
		}

		virtual T GetValue(const FPCGPoint& Point) const
		{
			if (!bValid || !bEnabled) { return GetDefaultValue(); }

			switch (Descriptor.GetSelection())
			{
			case EPCGAttributePropertySelection::Attribute:
				return PCGMetadataAttribute::CallbackWithRightType(
					Descriptor.UnderlyingType,
					[&](auto DummyValue) -> T
					{
						using AttributeType = decltype(DummyValue);
						FPCGMetadataAttribute<AttributeType>* Attribute = static_cast<FPCGMetadataAttribute<AttributeType>*>(Descriptor.Attribute);
						return Convert(Attribute->GetValueFromItemKey(Point.MetadataEntry));
					});
#define PCGEX_GET_BY_ACCESSOR(_ENUM, _ACCESSOR) case _ENUM: return Convert(Point._ACCESSOR);
			case EPCGAttributePropertySelection::PointProperty:
				switch (Descriptor.Selector.GetPointProperty())
				{
				PCGEX_FOREACH_POINTPROPERTY(PCGEX_GET_BY_ACCESSOR)
				}
				break;
			case EPCGAttributePropertySelection::ExtraProperty:
				switch (Descriptor.Selector.GetExtraProperty())
				{
				PCGEX_FOREACH_POINTEXTRAPROPERTY(PCGEX_GET_BY_ACCESSOR)
				}
				break;
			}

			return GetDefaultValue();
#undef PCGEX_GET_BY_ACCESSOR
		}

		bool SetValue(const FPCGPoint& Point, T Value) const
		{
			if (!bValid || !bEnabled) { return false; }

			switch (Descriptor.GetSelection())
			{
			case EPCGAttributePropertySelection::Attribute:
				FPCGMetadataAttribute<T>* Attribute = static_cast<FPCGMetadataAttribute<T>*>(Descriptor.Attribute);
				if (Attribute) { Attribute.SetValue(Point.MetadataEntry, Value); }
				else { return false; }
#define PCGEX_SET_BY_ACCESSOR(_ENUM, _ACCESSOR) case _ENUM: Point._ACCESSOR = Value;
			case EPCGAttributePropertySelection::PointProperty:
				switch (Descriptor.Selector.GetPointProperty())
				{
				PCGEX_FOREACH_POINTPROPERTY(PCGEX_SET_BY_ACCESSOR)
				}
				break;
			case EPCGAttributePropertySelection::ExtraProperty:
				switch (Descriptor.Selector.GetExtraProperty())
				{
				PCGEX_FOREACH_POINTEXTRAPROPERTY(PCGEX_SET_BY_ACCESSOR)
				}
				break;
			}

			return true;
#undef PCGEX_SET_BY_ACCESSOR
		}

	protected:
		virtual T GetDefaultValue() const = 0;

#define  PCGEX_PRINT_VIRTUAL(_TYPE, _NAME, ...) virtual T Convert(const _TYPE Value) const { return GetDefaultValue(); };
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_PRINT_VIRTUAL)
	};


#define PCGEX_SINGLE(_NAME, _TYPE)\
struct PCGEXTENDEDTOOLKIT_API FLocal ## _NAME ## Input : public FAttributeGetter<_TYPE>	{\
protected: \
virtual _TYPE GetDefaultValue() const override{ return 0; }\
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
struct PCGEXTENDEDTOOLKIT_API FLocal ## _NAME ## Input : public FAttributeGetter<_TYPE>	{\
protected: \
virtual _TYPE GetDefaultValue() const override { return _TYPE(0); }\
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
struct PCGEXTENDEDTOOLKIT_API FLocal ## _NAME ## Input : public FAttributeGetter<_TYPE>	{\
protected: \
virtual _TYPE GetDefaultValue() const override { return _TYPE(""); }\
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

	struct PCGEXTENDEDTOOLKIT_API FLocalSingleFieldGetter : public FAttributeGetter<double>
	{
		FLocalSingleFieldGetter()
		{
		}

		FLocalSingleFieldGetter(
			EPCGExSingleField InField,
			EPCGExAxis InAxis)
		{
			Field = InField;
			Axis = InAxis;
		}

	public:
		EPCGExSingleField Field = EPCGExSingleField::X;
		EPCGExAxis Axis = EPCGExAxis::Forward;

		void Capture(const FPCGExInputDescriptorWithSingleField& InDescriptor);
		void Capture(const FPCGExInputDescriptorGeneric& InDescriptor);

	protected:
		virtual double GetDefaultValue() const override { return 0; }

		virtual double Convert(const int32 Value) const override { return Value; }
		virtual double Convert(const int64 Value) const override { return static_cast<double>(Value); }
		virtual double Convert(const float Value) const override { return Value; }
		virtual double Convert(const double Value) const override { return Value; }

		virtual double Convert(const FVector2D Value) const override
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
			}
		}

		virtual double Convert(const FVector Value) const override
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
			}
		}

		virtual double Convert(const FVector4 Value) const override
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
			}
		}

		virtual double Convert(const FQuat Value) const override { return Convert(GetDirection(Value, Axis)); }
		virtual double Convert(const FTransform Value) const override { return Convert(Value.GetLocation()); }
		virtual double Convert(const bool Value) const override { return Value; }
		virtual double Convert(const FRotator Value) const override { return Convert(Value.Vector()); }
		virtual double Convert(const FString Value) const override { return PCGExMath::ConvertStringToDouble(Value); }
		virtual double Convert(const FName Value) const override { return PCGExMath::ConvertStringToDouble(Value.ToString()); }
	};

	struct PCGEXTENDEDTOOLKIT_API FLocalDirectionGetter : public FAttributeGetter<FVector>
	{
		FLocalDirectionGetter()
		{
		}

		FLocalDirectionGetter(
			EPCGExAxis InAxis)
		{
			Axis = InAxis;
		}

	public:
		EPCGExAxis Axis = EPCGExAxis::Forward;

		void Capture(const FPCGExInputDescriptorWithDirection& InDescriptor)
		{
			Descriptor = static_cast<FPCGExInputDescriptor>(InDescriptor);
			Axis = InDescriptor.Axis;
		}

		void Capture(const FPCGExInputDescriptorGeneric& InDescriptor)
		{
			Descriptor = static_cast<FPCGExInputDescriptor>(InDescriptor);
			Axis = InDescriptor.Axis;
		}

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
		virtual FVector Convert(const FQuat Value) const override { return GetDirection(Value, Axis); }
		virtual FVector Convert(const FTransform Value) const override { return GetDirection(Value.GetRotation(), Axis); }
		virtual FVector Convert(const FRotator Value) const override { return Value.Vector(); }
		virtual FVector Convert(const FString Value) const override { return GetDefaultValue(); }
		virtual FVector Convert(const FName Value) const override { return GetDefaultValue(); }
	};

	struct PCGEXTENDEDTOOLKIT_API FLocalToStringGetter : public FAttributeGetter<FString>
	{
		FLocalToStringGetter()
		{
		}

	public:

	protected:
		virtual FString GetDefaultValue() const override { return ""; }

		virtual FString Convert(const bool Value) const override { return FString::Printf(TEXT("%s"), Value ? TEXT("true") : TEXT("false")); }
		virtual FString Convert(const int32 Value) const override { return FString::Printf(TEXT("%d"), Value); }
		virtual FString Convert(const int64 Value) const override { return FString::Printf(TEXT("%lld"), Value); }
		virtual FString Convert(const float Value) const override { return FString::Printf(TEXT("%f"), Value); }
		virtual FString Convert(const double Value) const override { return FString::Printf(TEXT("%lf"), Value); }
		virtual FString Convert(const FVector2D Value) const override { return FString::Printf(TEXT("%s"), *Value.ToString()); }
		virtual FString Convert(const FVector Value) const override { return FString::Printf(TEXT("%s"), *Value.ToString()); }
		virtual FString Convert(const FVector4 Value) const override { return FString::Printf(TEXT("%s"), *Value.ToString()); }
		virtual FString Convert(const FQuat Value) const override { return FString::Printf(TEXT("%s"), *Value.ToString()); }
		virtual FString Convert(const FTransform Value) const override { return FString::Printf(TEXT("%s"), *Value.ToString()); }
		virtual FString Convert(const FRotator Value) const override { return FString::Printf(TEXT("%s"), *Value.ToString()); }
		virtual FString Convert(const FString Value) const override { return FString::Printf(TEXT("%s"), *Value); }
		virtual FString Convert(const FName Value) const override { return FString::Printf(TEXT("%s"), *Value.ToString()); }
	};

#pragma endregion
}

#undef PCGEX_ATTRIBUTE_RETURN
#undef PCGEX_ATTRIBUTE
