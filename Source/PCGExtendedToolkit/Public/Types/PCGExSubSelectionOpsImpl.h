// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubSelectionOps.h"
#include "PCGExMath.h"
#include "Types/PCGExTypeOpsImpl.h"

/**
 * PCGEx Sub-Selection Operations Implementation
 * 
 * Contains the templated TSubSelectorOpsImpl<T> that bridges the type-specific
 * operations to the ISubSelectorOps interface.
 */

namespace PCGEx
{
	/**
	 * Field extraction functions - static implementations per type
	 * 
	 * These are the actual extraction implementations. Only 14 of each function
	 * exist (one per type), not 14×14 combinations.
	 */
	namespace SubSelectionFunctions
	{
		//
		// Scalar types - identity extraction
		//

		template <typename T>
		FORCEINLINE typename TEnableIf<TIsArithmetic<T>::Value, double>::Type ExtractFieldNumeric(const T& Value, ESingleField /*Field*/)
		{
			return static_cast<double>(Value);
		}

		template <typename T>
		FORCEINLINE typename TEnableIf<TIsArithmetic<T>::Value, void>::Type InjectFieldNumeric(T& Target, double Value, ESingleField /*Field*/)
		{
			Target = static_cast<T>(Value);
		}

		//
		// FVector2D
		//

		FORCEINLINE double ExtractFieldVector2D(const FVector2D& Value, ESingleField Field)
		{
			switch (Field)
			{
			case ESingleField::X: return Value.X;
			case ESingleField::Y: return Value.Y;
			case ESingleField::Z:
			case ESingleField::W: return 0.0;
			case ESingleField::Length: return Value.Length();
			case ESingleField::SquaredLength: return Value.SquaredLength();
			case ESingleField::Volume: return Value.X * Value.Y;
			case ESingleField::Sum: return Value.X + Value.Y;
			default: return Value.X;
			}
		}

		FORCEINLINE void InjectFieldVector2D(FVector2D& Target, double Value, ESingleField Field)
		{
			switch (Field)
			{
			case ESingleField::X: Target.X = Value;
				break;
			case ESingleField::Y: Target.Y = Value;
				break;
			case ESingleField::Length: Target = Target.GetSafeNormal() * Value;
				break;
			case ESingleField::SquaredLength: Target = Target.GetSafeNormal() * FMath::Sqrt(Value);
				break;
			default: break;
			}
		}

		//
		// FVector
		//

		FORCEINLINE double ExtractFieldVector(const FVector& Value, ESingleField Field)
		{
			switch (Field)
			{
			case ESingleField::X: return Value.X;
			case ESingleField::Y: return Value.Y;
			case ESingleField::Z: return Value.Z;
			case ESingleField::W: return 0.0;
			case ESingleField::Length: return Value.Length();
			case ESingleField::SquaredLength: return Value.SquaredLength();
			case ESingleField::Volume: return Value.X * Value.Y * Value.Z;
			case ESingleField::Sum: return Value.X + Value.Y + Value.Z;
			default: return Value.X;
			}
		}

		FORCEINLINE void InjectFieldVector(FVector& Target, double Value, ESingleField Field)
		{
			switch (Field)
			{
			case ESingleField::X: Target.X = Value;
				break;
			case ESingleField::Y: Target.Y = Value;
				break;
			case ESingleField::Z: Target.Z = Value;
				break;
			case ESingleField::Length: Target = Target.GetSafeNormal() * Value;
				break;
			case ESingleField::SquaredLength: Target = Target.GetSafeNormal() * FMath::Sqrt(Value);
				break;
			default: break;
			}
		}

		//
		// FVector4
		//

		FORCEINLINE double ExtractFieldVector4(const FVector4& Value, ESingleField Field)
		{
			switch (Field)
			{
			case ESingleField::X: return Value.X;
			case ESingleField::Y: return Value.Y;
			case ESingleField::Z: return Value.Z;
			case ESingleField::W: return Value.W;
			case ESingleField::Length: return FVector(Value.X, Value.Y, Value.Z).Length();
			case ESingleField::SquaredLength: return FVector(Value.X, Value.Y, Value.Z).SquaredLength();
			case ESingleField::Volume: return Value.X * Value.Y * Value.Z * Value.W;
			case ESingleField::Sum: return Value.X + Value.Y + Value.Z + Value.W;
			default: return Value.X;
			}
		}

