// Fill out your copyright notice in the Description page of Project Settings.


#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerAttribute.h"

#include "PCGEx.h"
#include "PCGExMath.h"
#include "Data/PCGPointData.h"

void UPCGExGoalPickerAttribute::PrepareForData(const UPCGPointData* InSeeds, const UPCGPointData* InGoals)
{
	Super::PrepareForData(InSeeds, InGoals);

	if (GoalCount == EPCGExGoalPickAttributeAmount::Single)
	{
		AttributeGetter.Descriptor = static_cast<FPCGExInputDescriptor>(Attribute);
		AttributeGetter.Validate(InSeeds);
	}
	else
	{
		AttributeGetters.Reset(Attributes.Num());
		for (const FPCGExInputDescriptorWithSingleField& Descriptor : Attributes)
		{
			PCGEx::FLocalSingleFieldGetter& Getter = AttributeGetters.Emplace_GetRef();
			Getter.Descriptor = static_cast<FPCGExInputDescriptor>(Descriptor);
			Getter.Validate(InSeeds);
		}
	}
}

int32 UPCGExGoalPickerAttribute::GetGoalIndex(const FPCGPoint& Seed, const int32 SeedIndex) const
{
	return PCGEx::SanitizeIndex(
		static_cast<int32>(AttributeGetter.GetValueSafe(Seed, -1)),
		MaxGoalIndex, IndexSafety);
}

void UPCGExGoalPickerAttribute::GetGoalIndices(const FPCGPoint& Seed, TArray<int32>& OutIndices) const
{
	for (PCGEx::FLocalSingleFieldGetter Getter : AttributeGetters)
	{
		if (!Getter.bValid) { continue; }
		OutIndices.Add(
			PCGEx::SanitizeIndex(
				static_cast<int32>(Getter.GetValueSafe(Seed, -1)),
				MaxGoalIndex, IndexSafety));
	}
}

bool UPCGExGoalPickerAttribute::OutputMultipleGoals() const { return GoalCount != EPCGExGoalPickAttributeAmount::Single; }

void UPCGExGoalPickerAttribute::UpdateUserFacingInfos()
{
	Super::UpdateUserFacingInfos();
	for (FPCGExInputDescriptorWithSingleField& Descriptor : Attributes)
	{
		Descriptor.UpdateUserFacingInfos();
	}
}
