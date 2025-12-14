// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGEx.h"
#include "PCGExTypeOpsImpl.h"
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
	// Forward declarations
	class ISubSelectorOps;

	/**
	 * Single field selection identifiers
	 */
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

	/**
	 * Transform component parts
	 */
	enum class ETransformPart : uint8
	{
		Position = 0,
		Rotation = 1,
		Scale    = 2,
	};

#pragma region Field helpers

	using FInputSelectorComponentData = TTuple<ETransformPart, EPCGMetadataTypes>;
	static const TMap<FString, FInputSelectorComponentData> STRMAP_TRANSFORM_FIELD = {
		{TEXT("POSITION"), FInputSelectorComponentData{ETransformPart::Position, EPCGMetadataTypes::Vector}},
		{TEXT("POS"), FInputSelectorComponentData{ETransformPart::Position, EPCGMetadataTypes::Vector}},
		{TEXT("ROTATION"), FInputSelectorComponentData{ETransformPart::Rotation, EPCGMetadataTypes::Quaternion}},
		{TEXT("ROT"), FInputSelectorComponentData{ETransformPart::Rotation, EPCGMetadataTypes::Quaternion}},
		{TEXT("ORIENT"), FInputSelectorComponentData{ETransformPart::Rotation, EPCGMetadataTypes::Quaternion}},
		{TEXT("SCALE"), FInputSelectorComponentData{ETransformPart::Scale, EPCGMetadataTypes::Vector}},
	};

	using FInputSelectorFieldData = TTuple<ESingleField, EPCGMetadataTypes, int32>;
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

	/**
	 * FSubSelection - Sub-selection configuration and type-erased operations
	 * 
	 * This struct stores the configuration for selecting sub-components of values
	 * (like extracting .X from a vector, or Position from a Transform) and provides
	 * type-erased methods for applying the selection.
	 * 
	 * The actual operations are delegated to ISubSelectorOps instances via
	 * FSubSelectorRegistry, eliminating the need for template instantiations
	 * at the call site.
	 * 
	 * Usage:
	 *   FSubSelection Sub(Selector);  // Parse selection from attribute path
	 *   
	 *   // Type-erased get (recommended)
	 *   double Result;
	 *   EPCGMetadataTypes ResultType;
	 *   Sub.GetVoid(SourceType, &SourceValue, &Result, ResultType);
	 *   
	 *   // Or for full extraction to working type
	 *   alignas(16) uint8 Buffer[96];
	 *   Sub.ApplyGet(SourceType, &SourceValue, Buffer, ResultType);
	 */
	struct PCGEXTENDEDTOOLKIT_API FSubSelection
	{
		// Configuration flags
		bool bIsValid = false;
		bool bIsAxisSet = false;
		bool bIsFieldSet = false;
		bool bIsComponentSet = false;

		// Selection parameters
		ETransformPart Component = ETransformPart::Position;
		EPCGExAxis Axis = EPCGExAxis::Forward;
		ESingleField Field = ESingleField::X;
		EPCGMetadataTypes PossibleSourceType = EPCGMetadataTypes::Unknown;

		// Constructors
		FSubSelection() = default;
		explicit FSubSelection(const TArray<FString>& ExtraNames);
		explicit FSubSelection(const FPCGAttributePropertyInputSelector& InSelector);
		explicit FSubSelection(const FString& Path, const UPCGData* InData = nullptr);

		/**
		 * Get the resulting type when this sub-selection is applied
		 * 
		 * - Field selection → Double
		 * - Axis selection → Vector  
		 * - Component selection → Vector (Position/Scale) or Quaternion (Rotation)
		 * - No selection → Fallback (original type)
		 */
		EPCGMetadataTypes GetSubType(EPCGMetadataTypes Fallback = EPCGMetadataTypes::Unknown) const;

		void SetComponent(const ETransformPart InComponent);
		bool SetFieldIndex(const int32 InFieldIndex);

	protected:
		void Init(const TArray<FString>& ExtraNames);

	public:

		//
		// Type-Erased Interface (Primary API)
		//
		// These methods delegate to ISubSelectorOps via FSubSelectorRegistry.
		// No template instantiation required at call sites.
		//

		/**
		 * Apply sub-selection when reading a value (Get)
		 * 
		 * Extracts the selected sub-component from the source and writes it
		 * to the output buffer. The output type depends on the selection:
		 * - Field → Double
		 * - Axis → Vector
		 * - Component → Vector or Quaternion
		 * - None → Same as source
		 * 
		 * @param SourceType Type of the source value
		 * @param Source Pointer to source value
		 * @param OutValue Pointer to output buffer (must be large enough for any type, recommend 96 bytes)
		 * @param OutType Receives the type of the output value
		 */
		void ApplyGet(EPCGMetadataTypes SourceType, const void* Source, void* OutValue, EPCGMetadataTypes& OutType) const;

		/**
		 * Apply sub-selection when writing a value (Set)
		 * 
		 * Sets the selected sub-component of the target from the source value.
		 * Handles conversion from source type to appropriate sub-component type.
		 * 
		 * @param TargetType Type of the target value
		 * @param Target Pointer to target value (modified in place)
		 * @param SourceType Type of the source value  
		 * @param Source Pointer to source value
		 */
		void ApplySet(EPCGMetadataTypes TargetType, void* Target, EPCGMetadataTypes SourceType, const void* Source) const;

		/**
		 * Extract a field value to double
		 * 
		 * Convenience method for common case of extracting a scalar field.
		 * Returns 0.0 if the type doesn't support field extraction.
		 * 
		 * @param SourceType Type of the source value
		 * @param Source Pointer to source value
		 * @return Extracted field value as double
		 */
		double ExtractFieldToDouble(EPCGMetadataTypes SourceType, const void* Source) const;

		/**
		 * Inject a double value into a field
		 * 
		 * Convenience method for setting a single scalar field.
		 * No-op if the type doesn't support field injection.
		 * 
		 * @param TargetType Type of the target value
		 * @param Target Pointer to target value (modified in place)
		 * @param Value Double value to inject
		 */
		void InjectFieldFromDouble(EPCGMetadataTypes TargetType, void* Target, double Value) const;

		//
		// Legacy Type-Erased Interface (for compatibility)
		//
		// These match the existing GetVoid/SetVoid signatures but internally
		// use the new system.
		//

		/**
		 * Type-erased Get with explicit working type
		 * (Legacy compatibility wrapper)
		 */
		void GetVoid(EPCGMetadataTypes SourceType, const void* Source, EPCGMetadataTypes WorkingType, void* Target) const;

		/**
		 * Type-erased Set with explicit types
		 * (Legacy compatibility wrapper)
		 */
		void SetVoid(EPCGMetadataTypes TargetType, void* Target, EPCGMetadataTypes SourceType, const void* Source) const;

		//
		// Templated Interface (for convenience, uses type-erased internally)
		//
		// These templates are thin wrappers that call the type-erased methods.
		// They do NOT create 14×14 instantiations - they just provide type safety
		// and call the void* versions.
		//

		template <typename TSource, typename TResult>
		TResult Get(const TSource& Value) const;

		template <typename TTarget, typename TSource>
		void Set(TTarget& Target, const TSource& Value) const;
	};

	//
	// Template implementations - delegate to type-erased path
	//

	template <typename TSource, typename TResult>
	TResult FSubSelection::Get(const TSource& Value) const
	{
		TResult Result{};

		constexpr EPCGMetadataTypes SourceType = PCGExTypeOps::TTypeToMetadata<TSource>::Type;
		constexpr EPCGMetadataTypes ResultType = PCGExTypeOps::TTypeToMetadata<TResult>::Type;

		GetVoid(SourceType, &Value, ResultType, &Result);

		return Result;
	}

	template <typename TTarget, typename TSource>
	void FSubSelection::Set(TTarget& Target, const TSource& Value) const
	{
		constexpr EPCGMetadataTypes TargetType = PCGExTypeOps::TTypeToMetadata<TTarget>::Type;
		constexpr EPCGMetadataTypes SourceType = PCGExTypeOps::TTypeToMetadata<TSource>::Type;

		SetVoid(TargetType, &Target, SourceType, &Value);
	}

	PCGEXTENDEDTOOLKIT_API bool TryGetType(const FPCGAttributePropertyInputSelector& InputSelector, const UPCGData* InData, EPCGMetadataTypes& OutType);
	PCGEXTENDEDTOOLKIT_API bool TryGetTypeAndSource(const FPCGAttributePropertyInputSelector& InputSelector, const TSharedPtr<PCGExData::FFacade>& InDataFacade, EPCGMetadataTypes& OutType, PCGExData::EIOSide& InOutSide);
	PCGEXTENDEDTOOLKIT_API bool TryGetTypeAndSource(const FName AttributeName, const TSharedPtr<PCGExData::FFacade>& InDataFacade, EPCGMetadataTypes& OutType, PCGExData::EIOSide& InOutSource);
}
