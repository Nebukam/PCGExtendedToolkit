// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExSubSelection.h"

#include "CoreMinimal.h"
#include "Data/PCGExSubSelectionOpsImpl.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"

namespace PCGExData
{
	//
	// Helper function implementations
	//

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

	//
	// FSubSelection constructors
	//

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
		case PCGExTypeOps::ETransformPart::Position:
		case PCGExTypeOps::ETransformPart::Scale: return EPCGMetadataTypes::Vector;
		case PCGExTypeOps::ETransformPart::Rotation: return EPCGMetadataTypes::Quaternion;
		}

		return Fallback;
	}

	void FSubSelection::SetComponent(const PCGExTypeOps::ETransformPart InComponent)
	{
		bIsValid = true;
		bIsComponentSet = true;
		Component = InComponent;
	}

	bool FSubSelection::SetFieldIndex(const int32 InFieldIndex)
	{
		if (InFieldIndex < 0 || InFieldIndex > 3)
		{
			bIsFieldSet = false;
			return false;
		}

		bIsValid = true;
		bIsFieldSet = true;

		if (InFieldIndex == 0) { Field = PCGExTypeOps::ESingleField::X; }
		else if (InFieldIndex == 1) { Field = PCGExTypeOps::ESingleField::Y; }
		else if (InFieldIndex == 2) { Field = PCGExTypeOps::ESingleField::Z; }
		else if (InFieldIndex == 3) { Field = PCGExTypeOps::ESingleField::W; }

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
		FInputSelectorComponentData ComponentIDMapping = {PCGExTypeOps::ETransformPart::Rotation, EPCGMetadataTypes::Quaternion};
		FInputSelectorFieldData FieldIDMapping = {PCGExTypeOps::ESingleField::X, EPCGMetadataTypes::Unknown, 0};

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
		SetFieldIndex(FieldIDMapping.Get<2>());

		if (bIsFieldSet)
		{
			bIsValid = true;
			if (!bIsComponentSet) { PossibleSourceType = FieldIDMapping.Get<1>(); }
		}
	}

	//
	// Type-Erased Interface Implementation
	//

	void FSubSelection::ApplyGet(EPCGMetadataTypes SourceType, const void* Source,
	                             void* OutValue, EPCGMetadataTypes& OutType) const
	{
		const ISubSelectorOps* Ops = FSubSelectorRegistry::Get(SourceType);
		if (!Ops)
		{
			OutType = EPCGMetadataTypes::Unknown;
			return;
		}

		Ops->ApplyGetSelection(Source, *this, OutValue, OutType);
	}

	void FSubSelection::ApplySet(EPCGMetadataTypes TargetType, void* Target,
	                             EPCGMetadataTypes SourceType, const void* Source) const
	{
		const ISubSelectorOps* Ops = FSubSelectorRegistry::Get(TargetType);
		if (!Ops) { return; }

		Ops->ApplySetSelection(Target, *this, Source, SourceType);
	}

	double FSubSelection::ExtractFieldToDouble(EPCGMetadataTypes SourceType, const void* Source) const
	{
		const ISubSelectorOps* Ops = FSubSelectorRegistry::Get(SourceType);
		if (!Ops) { return 0.0; }

		return Ops->ExtractField(Source, Field);
	}

	void FSubSelection::InjectFieldFromDouble(EPCGMetadataTypes TargetType, void* Target, double Value) const
	{
		const ISubSelectorOps* Ops = FSubSelectorRegistry::Get(TargetType);
		if (!Ops) { return; }

		Ops->InjectField(Target, Value, Field);
	}

	//
	// Legacy Type-Erased Interface (GetVoid/SetVoid)
	//
	// These implement the original signature but use the new type-erased system internally.
	// NOTE: For performance, prefer using FCachedSubSelection in IBufferProxy instead.
	//

	void FSubSelection::GetVoid(EPCGMetadataTypes SourceType, const void* Source, EPCGMetadataTypes WorkingType, void* Target) const
	{
		if (!bIsValid)
		{
			// No sub-selection - just convert
			PCGExTypeOps::FConversionTable::Convert(SourceType, Source, WorkingType, Target);
			return;
		}

		// Apply the sub-selection to get an intermediate value
		alignas(16) uint8 IntermediateBuffer[96];
		EPCGMetadataTypes IntermediateType = EPCGMetadataTypes::Unknown;

		ApplyGet(SourceType, Source, IntermediateBuffer, IntermediateType);

		// Convert from intermediate to working type if needed
		if (IntermediateType == WorkingType)
		{
			// Direct copy using type ops (handles strings, etc.)
			const PCGExTypeOps::ITypeOpsBase* TypeOps = PCGExTypeOps::FTypeOpsRegistry::Get(IntermediateType);
			if (TypeOps)
			{
				TypeOps->Copy(IntermediateBuffer, Target);
			}
		}
		else if (IntermediateType != EPCGMetadataTypes::Unknown)
		{
			// Need conversion
			PCGExTypeOps::FConversionTable::Convert(IntermediateType, IntermediateBuffer, WorkingType, Target);
		}
		else
		{
			// ApplyGet didn't produce valid output, fallback to direct conversion
			PCGExTypeOps::FConversionTable::Convert(SourceType, Source, WorkingType, Target);
		}
	}

	void FSubSelection::SetVoid(EPCGMetadataTypes TargetType, void* Target,
	                            EPCGMetadataTypes SourceType, const void* Source) const
	{
		if (!bIsValid)
		{
			// No sub-selection - just convert
			PCGExTypeOps::FConversionTable::Convert(SourceType, Source, TargetType, Target);
			return;
		}

		// Use the sub-selector ops to apply the set
		ApplySet(TargetType, Target, SourceType, Source);
	}

	//
	// Helper functions for type resolution
	//

	bool TryGetType(const FPCGAttributePropertyInputSelector& InputSelector, const UPCGData* InData, EPCGMetadataTypes& OutType)
	{
		OutType = EPCGMetadataTypes::Unknown;

		if (!IsValid(InData)) { return false; }

		const FPCGAttributePropertyInputSelector FixedSelector = InputSelector.CopyAndFixLast(InData);
		if (!FixedSelector.IsValid()) { return false; }

		if (FixedSelector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			if (!InData->Metadata) { return false; }
			if (const FPCGMetadataAttributeBase* AttributeBase = InData->Metadata->GetConstAttribute(PCGExMetaHelpers::GetAttributeIdentifier(FixedSelector, InData)))
			{
				OutType = static_cast<EPCGMetadataTypes>(AttributeBase->GetTypeId());
			}
		}
		else if (FixedSelector.GetSelection() == EPCGAttributePropertySelection::ExtraProperty)
		{
			OutType = PCGExMetaHelpers::GetPropertyType(FixedSelector.GetExtraProperty());
		}
		else if (FixedSelector.GetSelection() == EPCGAttributePropertySelection::Property)
		{
			OutType = PCGExMetaHelpers::GetPropertyType(FixedSelector.GetPointProperty());
		}

		return OutType != EPCGMetadataTypes::Unknown;
	}

	bool TryGetTypeAndSource(const FPCGAttributePropertyInputSelector& InputSelector, const TSharedPtr<FFacade>& InDataFacade, EPCGMetadataTypes& OutType, EIOSide& InOutSide)
	{
		OutType = EPCGMetadataTypes::Unknown;
		if (InOutSide == EIOSide::In)
		{
			if (!TryGetType(InputSelector, InDataFacade->GetIn(), OutType))
			{
				if (TryGetType(InputSelector, InDataFacade->GetOut(), OutType)) { InOutSide = EIOSide::Out; }
			}
		}
		else
		{
			if (!TryGetType(InputSelector, InDataFacade->GetOut(), OutType))
			{
				if (TryGetType(InputSelector, InDataFacade->GetIn(), OutType)) { InOutSide = EIOSide::In; }
			}
		}

		return OutType != EPCGMetadataTypes::Unknown;
	}

	bool TryGetTypeAndSource(const FName AttributeName, const TSharedPtr<FFacade>& InDataFacade, EPCGMetadataTypes& OutType, EIOSide& InOutSource)
	{
		FPCGAttributePropertyInputSelector Selector;
		Selector.SetAttributeName(AttributeName);
		return TryGetTypeAndSource(Selector, InDataFacade, OutType, InOutSource);
	}
}
