// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExBroadcast.h"
#include "Data/PCGExData.h"

namespace PCGEx
{
	bool GetComponentSelection(const TArray<FString>& Names, EPCGExTransformComponent& OutSelection)
	{
		if (Names.IsEmpty()) { return false; }
		for (const FString& Name : Names)
		{
			if (const EPCGExTransformComponent* Selection = STRMAP_TRANSFORM_FIELD.Find(Name.ToUpper()))
			{
				OutSelection = *Selection;
				return true;
			}
		}
		return false;
	}

	bool GetFieldSelection(const TArray<FString>& Names, EPCGExSingleField& OutSelection)
	{
		if (Names.IsEmpty()) { return false; }
		const FString& STR = Names.Num() > 1 ? Names[1].ToUpper() : Names[0].ToUpper();
		if (const EPCGExSingleField* Selection = STRMAP_SINGLE_FIELD.Find(STR))
		{
			OutSelection = *Selection;
			return true;
		}
		if (STR.Len() <= 0) { return false; }
		if (const EPCGExSingleField* Selection = STRMAP_SINGLE_FIELD.Find(FString::Printf(TEXT("%c"), STR[0]).ToUpper()))
		{
			OutSelection = *Selection;
			return true;
		}
		return false;
	}

	bool GetAxisSelection(const TArray<FString>& Names, EPCGExAxis& OutSelection)
	{
		if (Names.IsEmpty()) { return false; }
		for (const FString& Name : Names)
		{
			if (const EPCGExAxis* Selection = STRMAP_AXIS.Find(Name.ToUpper()))
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

	EPCGMetadataTypes FSubSelection::GetSubType() const
	{
		if (!bIsValid) { return EPCGMetadataTypes::Unknown; }
		if (bIsFieldSet) { return EPCGMetadataTypes::Double; }
		if (bIsAxisSet) { return EPCGMetadataTypes::Vector; }

		switch (Component)
		{
		case EPCGExTransformComponent::Position:
		case EPCGExTransformComponent::Scale:
			return EPCGMetadataTypes::Vector;
		case EPCGExTransformComponent::Rotation:
			return EPCGMetadataTypes::Quaternion;
		}

		return EPCGMetadataTypes::Unknown;
	}

	void FSubSelection::Init(const TArray<FString>& ExtraNames)
	{
		if (ExtraNames.IsEmpty())
		{
			bIsValid = false;
			return;
		}

		bIsAxisSet = GetAxisSelection(ExtraNames, Axis);
		if (bIsAxisSet)
		{
			bIsValid = true;
			if (!GetComponentSelection(ExtraNames, Component))
			{
				// Only axis is set, assume it's a transform, so rotation
				Component = EPCGExTransformComponent::Rotation;
			}
		}
		else
		{
			bIsValid = GetComponentSelection(ExtraNames, Component);
		}

		bIsFieldSet = GetFieldSelection(ExtraNames, Field);
		if (bIsFieldSet) { bIsValid = true; }

		Update();
	}

	void FSubSelection::Update()
	{
		switch (Field)
		{
		case EPCGExSingleField::X:
			FieldIndex = 0;
			break;
		case EPCGExSingleField::Y:
			FieldIndex = 1;
			break;
		case EPCGExSingleField::Z:
			FieldIndex = 2;
			break;
		case EPCGExSingleField::W:
			FieldIndex = 3;
			break;
		case EPCGExSingleField::Length:
		case EPCGExSingleField::SquaredLength:
		case EPCGExSingleField::Volume:
			FieldIndex = 0;
			break;
		}
	}

	bool TryGetTypeAndSource(
		const FPCGAttributePropertyInputSelector& InputSelector,
		const TSharedPtr<PCGExData::FFacade>& InDataFacade,
		EPCGMetadataTypes& OutType, PCGExData::ESource& InOutSource)
	{
		OutType = EPCGMetadataTypes::Unknown;
		const UPCGPointData* Data = InOutSource == PCGExData::ESource::In ? InDataFacade->Source->GetInOut() : InDataFacade->Source->GetOutIn();
		if (!InDataFacade->Source->GetSource(Data, InOutSource)) { return false; }

		const FPCGAttributePropertyInputSelector FixedSelector = InputSelector.CopyAndFixLast(Data);
		if (!FixedSelector.IsValid()) { return false; }

		const TArray<FString>& ExtraNames = FixedSelector.GetExtraNames();

		if (FixedSelector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			if (!Data->Metadata) { return false; }
			if (const FPCGMetadataAttributeBase* AttributeBase = Data->Metadata->GetConstAttribute(FixedSelector.GetName()))
			{
				OutType = static_cast<EPCGMetadataTypes>(AttributeBase->GetTypeId());
			}
		}
		else if (FixedSelector.GetSelection() == EPCGAttributePropertySelection::ExtraProperty)
		{
			OutType = GetPropertyType(FixedSelector.GetExtraProperty());
		}
		else if (FixedSelector.GetSelection() == EPCGAttributePropertySelection::PointProperty)
		{
			OutType = GetPropertyType(FixedSelector.GetPointProperty());
		}

		return OutType != EPCGMetadataTypes::Unknown;
	}

	bool TryGetTypeAndSource(
		const FName AttributeName,
		const TSharedPtr<PCGExData::FFacade>& InDataFacade,
		EPCGMetadataTypes& OutType, PCGExData::ESource& InOutSource)
	{
		return true;
	}
}
