// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/PCGExRelationalData.h"

#include "PCGExCommon.h"
#include "Data/PCGPointData.h"
//#include "Metadata/Accessors/IPCGAttributeAccessor.h"
//#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

UPCGExRelationalData::UPCGExRelationalData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LocalRelations.Empty();
	Relations = LocalRelations;
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
 * Initialize as new from Params
 * @param InParams 
 * @param InRelationalData 
 */
void UPCGExRelationalData::Initialize(UPCGExRelationalParamsData** InParams)
{

	Parent = nullptr;
	Params = *InParams;
	
	LocalRelations.Empty();
	Relations = LocalRelations;
	
}

/**
 * Initialize this from another RelationalData
 * @param InRelationalData 
 */
void UPCGExRelationalData::Initialize(UPCGExRelationalData** InRelationalData)
{

	Parent = *InRelationalData;
	Params = Parent->Params;
	
	Relations = Parent->Relations;
	
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

