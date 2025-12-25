// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExCachedSubSelection.h"

#include "Helpers/PCGExMetaHelpers.h"
#include "Types/PCGExTypeOpsImpl.h"

namespace PCGExData
{
	namespace SubSelectionImpl
	{
		FExtractFieldFn GetExtractFieldFn(EPCGMetadataTypes Type)
		{
#define PCGEX_FN(_TYPE, _NAME, ...) case EPCGMetadataTypes::_NAME: return &PCGExTypeOps::FTypeOps<_TYPE>::ExtractField;
			switch (Type)
			{
			PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_FN)
			default: return nullptr;
			}
#undef PCGEX_FN
		}

		FInjectFieldFn GetInjectFieldFn(EPCGMetadataTypes Type)
		{
#define PCGEX_FN(_TYPE, _NAME, ...) case EPCGMetadataTypes::_NAME: return &PCGExTypeOps::FTypeOps<_TYPE>::InjectField;
			switch (Type)
			{
			PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_FN)
			default: return nullptr;
			}
#undef PCGEX_FN
		}

		FExtractAxisFn GetExtractAxisFn(EPCGMetadataTypes Type)
		{
			switch (Type)
			{
			case EPCGMetadataTypes::Quaternion: return &PCGExTypeOps::FTypeOps<FQuat>::ExtractAxis;
			case EPCGMetadataTypes::Rotator: return &PCGExTypeOps::FTypeOps<FRotator>::ExtractAxis;
			case EPCGMetadataTypes::Transform: return &PCGExTypeOps::FTypeOps<FTransform>::ExtractAxis;
			default: return &ExtractAxisDefault;
			}
		}

		// Helper to get number of fields for a type
		int32 GetNumFields(EPCGMetadataTypes Type)
		{
			switch (Type)
			{
			case EPCGMetadataTypes::Vector2: return 2;
			case EPCGMetadataTypes::Vector:
			case EPCGMetadataTypes::Rotator: return 3;
			case EPCGMetadataTypes::Vector4:
			case EPCGMetadataTypes::Quaternion: return 4;
			case EPCGMetadataTypes::Transform: return 9;
			default: return 1;
			}
		}

		bool SupportsAxisExtraction(EPCGMetadataTypes Type)
		{
			return Type == EPCGMetadataTypes::Quaternion ||
				Type == EPCGMetadataTypes::Rotator ||
				Type == EPCGMetadataTypes::Transform;
		}
	}

	void FCachedSubSelection::Initialize(
		const FSubSelection& Selection,
		EPCGMetadataTypes InRealType,
		EPCGMetadataTypes InWorkingType)
	{
		// Copy configuration
		bIsValid = Selection.bIsValid;
		bIsFieldSet = Selection.bIsFieldSet;
		bIsAxisSet = Selection.bIsAxisSet;
		bIsComponentSet = Selection.bIsComponentSet;
		Field = Selection.Field;
		Axis = Selection.Axis;
		Component = Selection.Component;

		RealType = InRealType;
		WorkingType = InWorkingType;

		// Determine component type for transforms
		if (bIsComponentSet && InRealType == EPCGMetadataTypes::Transform)
		{
			switch (Component)
			{
			case PCGExTypeOps::ETransformPart::Position:
			case PCGExTypeOps::ETransformPart::Scale: ComponentType = EPCGMetadataTypes::Vector;
				break;
			case PCGExTypeOps::ETransformPart::Rotation: ComponentType = EPCGMetadataTypes::Quaternion;
				break;
			}
		}

		// Cache type ops
		RealOps = PCGExTypeOps::FTypeOpsRegistry::Get(RealType);
		WorkingOps = PCGExTypeOps::FTypeOpsRegistry::Get(WorkingType);

		// Cache field operation function pointers
		ExtractFieldFromReal = SubSelectionImpl::GetExtractFieldFn(RealType);
		InjectFieldToReal = SubSelectionImpl::GetInjectFieldFn(RealType);
		ExtractFieldFromWorking = SubSelectionImpl::GetExtractFieldFn(WorkingType);
		InjectFieldToWorking = SubSelectionImpl::GetInjectFieldFn(WorkingType);

		// Cache axis extraction
		ExtractAxisFromReal = SubSelectionImpl::GetExtractAxisFn(RealType);

		// Cache transform component operations (only for Transform type)
		if (RealType == EPCGMetadataTypes::Transform)
		{
			ExtractComponent = &PCGExTypeOps::FTypeOps<FTransform>::ExtractComponent;
			InjectComponent = &PCGExTypeOps::FTypeOps<FTransform>::InjectComponent;
		}

		// Cache conversion functions
		ConvertRealToWorking = PCGExTypeOps::FConversionTable::GetConversionFn(RealType, WorkingType);
		ConvertWorkingToReal = PCGExTypeOps::FConversionTable::GetConversionFn(WorkingType, RealType);
		ConvertWorkingToDouble = PCGExTypeOps::FConversionTable::GetConversionFn(WorkingType, EPCGMetadataTypes::Double);
		ConvertDoubleToWorking = PCGExTypeOps::FConversionTable::GetConversionFn(EPCGMetadataTypes::Double, WorkingType);
		ConvertRealToDouble = PCGExTypeOps::FConversionTable::GetConversionFn(RealType, EPCGMetadataTypes::Double);
		ConvertDoubleToReal = PCGExTypeOps::FConversionTable::GetConversionFn(EPCGMetadataTypes::Double, RealType);
	}

	bool FCachedSubSelection::AppliesToSourceRead() const
	{
		if (!bIsValid) { return false; }

		// For field selection, only applies if source has multiple fields
		if (bIsFieldSet) { return SubSelectionImpl::GetNumFields(RealType) > 1; }

		// For axis selection, only applies if source is rotation type
		if (bIsAxisSet) { return SubSelectionImpl::SupportsAxisExtraction(RealType); }

		// For component selection, only applies to Transform
		if (bIsComponentSet) { return RealType == EPCGMetadataTypes::Transform; }

		return false;
	}

	bool FCachedSubSelection::AppliesToTargetWrite() const
	{
		if (!bIsValid) { return false; }

		// For field selection, only applies if target has multiple fields
		if (bIsFieldSet) { return SubSelectionImpl::GetNumFields(RealType) > 1; }

		// For component selection, only applies to Transform
		if (bIsComponentSet) { return RealType == EPCGMetadataTypes::Transform; }

		return false;
	}

	void FCachedSubSelection::ApplyGet(const void* Source, void* OutValue) const
	{
		if (!bIsValid || !AppliesToSourceRead())
		{
			// No applicable sub-selection - just convert
			if (ConvertRealToWorking) { ConvertRealToWorking(Source, OutValue); }
			return;
		}

		// Handle component extraction for Transform
		if (bIsComponentSet && RealType == EPCGMetadataTypes::Transform)
		{
			ApplyGetWithComponent(Source, OutValue);
			return;
		}

		// Handle axis extraction
		if (bIsAxisSet && ExtractAxisFromReal)
		{
			const FVector AxisDir = ExtractAxisFromReal(Source, Axis);

			// Convert FVector to WorkingType
			if (WorkingType == EPCGMetadataTypes::Vector) { *static_cast<FVector*>(OutValue) = AxisDir; }
			// Need to convert FVector → WorkingType
			else { PCGExTypeOps::FConversionTable::Convert(EPCGMetadataTypes::Vector, &AxisDir, WorkingType, OutValue); }
			return;
		}

		// Handle field extraction
		if (bIsFieldSet && ExtractFieldFromReal)
		{
			const double FieldValue = ExtractFieldFromReal(Source, Field);

			// Convert double to WorkingType
			if (WorkingType == EPCGMetadataTypes::Double) { *static_cast<double*>(OutValue) = FieldValue; }
			else if (ConvertDoubleToWorking) { ConvertDoubleToWorking(&FieldValue, OutValue); }
			return;
		}

		// Fallback - just convert
		if (ConvertRealToWorking) { ConvertRealToWorking(Source, OutValue); }
	}

	void FCachedSubSelection::ApplySet(void* Target, const void* Source) const
	{
		if (!bIsValid || !AppliesToTargetWrite())
		{
			// No applicable sub-selection - just convert
			if (ConvertWorkingToReal) { ConvertWorkingToReal(Source, Target); }
			return;
		}

		// Handle component injection for Transform
		if (bIsComponentSet && RealType == EPCGMetadataTypes::Transform)
		{
			ApplySetWithComponent(Target, Source);
			return;
		}

		// Handle field injection
		if (bIsFieldSet && InjectFieldToReal)
		{
			// Convert source (WorkingType) to double
			double ScalarValue = 0.0;

			if (WorkingType == EPCGMetadataTypes::Double) { ScalarValue = *static_cast<const double*>(Source); }
			else if (ConvertWorkingToDouble) { ConvertWorkingToDouble(Source, &ScalarValue); }

			// Inject into target field
			InjectFieldToReal(Target, ScalarValue, Field);
			return;
		}

		// Fallback - just convert
		if (ConvertWorkingToReal) { ConvertWorkingToReal(Source, Target); }
	}

	void FCachedSubSelection::ApplyGetWithComponent(const void* Source, void* OutValue) const
	{
		// Extract the component from transform
		alignas(16) uint8 ComponentBuffer[96];

		if (ExtractComponent)
		{
			EPCGMetadataTypes SubType;
			ExtractComponent(Source, Component, ComponentBuffer, SubType);
		}

		// Now apply axis or field selection to the component
		if (bIsAxisSet && Component == PCGExTypeOps::ETransformPart::Rotation)
		{
			// Extract axis from quaternion
			const FVector AxisDir = PCGExTypeOps::FTypeOps<FQuat>::ExtractAxis(ComponentBuffer, Axis);

			if (WorkingType == EPCGMetadataTypes::Vector) { *static_cast<FVector*>(OutValue) = AxisDir; }
			else { PCGExTypeOps::FConversionTable::Convert(EPCGMetadataTypes::Vector, &AxisDir, WorkingType, OutValue); }
		}
		else if (bIsFieldSet)
		{
			// Extract field from component
			FExtractFieldFn ExtractFn = SubSelectionImpl::GetExtractFieldFn(ComponentType);
			if (ExtractFn)
			{
				const double FieldValue = ExtractFn(ComponentBuffer, Field);

				if (WorkingType == EPCGMetadataTypes::Double) { *static_cast<double*>(OutValue) = FieldValue; }
				else if (ConvertDoubleToWorking) { ConvertDoubleToWorking(&FieldValue, OutValue); }
			}
		}
		else
		{
			// Just output the component
			PCGExTypeOps::FConversionTable::Convert(
				ComponentType, ComponentBuffer,
				WorkingType, OutValue);
		}
	}

	void FCachedSubSelection::ApplySetWithComponent(void* Target, const void* Source) const
	{
		FTransform& T = *static_cast<FTransform*>(Target);

		if (bIsFieldSet)
		{
			// Convert source to double
			double ScalarValue = 0.0;

			if (WorkingType == EPCGMetadataTypes::Double) { ScalarValue = *static_cast<const double*>(Source); }
			else if (ConvertWorkingToDouble) { ConvertWorkingToDouble(Source, &ScalarValue); }

			// Inject into the appropriate component
			switch (Component)
			{
			case PCGExTypeOps::ETransformPart::Position:
				{
					FVector Pos = T.GetLocation();
					PCGExTypeOps::FTypeOps<FVector>::InjectField(&Pos, ScalarValue, Field);
					T.SetLocation(Pos);
				}
				break;
			case PCGExTypeOps::ETransformPart::Rotation:
				{
					FQuat Rot = T.GetRotation();
					PCGExTypeOps::FTypeOps<FQuat>::InjectField(&Rot, ScalarValue, Field);
					T.SetRotation(Rot);
				}
				break;
			case PCGExTypeOps::ETransformPart::Scale:
				{
					FVector Scale = T.GetScale3D();
					PCGExTypeOps::FTypeOps<FVector>::InjectField(&Scale, ScalarValue, Field);
					T.SetScale3D(Scale);
				}
				break;
			}
		}
		else
		{
			// Set the whole component
			alignas(16) uint8 ComponentBuffer[96];

			// Convert source to component type
			PCGExTypeOps::FConversionTable::Convert(WorkingType, Source, ComponentType, ComponentBuffer);
			if (InjectComponent) { InjectComponent(Target, Component, ComponentBuffer, ComponentType); }
		}
	}
}
