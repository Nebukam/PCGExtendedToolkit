// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "PCGEx.h"
#include "Details/PCGExMacros.h"
#include "Details/PCGExDetailsAxis.h"
#include "Metadata/PCGMetadataAttributeTraits.h"

class UPCGData;
struct FPCGAttributePropertyInputSelector;

namespace PCGExData
{
	class FPointIO;
	enum class EIOSide : uint8;
	class FFacade;
}

namespace PCGEx
{
	enum class ESingleField : uint8
	{
		X             = 0,
		Y             = 1,
		Z             = 2,
		W             = 3,
		Length        = 4,
		SquaredLength = 5,
		Volume        = 6,
		Sum           = 7,
	};

	enum class ETransformPart : uint8
	{
		Position = 0,
		Rotation = 1,
		Scale    = 2,
	};

#pragma region Field helpers

	using FInputSelectorComponentData = TTuple<ETransformPart, EPCGMetadataTypes>;
	// Transform component, root type
	static const TMap<FString, FInputSelectorComponentData> STRMAP_TRANSFORM_FIELD = {
		{TEXT("POSITION"), FInputSelectorComponentData{ETransformPart::Position, EPCGMetadataTypes::Vector}},
		{TEXT("POS"), FInputSelectorComponentData{ETransformPart::Position, EPCGMetadataTypes::Vector}},
		{TEXT("ROTATION"), FInputSelectorComponentData{ETransformPart::Rotation, EPCGMetadataTypes::Quaternion}},
		{TEXT("ROT"), FInputSelectorComponentData{ETransformPart::Rotation, EPCGMetadataTypes::Quaternion}},
		{TEXT("ORIENT"), FInputSelectorComponentData{ETransformPart::Rotation, EPCGMetadataTypes::Quaternion}},
		{TEXT("SCALE"), FInputSelectorComponentData{ETransformPart::Scale, EPCGMetadataTypes::Vector}},
	};

	using FInputSelectorFieldData = TTuple<ESingleField, EPCGMetadataTypes, int32>;
	// Single component, root type
	static const TMap<FString, FInputSelectorFieldData> STRMAP_SINGLE_FIELD = {
		{TEXT("X"), FInputSelectorFieldData{ESingleField::X, EPCGMetadataTypes::Vector, 0}},
		{TEXT("R"), FInputSelectorFieldData{ESingleField::X, EPCGMetadataTypes::Quaternion, 0}},
		{TEXT("ROLL"), FInputSelectorFieldData{ESingleField::X, EPCGMetadataTypes::Quaternion, 0}},
		{TEXT("RX"), FInputSelectorFieldData{ESingleField::X, EPCGMetadataTypes::Quaternion, 0}},
		{TEXT("Y"), FInputSelectorFieldData{ESingleField::Y, EPCGMetadataTypes::Vector, 1}},
		{TEXT("G"), FInputSelectorFieldData{ESingleField::Y, EPCGMetadataTypes::Vector4, 1}},
		{TEXT("YAW"), FInputSelectorFieldData{ESingleField::Y, EPCGMetadataTypes::Quaternion, 1}},
		{TEXT("RY"), FInputSelectorFieldData{ESingleField::Y, EPCGMetadataTypes::Quaternion, 1}},
		{TEXT("Z"), FInputSelectorFieldData{ESingleField::Z, EPCGMetadataTypes::Vector, 2}},
		{TEXT("B"), FInputSelectorFieldData{ESingleField::Z, EPCGMetadataTypes::Vector4, 2}},
		{TEXT("P"), FInputSelectorFieldData{ESingleField::Z, EPCGMetadataTypes::Quaternion, 2}},
		{TEXT("PITCH"), FInputSelectorFieldData{ESingleField::Z, EPCGMetadataTypes::Quaternion, 2}},
		{TEXT("RZ"), FInputSelectorFieldData{ESingleField::Z, EPCGMetadataTypes::Quaternion, 2}},
		{TEXT("W"), FInputSelectorFieldData{ESingleField::W, EPCGMetadataTypes::Vector4, 3}},
		{TEXT("A"), FInputSelectorFieldData{ESingleField::W, EPCGMetadataTypes::Vector4, 3}},
		{TEXT("L"), FInputSelectorFieldData{ESingleField::Length, EPCGMetadataTypes::Vector, 0}},
		{TEXT("LEN"), FInputSelectorFieldData{ESingleField::Length, EPCGMetadataTypes::Vector, 0}},
		{TEXT("LENGTH"), FInputSelectorFieldData{ESingleField::Length, EPCGMetadataTypes::Vector, 0}},
		{TEXT("SQUAREDLENGTH"), FInputSelectorFieldData{ESingleField::SquaredLength, EPCGMetadataTypes::Vector, 0}},
		{TEXT("LENSQR"), FInputSelectorFieldData{ESingleField::SquaredLength, EPCGMetadataTypes::Vector, 0}},
		{TEXT("VOL"), FInputSelectorFieldData{ESingleField::Volume, EPCGMetadataTypes::Vector, 0}},
		{TEXT("VOLUME"), FInputSelectorFieldData{ESingleField::Volume, EPCGMetadataTypes::Vector, 0}},
		{TEXT("SUM"), FInputSelectorFieldData{ESingleField::Sum, EPCGMetadataTypes::Vector, 0}},
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

		ETransformPart Component = ETransformPart::Position;
		EPCGExAxis Axis = EPCGExAxis::Forward;
		ESingleField Field = ESingleField::X;
		EPCGMetadataTypes PossibleSourceType = EPCGMetadataTypes::Unknown;
		int32 FieldIndex = 0;

		FSubSelection() = default;
		explicit FSubSelection(const TArray<FString>& ExtraNames);
		explicit FSubSelection(const FPCGAttributePropertyInputSelector& InSelector);
		explicit FSubSelection(const FString& Path, const UPCGData* InData = nullptr);

		EPCGMetadataTypes GetSubType(EPCGMetadataTypes Fallback = EPCGMetadataTypes::Unknown) const;
		void SetComponent(const ETransformPart InComponent);
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
extern template void FSubSelection::Set<_TYPE_A, _TYPE_B>(_TYPE_A& Target, const _TYPE_B& Value) const;
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
		void Set(const FSubSelection& SubSelection, const int32 Index, const T_VALUE& Value) { *(Values->GetData() + Index) = SubSelection.Get<T>(Value); }

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

	PCGEXTENDEDTOOLKIT_API
	bool TryGetType(
		const FPCGAttributePropertyInputSelector& InputSelector,
		const UPCGData* InData, EPCGMetadataTypes& OutType);

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
}
