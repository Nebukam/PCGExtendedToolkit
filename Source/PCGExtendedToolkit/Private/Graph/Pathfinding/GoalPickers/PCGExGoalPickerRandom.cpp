// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"

#include "PCGExMath.h"

void UPCGExGoalPickerRandom::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	const UPCGExGoalPickerRandom* TypedOther = Cast<UPCGExGoalPickerRandom>(Other);
	if (TypedOther)
	{
		GoalCount = TypedOther->GoalCount;
		NumGoals = TypedOther->NumGoals;
		bUseLocalNumGoals = TypedOther->bUseLocalNumGoals;
		LocalNumGoalAttribute = TypedOther->LocalNumGoalAttribute;
	}
}

void UPCGExGoalPickerRandom::PrepareForData(PCGExData::FFacade* InSeedsDataFacade, PCGExData::FFacade* InGoalsDataFacade)
{
	if (bUseLocalNumGoals) { NumGoalsGetter = InSeedsDataFacade->GetBroadcaster<int32>(LocalNumGoalAttribute); }
	Super::PrepareForData(InSeedsDataFacade, InGoalsDataFacade);
}

int32 UPCGExGoalPickerRandom::GetGoalIndex(const PCGExData::FPointRef& Seed) const
{
	const int32 Index = static_cast<int32>(PCGExMath::Remap(
		FMath::PerlinNoise3D(PCGExMath::Tile(Seed.Point->Transform.GetLocation() * 0.001, FVector(-1), FVector(1))),
		-1, 1, 0, MaxGoalIndex));
	return PCGExMath::SanitizeIndex(Index, MaxGoalIndex, IndexSafety);
}

void UPCGExGoalPickerRandom::GetGoalIndices(const PCGExData::FPointRef& Seed, TArray<int32>& OutIndices) const
{
	int32 Picks = NumGoalsGetter ? NumGoalsGetter->Values[Seed.Index] : NumGoals;

	if (GoalCount == EPCGExGoalPickRandomAmount::Random)
	{
		Picks = PCGExMath::Remap(
			FMath::PerlinNoise3D(PCGExMath::Tile(Seed.Point->Transform.GetLocation() * 0.001 + Picks, FVector(-1), FVector(1))),
			-1, 1, 0, Picks);
	}

	Picks = FMath::Min(1, FMath::Min(Picks, MaxGoalIndex));

	for (int i = 0; i < Picks; i++)
	{
		int32 Index = static_cast<int32>(PCGExMath::Remap(
			FMath::PerlinNoise3D(PCGExMath::Tile(Seed.Point->Transform.GetLocation() * 0.001 + i, FVector(-1), FVector(1))),
			-1, 1, 0, MaxGoalIndex));
		OutIndices.Add(PCGExMath::SanitizeIndex(Index, MaxGoalIndex, IndexSafety));
	}
}

bool UPCGExGoalPickerRandom::OutputMultipleGoals() const { return GoalCount != EPCGExGoalPickRandomAmount::Single; }

void UPCGExGoalPickerRandom::Cleanup()
{
	Super::Cleanup();
}

void UPCGExGoalPickerRandom::ApplyOverrides()
{
	Super::ApplyOverrides();
	PCGEX_OVERRIDE_OP_PROPERTY(NumGoals, FName(TEXT("Goals/NumGoals")), EPCGMetadataTypes::Integer32);
	PCGEX_OVERRIDE_OP_PROPERTY(bUseLocalNumGoals, FName(TEXT("Goals/UseLocalNumGoals")), EPCGMetadataTypes::Boolean);
}
