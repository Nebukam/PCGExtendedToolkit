// Fill out your copyright notice in the Description page of Project Settings.


#include "Relational/PCGExRelationalData.h"

#include "PCGExCommon.h"
#include "Data/PCGPointData.h"
//#include "Metadata/Accessors/IPCGAttributeAccessor.h"
//#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

UPCGExRelationalData::UPCGExRelationalData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/**
 * 
 * @param PointData 
 * @return 
 */
bool UPCGExRelationalData::IsDataReady(UPCGPointData* PointData)
{
	// Whether the data has metadata matching this RelationalData block or not
	return true;
}

/**
 * 
 * @return 
 */
const TArray<FPCGExRelationDefinition>& UPCGExRelationalData::GetConstSlots()
{
	return RelationSlots;
}

/**
 * 
 * @param Definition 
 */
void UPCGExRelationalData::InitializeFromSettings(const FPCGExRelationsDefinition& Definition)
{
	GreatestStaticMaxDistance = 0.0;
	bHasVariableMaxDistance = false;
	RelationSlots.Reset(Definition.RelationSlots.Num());
	for (const FPCGExRelationDefinition& Slot : Definition.RelationSlots)
	{
		if (!Slot.bEnabled) { continue; }
		RelationSlots.Add(Slot);
		if (Slot.bApplyAttributeModifier) { bHasVariableMaxDistance = true; }
		GreatestStaticMaxDistance = FMath::Max(GreatestStaticMaxDistance, Slot.Direction.MaxDistance);
	}
}

/**
 * 
 * @param PointData 
 * @param OutSelectors 
 * @return 
 */
bool UPCGExRelationalData::PrepareSelectors(const UPCGPointData* PointData, TArray<FPCGExSamplingModifier>& OutSelectors) const
{
	OutSelectors.Reset();

	int NumInvalid = 0;
	for (int i = 0; i < RelationSlots.Num(); i++)
	{
		const FPCGExRelationDefinition& Slot = RelationSlots[i];
		bool bValid = false;

		OutSelectors.Add(FPCGExSamplingModifier(Slot.AttributeModifier));

		if (Slot.bApplyAttributeModifier)
		{
			FPCGExSamplingModifier* Selector = &OutSelectors[i];
			Selector->CopyAndFixLast(PointData);
			if (Selector->IsValid(PointData)) { bValid = true; }
		}

		if (!bValid) { NumInvalid++; }
	}

	return NumInvalid < RelationSlots.Num();
}

/**
 * 
 * @param Candidates 
 * @param Point 
 * @param bUseModifiers 
 * @param Modifiers 
 * @return 
 */
double UPCGExRelationalData::PrepareCandidatesForPoint(TArray<FPCGExRelationCandidate>& Candidates, const FPCGPoint& Point, bool bUseModifiers, TArray<FPCGExSamplingModifier>& Modifiers) const
{
	Candidates.Reset();

	if (bHasVariableMaxDistance && bUseModifiers)
	{
		double GreatestMaxDistance = GreatestStaticMaxDistance;

		for (int i = 0; i < RelationSlots.Num(); i++)
		{
			const FPCGExRelationDefinition& Slot = RelationSlots[i];
			const FPCGExSamplingModifier* Modifier = &Modifiers[i];
			FPCGExRelationCandidate Candidate = FPCGExRelationCandidate(Point, Slot);

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
		for (const FPCGExRelationDefinition& Slot : RelationSlots)
		{
			FPCGExRelationCandidate Candidate = FPCGExRelationCandidate(Point, Slot);
			Candidates.Add(Candidate);
		}

		return GreatestStaticMaxDistance;
	}
}
