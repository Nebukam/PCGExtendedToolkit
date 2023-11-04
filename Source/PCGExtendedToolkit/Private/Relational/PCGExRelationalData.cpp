// Fill out your copyright notice in the Description page of Project Settings.


#include "Relational/PCGExRelationalData.h"

#include "PCGExCommon.h"
#include "Data/PCGPointData.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

UPCGExRelationalData::UPCGExRelationalData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPCGExRelationalData::IsDataReady(UPCGPointData* PointData)
{
	// Whether the data has metadata matching this RelationalData block or not
	return true;
}

const TArray<FPCGExRelationalSlot>& UPCGExRelationalData::GetConstSlots()
{
	return Slots;
}

void UPCGExRelationalData::InitializeLocalDefinition(const FPCGExRelationsDefinition& Definition)
{
	GreatestStaticMaxDistance = 0.0;
	bHasVariableMaxDistance = false;
	Slots.Reset(Definition.Slots.Num());
	for (const FPCGExRelationalSlot& Slot : Definition.Slots)
	{
		if (!Slot.bEnabled) { continue; }
		Slots.Add(Slot);
		if (Slot.bModulateDistance) { bHasVariableMaxDistance = true; }
		GreatestStaticMaxDistance = FMath::Max(GreatestStaticMaxDistance, Slot.Direction.MaxDistance);
	}
}

bool UPCGExRelationalData::PrepareModifiers(const UPCGPointData* PointData, TArray<FPCGExSelectorSettingsBase>& OutModifiers) const
{
	OutModifiers.Reset();

	int NumInvalid = 0;
	for (int i = 0; i < Slots.Num(); i++)
	{
		const FPCGExRelationalSlot& Slot = Slots[i];
		bool bValid = false;

		OutModifiers.Add(FPCGExSelectorSettingsBase(Slot.ModulationAttribute));

		if (Slot.bModulateDistance)
		{
			FPCGExSelectorSettingsBase* Selector = &OutModifiers[i];
			Selector->CopyAndFixLast(PointData);
			if (Selector->IsValid(PointData)) { bValid = true; }
		}

		if (!bValid) { NumInvalid++; }
	}

	return NumInvalid < Slots.Num();
}

double UPCGExRelationalData::InitializeCandidatesForPoint(TArray<FPCGExSamplingCandidateData>& Candidates, const FPCGPoint& Point, bool bUseModifiers, TArray<FPCGExSelectorSettingsBase>& Modifiers) const
{
	Candidates.Reset();

	if (bHasVariableMaxDistance && bUseModifiers)
	{
		double GreatestMaxDistance = GreatestStaticMaxDistance;

		for (int i = 0; i < Slots.Num(); i++)
		{
			const FPCGExRelationalSlot& Slot = Slots[i];
			const FPCGExSelectorSettingsBase* Modifier = &Modifiers[i];
			FPCGExSamplingCandidateData Candidate = FPCGExSamplingCandidateData(Point, Slot);

			double Scale = 1.0;

			auto GetAttributeScaleFactor = [&Modifier, &Point](auto DummyValue) -> double
			{
				using T = decltype(DummyValue);
				FPCGMetadataAttribute<T>* Attribute = FPCGExCommon::GetTypedAttribute<T>(Modifier);
				return GetScaleFactor(Attribute->GetValue(Point.MetadataEntry));
			};

			if (Modifier->bFixed)
			{
				switch (Modifier->Selector.GetSelection())
				{
				case EPCGAttributePropertySelection::Attribute:
					Scale = PCGMetadataAttribute::CallbackWithRightType(Modifier->Attribute->GetTypeId(), GetAttributeScaleFactor);
					break;
#define PCGEX_SCALE_BY_ACCESSOR(_ENUM, _ACCESSOR) case _ENUM: Scale=GetScaleFactor(Point._ACCESSOR); break;
				case EPCGAttributePropertySelection::PointProperty:
					switch (Modifier->Selector.GetPointProperty())
					{
					PCGEX_FOREACH_POINTPROPERTY(PCGEX_SCALE_BY_ACCESSOR)
					default: ;
					}
					break;
				case EPCGAttributePropertySelection::ExtraProperty:
					switch (Modifier->Selector.GetExtraProperty())
					{
					PCGEX_FOREACH_POINTEXTRAPROPERTY(PCGEX_SCALE_BY_ACCESSOR)
					default: ;
					}
					break;
				default: ;
				}
#undef PCGEX_SCALE_BY_ACCESSOR
			}

			Candidate.DistanceScale = Scale;
			GreatestMaxDistance = FMath::Max(GreatestMaxDistance, Candidate.GetScaledDistance());
			Candidates.Add(Candidate);
		}

		return GreatestMaxDistance;
	}
	else
	{
		for (const FPCGExRelationalSlot& Slot : Slots)
		{
			FPCGExSamplingCandidateData Candidate = FPCGExSamplingCandidateData(Point, Slot);
			Candidates.Add(Candidate);
		}

		return GreatestStaticMaxDistance;
	}
}