		FORCEINLINE void InjectFieldVector4(FVector4& Target, double Value, ESingleField Field)
		{
			switch (Field)
			{
			case ESingleField::X: Target.X = Value;
				break;
			case ESingleField::Y: Target.Y = Value;
				break;
			case ESingleField::Z: Target.Z = Value;
				break;
			case ESingleField::W: Target.W = Value;
				break;
			case ESingleField::Length:
				{
					FVector V(Target.X, Target.Y, Target.Z);
					V = V.GetSafeNormal() * Value;
					Target = FVector4(V.X, V.Y, V.Z, Target.W);
				}
				break;
			case ESingleField::SquaredLength:
				{
					FVector V(Target.X, Target.Y, Target.Z);
					V = V.GetSafeNormal() * FMath::Sqrt(Value);
					Target = FVector4(V.X, V.Y, V.Z, Target.W);
				}
				break;
			default: break;
			}
		}

		//
		// FRotator
		//

		FORCEINLINE double ExtractFieldRotator(const FRotator& Value, ESingleField Field)
		{
			switch (Field)
			{
			case ESingleField::X: return Value.Roll;
			case ESingleField::Y: return Value.Yaw;
			case ESingleField::Z: return Value.Pitch;
			default: return 0.0;
			}
		}

		FORCEINLINE void InjectFieldRotator(FRotator& Target, double Value, ESingleField Field)
		{
			switch (Field)
			{
			case ESingleField::X: Target.Roll = Value;
				break;
			case ESingleField::Y: Target.Yaw = Value;
				break;
			case ESingleField::Z: Target.Pitch = Value;
				break;
			case ESingleField::Length: Target = Target.GetNormalized() * Value;
				break;
			case ESingleField::SquaredLength: Target = Target.GetNormalized() * FMath::Sqrt(Value);
				break;
			default: break;
			}
		}

		//
		// FQuat (extracts via Rotator)
		//

		FORCEINLINE double ExtractFieldQuat(const FQuat& Value, ESingleField Field)
		{
			const FRotator R = Value.Rotator();
			return ExtractFieldRotator(R, Field);
		}

		FORCEINLINE void InjectFieldQuat(FQuat& Target, double Value, ESingleField Field)
		{
			FRotator R = Target.Rotator();
			InjectFieldRotator(R, Value, Field);
			Target = R.Quaternion();
		}

		//
		// Axis extraction
		//

		FORCEINLINE FVector ExtractAxisFromQuat(const FQuat& Rotation, EPCGExAxis Axis)
		{
			return PCGExMath::GetDirection(Rotation, Axis);
		}

		FORCEINLINE FVector ExtractAxisFromRotator(const FRotator& Rotation, EPCGExAxis Axis)
		{
			return PCGExMath::GetDirection(Rotation.Quaternion(), Axis);
		}

		//
		// Transform component operations
		//

		FORCEINLINE void ExtractTransformComponent(const FTransform& Transform, ETransformPart Part, void* OutValue, EPCGMetadataTypes& OutType)
		{
			switch (Part)
			{
			case ETransformPart::Position: *static_cast<FVector*>(OutValue) = Transform.GetLocation();
				OutType = EPCGMetadataTypes::Vector;
				break;
			case ETransformPart::Rotation: *static_cast<FQuat*>(OutValue) = Transform.GetRotation();
				OutType = EPCGMetadataTypes::Quaternion;
				break;
			case ETransformPart::Scale: *static_cast<FVector*>(OutValue) = Transform.GetScale3D();
				OutType = EPCGMetadataTypes::Vector;
				break;
			}
		}

