﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerAttribute.h"

#include "PCGEx.h"

void UPCGExGoalPickerAttribute::PrepareForData(const PCGExData::FPointIO& InSeeds, const PCGExData::FPointIO& InGoals)
{
	Super::PrepareForData(InSeeds, InGoals);

	if (GoalCount == EPCGExGoalPickAttributeAmount::Single)
	{
		AttributeGetter.Descriptor = static_cast<FPCGExInputDescriptor>(Attribute);
		AttributeGetter.Bind(InSeeds);
	}
	else
	{
		AttributeGetters.Reset(Attributes.Num());
		for (const FPCGExInputDescriptorWithSingleField& Descriptor : Attributes)
		{
			PCGEx::FLocalSingleFieldGetter& Getter = AttributeGetters.Emplace_GetRef();
			Getter.Descriptor = static_cast<FPCGExInputDescriptor>(Descriptor);
			Getter.Bind(InSeeds);
		}
	}
}

int32 UPCGExGoalPickerAttribute::GetGoalIndex(const PCGEx::FPointRef& Seed) const
{
	return PCGEx::SanitizeIndex(
		static_cast<int32>(AttributeGetter.SafeGet(Seed.Index, -1)),
		MaxGoalIndex, IndexSafety);
}

void UPCGExGoalPickerAttribute::GetGoalIndices(const PCGEx::FPointRef& Seed, TArray<int32>& OutIndices) const
{
	for (PCGEx::FLocalSingleFieldGetter Getter : AttributeGetters)
	{
		if (!Getter.bValid) { continue; }
		OutIndices.Add(
			PCGEx::SanitizeIndex(
				static_cast<int32>(Getter.SafeGet(Seed.Index, -1)),
				MaxGoalIndex, IndexSafety));
	}
}

bool UPCGExGoalPickerAttribute::OutputMultipleGoals() const { return GoalCount != EPCGExGoalPickAttributeAmount::Single; }

#if WITH_EDITOR
void UPCGExGoalPickerAttribute::UpdateUserFacingInfos()
{
	Super::UpdateUserFacingInfos();
	for (FPCGExInputDescriptorWithSingleField& Descriptor : Attributes)
	{
		Descriptor.UpdateUserFacingInfos();
	}
}
#endif