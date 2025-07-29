// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "PCGEx.h"

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

	template <typename T>
	T ConvertFromBoolean(const bool& Value);

	template <typename T>
	T ConvertFromInteger32(const int32& Value);

	template <typename T>
	T ConvertFromInteger64(const int64& Value);

	template <typename T>
	T ConvertFromFloat(const float& Value);

	template <typename T>
	T ConvertFromDouble(const double& Value);

	template <typename T>
	T ConvertFromVector2(const FVector2D& Value);

	template <typename T>
	T ConvertFromVector(const FVector& Value);

	template <typename T>
	T ConvertFromVector4(const FVector4& Value);

	template <typename T>
	T ConvertFromQuaternion(const FQuat& Value);

	template <typename T>
	T ConvertFromRotator(const FRotator& Value);

	template <typename T>
	T ConvertFromTransform(const FTransform& Value);

	template <typename T>
	T ConvertFromString(const FString& Value);

	template <typename T>
	T ConvertFromName(const FName& Value);

	template <typename T>
	T ConvertFromSoftClassPath(const FSoftClassPath& Value);

	template <typename T>
	T ConvertFromSoftObjectPath(const FSoftObjectPath& Value);

	template <typename T_VALUE, typename T>
	T Convert(const T_VALUE& Value);

#define PCGEX_TPL(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) \
extern template _TYPE_B ConvertFrom##_NAME_A<_TYPE_B>(const _TYPE_A& Value); \
extern template _TYPE_B Convert<_TYPE_A, _TYPE_B>(const _TYPE_A& Value);
	PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_TPL)
#undef PCGEX_TPL

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
		T Get(const T_VALUE& Value) const;

		// Set component subselection inside Target from provided value
		template <typename T, typename T_VALUE>
		void Set(T& Target, const T_VALUE& Value) const;

	};

#define PCGEX_TPL(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) \
extern template _TYPE_A FSubSelection::Get<_TYPE_B, _TYPE_A>(const _TYPE_B&) const; \
extern template void FSubSelection::Set(_TYPE_A& Target, const _TYPE_B& Value) const;
	PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_TPL)
#undef PCGEX_TPL

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
			const FSoftObjectPath Path = S.Get<T, FSoftObjectPath>(InValue);

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

#define PCGEX_TRY_SET_FPROPERTY(_PTYPE, _TYPE) if(InProperty->IsA<_PTYPE>()){ _PTYPE* P = CastField<_PTYPE>(InProperty); P->SetPropertyValue_InContainer(InContainer, S.Get<T, _TYPE>(InValue)); return true;}
		PCGEX_FOREACH_SUPPORTEDFPROPERTY(PCGEX_TRY_SET_FPROPERTY)
#undef PCGEX_TRY_SET_FPROPERTY

		if (FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
		{
#define PCGEX_TRY_SET_FSTRUCT(_PTYPE, _TYPE) if (StructProperty->Struct == TBaseStructure<_TYPE>::Get()){ void* StructContainer = StructProperty->ContainerPtrToValuePtr<void>(InContainer); *reinterpret_cast<_TYPE*>(StructContainer) = S.Get<T, _TYPE>(InValue); return true; }
			PCGEX_FOREACH_SUPPORTEDFSTRUCT(PCGEX_TRY_SET_FSTRUCT)
#undef PCGEX_TRY_SET_FSTRUCT

			if (StructProperty->Struct == TBaseStructure<FPCGAttributePropertyInputSelector>::Get())
			{
				FPCGAttributePropertyInputSelector NewSelector = FPCGAttributePropertyInputSelector();
				NewSelector.Update(S.Get<T, FString>(InValue));
				void* StructContainer = StructProperty->ContainerPtrToValuePtr<void>(InContainer);
				*reinterpret_cast<FPCGAttributePropertyInputSelector*>(StructContainer) = NewSelector;
				return true;
			}
		}

		return false;
	}
}