		FORCEINLINE void InjectTransformComponent(FTransform& Transform, ETransformPart Part, const void* Value, EPCGMetadataTypes ValueType)
		{
			switch (Part)
			{
			case ETransformPart::Position: if (ValueType == EPCGMetadataTypes::Vector)
				{
					Transform.SetLocation(*static_cast<const FVector*>(Value));
				}
				break;
			case ETransformPart::Rotation: if (ValueType == EPCGMetadataTypes::Quaternion)
				{
					Transform.SetRotation(*static_cast<const FQuat*>(Value));
				}
				else if (ValueType == EPCGMetadataTypes::Rotator)
				{
					Transform.SetRotation(static_cast<const FRotator*>(Value)->Quaternion());
				}
				break;
			case ETransformPart::Scale: if (ValueType == EPCGMetadataTypes::Vector)
				{
					Transform.SetScale3D(*static_cast<const FVector*>(Value));
				}
				break;
			}
		}
	} // namespace SubSelectionFunctions

	/**
	 * TSubSelectorOpsImpl<T> - Per-type implementation of ISubSelectorOps
	 * 
	 * Only 14 instantiations exist (one per supported type).
	 * All sub-selection logic is contained here, eliminating the need for
	 * 14×14 template combinations.
	 */
	template <typename T>
	class TSubSelectorOpsImpl : public ISubSelectorOps
	{
	public:
		using Traits = TSubSelectionTraits<T>;

		//~ Begin ISubSelectorOps interface

		virtual EPCGMetadataTypes GetTypeId() const override
		{
			return PCGExTypeOps::TTypeToMetadata<T>::Type;
		}

		virtual int32 GetNumFields() const override
		{
			return Traits::NumFields;
		}

		virtual bool SupportsFieldExtraction() const override
		{
			return Traits::bSupportsFieldExtraction;
		}

		virtual bool SupportsAxisExtraction() const override
		{
			return Traits::bSupportsAxisExtraction;
		}

		virtual bool SupportsComponentExtraction() const override
		{
			return Traits::bSupportsComponentExtraction;
		}

		virtual double ExtractField(const void* Value, ESingleField Field) const override
		{
			return ExtractFieldImpl(*static_cast<const T*>(Value), Field);
		}

		virtual void InjectField(void* Target, double Value, ESingleField Field) const override
		{
			InjectFieldImpl(*static_cast<T*>(Target), Value, Field);
		}

		virtual FVector ExtractAxis(const void* Value, EPCGExAxis Axis) const override
		{
			return ExtractAxisImpl(*static_cast<const T*>(Value), Axis);
		}

		virtual void ExtractComponent(const void* Transform, ETransformPart Part, void* OutValue, EPCGMetadataTypes& OutType) const override
		{
			ExtractComponentImpl(*static_cast<const T*>(Transform), Part, OutValue, OutType);
		}

		virtual void InjectComponent(void* Transform, ETransformPart Part, const void* Value, EPCGMetadataTypes ValueType) const override
		{
			InjectComponentImpl(*static_cast<T*>(Transform), Part, Value, ValueType);
		}

		virtual void ApplyGetSelection(const void* Value, const FSubSelection& Selection, void* OutValue, EPCGMetadataTypes& OutType) const override
		{
			ApplyGetSelectionImpl(*static_cast<const T*>(Value), Selection, OutValue, OutType);
		}

		virtual void ApplySetSelection(void* Target, const FSubSelection& Selection, const void* Source, EPCGMetadataTypes SourceType) const override
		{
			ApplySetSelectionImpl(*static_cast<T*>(Target), Selection, Source, SourceType);
		}

		//~ End ISubSelectorOps interface

		// Static instance accessor (for registry)
		static TSubSelectorOpsImpl<T>& GetInstance()
		{
			static TSubSelectorOpsImpl<T> Instance;
			return Instance;
		}

	private:
		//
		// Field extraction/injection - dispatches based on type
		//

		// Default: scalar types
		template <typename U = T>
		FORCEINLINE typename TEnableIf<TIsArithmetic<U>::Value || std::is_same_v<U, bool>, double>::Type ExtractFieldImpl(const U& Value, ESingleField /*Field*/) const
		{
			return static_cast<double>(Value);
		}

		template <typename U = T>
		FORCEINLINE typename TEnableIf<TIsArithmetic<U>::Value || std::is_same_v<U, bool>, void>::Type InjectFieldImpl(U& Target, double Value, ESingleField /*Field*/) const
		{
			Target = static_cast<U>(Value);
		}

