// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/PCGExRelationalParamsData.h"

#include "PCGExCommon.h"
#include "Data/PCGPointData.h"
//#include "Metadata/Accessors/IPCGAttributeAccessor.h"
//#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

UPCGExRelationalParamsData::UPCGExRelationalParamsData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

/**
 * 
 * @param PointData 
 * @return 
 */
bool UPCGExRelationalParamsData::IsDataReady(UPCGPointData* PointData)
{
	// Whether the data has metadata matching this RelationalData block or not
	return true;
}

/**
 * 
 * @return 
 */
const TArray<FPCGExRelationDefinition>& UPCGExRelationalParamsData::GetConstSlots()
{
	return RelationSlots;
}

/**
 * Initialize this from params.
 * @param Identifier
 * @param Definition 
 */
void UPCGExRelationalParamsData::Initialize(FName Identifier, const FPCGExRelationsDefinition& Definition)
{
	
	RelationalIdentifier = Identifier; 
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
bool UPCGExRelationalParamsData::PrepareSelectors(const UPCGPointData* PointData, TArray<FPCGExSamplingModifier>& OutSelectors) const
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