// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerAttribute.h"

#include "PCGEx.h"

void UPCGExGoalPickerAttribute::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	const UPCGExGoalPickerAttribute* TypedOther = Cast<UPCGExGoalPickerAttribute>(Other);
	if (TypedOther)
	{
		GoalCount = TypedOther->GoalCount;
		Attribute = TypedOther->Attribute;
		Attributes = TypedOther->Attributes;
	}
}

void UPCGExGoalPickerAttribute::PrepareForData(const PCGExData::FPointIO* InSeeds, const PCGExData::FPointIO* InGoals)
{
	Super::PrepareForData(InSeeds, InGoals);

	if (GoalCount == EPCGExGoalPickAttributeAmount::Single)
	{
		AttributeGetter.Config = FPCGExInputConfig(Attribute);
		AttributeGetter.Grab(InSeeds);
	}
	else
	{
		AttributeGetters.Reset(Attributes.Num());
		for (const FPCGAttributePropertyInputSelector& Config : Attributes)
		{
			PCGEx::FLocalSingleFieldGetter& Getter = AttributeGetters.Emplace_GetRef();
			Getter.Config = FPCGExInputConfig(Config);
			Getter.Grab(InSeeds);
		}
	}
}

int32 UPCGExGoalPickerAttribute::GetGoalIndex(const PCGEx::FPointRef& Seed) const
{
	return PCGExMath::SanitizeIndex(
		static_cast<int32>(AttributeGetter.SafeGet(Seed.Index, -1)),
		MaxGoalIndex, IndexSafety);
}

void UPCGExGoalPickerAttribute::GetGoalIndices(const PCGEx::FPointRef& Seed, TArray<int32>& OutIndices) const
{
	for (PCGEx::FLocalSingleFieldGetter Getter : AttributeGetters)
	{
		if (!Getter.bValid) { continue; }
		OutIndices.Add(
			PCGExMath::SanitizeIndex(
				static_cast<int32>(Getter.SafeGet(Seed.Index, -1)),
				MaxGoalIndex, IndexSafety));
	}
}

bool UPCGExGoalPickerAttribute::OutputMultipleGoals() const { return GoalCount != EPCGExGoalPickAttributeAmount::Single; }