		// FVector2D specialization
		FORCEINLINE double ExtractFieldImpl(const FVector2D& Value, ESingleField Field) const
		{
			return SubSelectionFunctions::ExtractFieldVector2D(Value, Field);
		}

		FORCEINLINE void InjectFieldImpl(FVector2D& Target, double Value, ESingleField Field) const
		{
			SubSelectionFunctions::InjectFieldVector2D(Target, Value, Field);
		}

		// FVector specialization
		FORCEINLINE double ExtractFieldImpl(const FVector& Value, ESingleField Field) const
		{
			return SubSelectionFunctions::ExtractFieldVector(Value, Field);
		}

		FORCEINLINE void InjectFieldImpl(FVector& Target, double Value, ESingleField Field) const
		{
			SubSelectionFunctions::InjectFieldVector(Target, Value, Field);
		}

		// FVector4 specialization
		FORCEINLINE double ExtractFieldImpl(const FVector4& Value, ESingleField Field) const
		{
			return SubSelectionFunctions::ExtractFieldVector4(Value, Field);
		}

		FORCEINLINE void InjectFieldImpl(FVector4& Target, double Value, ESingleField Field) const
		{
			SubSelectionFunctions::InjectFieldVector4(Target, Value, Field);
		}

		// FRotator specialization
		FORCEINLINE double ExtractFieldImpl(const FRotator& Value, ESingleField Field) const
		{
			return SubSelectionFunctions::ExtractFieldRotator(Value, Field);
		}

		FORCEINLINE void InjectFieldImpl(FRotator& Target, double Value, ESingleField Field) const
		{
			SubSelectionFunctions::InjectFieldRotator(Target, Value, Field);
		}

		// FQuat specialization
		FORCEINLINE double ExtractFieldImpl(const FQuat& Value, ESingleField Field) const
		{
			return SubSelectionFunctions::ExtractFieldQuat(Value, Field);
		}

		FORCEINLINE void InjectFieldImpl(FQuat& Target, double Value, ESingleField Field) const
		{
			SubSelectionFunctions::InjectFieldQuat(Target, Value, Field);
		}

		// FTransform specialization - extracts from component
		FORCEINLINE double ExtractFieldImpl(const FTransform& Value, ESingleField Field) const
		{
			// For transform, we extract from Position by default (field selection makes most sense there)
			return SubSelectionFunctions::ExtractFieldVector(Value.GetLocation(), Field);
		}

		FORCEINLINE void InjectFieldImpl(FTransform& Target, double Value, ESingleField Field) const
		{
			FVector Pos = Target.GetLocation();
			SubSelectionFunctions::InjectFieldVector(Pos, Value, Field);
			Target.SetLocation(Pos);
		}

		// String types - return 0 (no field extraction)
		FORCEINLINE double ExtractFieldImpl(const FString& /*Value*/, ESingleField /*Field*/) const { return 0.0; }
		FORCEINLINE void InjectFieldImpl(FString& /*Target*/, double /*Value*/, ESingleField /*Field*/) const
		{
		}

		FORCEINLINE double ExtractFieldImpl(const FName& /*Value*/, ESingleField /*Field*/) const { return 0.0; }
		FORCEINLINE void InjectFieldImpl(FName& /*Target*/, double /*Value*/, ESingleField /*Field*/) const
		{
		}

		FORCEINLINE double ExtractFieldImpl(const FSoftObjectPath& /*Value*/, ESingleField /*Field*/) const { return 0.0; }
		FORCEINLINE void InjectFieldImpl(FSoftObjectPath& /*Target*/, double /*Value*/, ESingleField /*Field*/) const
		{
		}

		FORCEINLINE double ExtractFieldImpl(const FSoftClassPath& /*Value*/, ESingleField /*Field*/) const { return 0.0; }
		FORCEINLINE void InjectFieldImpl(FSoftClassPath& /*Target*/, double /*Value*/, ESingleField /*Field*/) const
		{
		}

		//
		// Axis extraction - only rotation types return meaningful values
		//

		// Default: return forward
		template <typename U = T>
		FORCEINLINE typename TEnableIf<!std::is_same_v<U, FQuat> && !std::is_same_v<U, FRotator> && !std::is_same_v<U, FTransform>, FVector>::Type ExtractAxisImpl(const U& /*Value*/, EPCGExAxis /*Axis*/) const
		{
			return FVector::ForwardVector;
		}

		// FQuat
		FORCEINLINE FVector ExtractAxisImpl(const FQuat& Value, EPCGExAxis Axis) const
		{
			return SubSelectionFunctions::ExtractAxisFromQuat(Value, Axis);
		}

		// FRotator
		FORCEINLINE FVector ExtractAxisImpl(const FRotator& Value, EPCGExAxis Axis) const
		{
			return SubSelectionFunctions::ExtractAxisFromRotator(Value, Axis);
		}

		// FTransform
		FORCEINLINE FVector ExtractAxisImpl(const FTransform& Value, EPCGExAxis Axis) const
		{
			return SubSelectionFunctions::ExtractAxisFromQuat(Value.GetRotation(), Axis);
		}

		//
		// Component extraction - only FTransform uses this
		//

		// Default: no-op
		template <typename U = T>
		FORCEINLINE typename TEnableIf<!std::is_same_v<U, FTransform>, void>::Type ExtractComponentImpl(const U& /*Value*/, ETransformPart /*Part*/, void* /*OutValue*/, EPCGMetadataTypes& OutType) const
		{
			OutType = EPCGMetadataTypes::Unknown;
		}

		template <typename U = T>
		FORCEINLINE typename TEnableIf<!std::is_same_v<U, FTransform>, void>::Type InjectComponentImpl(U& /*Target*/, ETransformPart /*Part*/, const void* /*Value*/, EPCGMetadataTypes /*ValueType*/) const
		{
		}

		// FTransform specializations
		FORCEINLINE void ExtractComponentImpl(const FTransform& Value, ETransformPart Part, void* OutValue, EPCGMetadataTypes& OutType) const
		{
			SubSelectionFunctions::ExtractTransformComponent(Value, Part, OutValue, OutType);
		}

		FORCEINLINE void InjectComponentImpl(FTransform& Target, ETransformPart Part, const void* Value, EPCGMetadataTypes ValueType) const
		{
			SubSelectionFunctions::InjectTransformComponent(Target, Part, Value, ValueType);
		}

		//
		// Apply selection - high-level operations that use the above primitives
		//

		void ApplyGetSelectionImpl(const T& Value, const FSubSelection& Selection, void* OutValue, EPCGMetadataTypes& OutType) const
		{
			if (!Selection.bIsValid)
			{
				// No sub-selection - copy the whole value
				*static_cast<T*>(OutValue) = Value;
				OutType = PCGExTypeOps::TTypeToMetadata<T>::Type;
				return;
			}

			// Handle component extraction first (Transform only)
			if constexpr (std::is_same_v<T, FTransform>)
			{
				if (Selection.bIsComponentSet)
				{
					// Extract the component first
					if (Selection.Component == ETransformPart::Rotation)
					{
						FQuat Rotation;
						EPCGMetadataTypes ComponentType;
						ExtractComponentImpl(Value, Selection.Component, &Rotation, ComponentType);

						// Then apply axis or field selection if needed
						if (Selection.bIsAxisSet)
						{
							*static_cast<FVector*>(OutValue) = SubSelectionFunctions::ExtractAxisFromQuat(Rotation, Selection.Axis);
							OutType = EPCGMetadataTypes::Vector;
						}
						else if (Selection.bIsFieldSet)
						{
							*static_cast<double*>(OutValue) = SubSelectionFunctions::ExtractFieldQuat(Rotation, Selection.Field);
							OutType = EPCGMetadataTypes::Double;
						}
						else
						{
							*static_cast<FQuat*>(OutValue) = Rotation;
							OutType = EPCGMetadataTypes::Quaternion;
						}
					}
					else // Position or Scale
					{
						FVector Vec;
						EPCGMetadataTypes ComponentType;
						ExtractComponentImpl(Value, Selection.Component, &Vec, ComponentType);

						if (Selection.bIsFieldSet)
						{
							*static_cast<double*>(OutValue) = SubSelectionFunctions::ExtractFieldVector(Vec, Selection.Field);
							OutType = EPCGMetadataTypes::Double;
						}
						else
						{
							*static_cast<FVector*>(OutValue) = Vec;
							OutType = EPCGMetadataTypes::Vector;
						}
					}
					return;
				}
			}

			// Handle axis extraction (rotation types)
			if (Selection.bIsAxisSet)
			{
				*static_cast<FVector*>(OutValue) = ExtractAxisImpl(Value, Selection.Axis);
				OutType = EPCGMetadataTypes::Vector;
				return;
			}

			// Handle field extraction
			if (Selection.bIsFieldSet)
			{
				*static_cast<double*>(OutValue) = ExtractFieldImpl(Value, Selection.Field);
				OutType = EPCGMetadataTypes::Double;
				return;
			}

			// Fallback - copy whole value
			*static_cast<T*>(OutValue) = Value;
			OutType = PCGExTypeOps::TTypeToMetadata<T>::Type;
		}

		void ApplySetSelectionImpl(T& Target, const FSubSelection& Selection, const void* Source, EPCGMetadataTypes SourceType) const
		{
			if (!Selection.bIsValid)
			{
				// No sub-selection - need to convert and copy the whole value
				// Use conversion table
				PCGExTypeOps::FConversionTable::Convert(
					SourceType, Source,
					PCGExTypeOps::TTypeToMetadata<T>::Type, &Target);
				return;
			}

			// Handle component injection (Transform only)
			if constexpr (std::is_same_v<T, FTransform>)
			{
				if (Selection.bIsComponentSet)
				{
					if (Selection.Component == ETransformPart::Rotation)
					{
						if (Selection.bIsFieldSet)
						{
							// Get the current rotation, modify one field
							FQuat Rotation = Target.GetRotation();
							double ScalarValue = 0.0;
							PCGExTypeOps::FConversionTable::Convert(SourceType, Source, EPCGMetadataTypes::Double, &ScalarValue);
							SubSelectionFunctions::InjectFieldQuat(Rotation, ScalarValue, Selection.Field);
							Target.SetRotation(Rotation);
						}
						else
						{
							// Set the whole rotation
							InjectComponentImpl(Target, Selection.Component, Source, SourceType);
						}
					}
					else // Position or Scale
					{
						if (Selection.bIsFieldSet)
						{
							FVector Vec = (Selection.Component == ETransformPart::Position)
								              ? Target.GetLocation()
								              : Target.GetScale3D();
							double ScalarValue = 0.0;
							PCGExTypeOps::FConversionTable::Convert(SourceType, Source, EPCGMetadataTypes::Double, &ScalarValue);
							SubSelectionFunctions::InjectFieldVector(Vec, ScalarValue, Selection.Field);

							if (Selection.Component == ETransformPart::Position)
								Target.SetLocation(Vec);
							else
								Target.SetScale3D(Vec);
						}
						else
						{
							InjectComponentImpl(Target, Selection.Component, Source, SourceType);
						}
					}
					return;
				}
			}

			// Handle field injection
			if (Selection.bIsFieldSet)
			{
				double ScalarValue = 0.0;
				PCGExTypeOps::FConversionTable::Convert(SourceType, Source, EPCGMetadataTypes::Double, &ScalarValue);
				InjectFieldImpl(Target, ScalarValue, Selection.Field);
				return;
			}

			// Fallback - convert and copy
			PCGExTypeOps::FConversionTable::Convert(
				SourceType, Source,
				PCGExTypeOps::TTypeToMetadata<T>::Type, &Target);
		}
	};

	// Extern template declarations (instantiated in cpp)
#define PCGEX_DECLARE_SUBSELECTOR_OPS_EXTERN(_TYPE, _NAME, ...) \
	extern template class TSubSelectorOpsImpl<_TYPE>;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_DECLARE_SUBSELECTOR_OPS_EXTERN)
#undef PCGEX_DECLARE_SUBSELECTOR_OPS_EXTERN

	// FSubSelectorRegistry template implementation
	template <typename T>
	const ISubSelectorOps* FSubSelectorRegistry::Get()
	{
		return &TSubSelectorOpsImpl<T>::GetInstance();
	}
} // namespace PCGEx
