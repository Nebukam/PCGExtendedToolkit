// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExPathfinding.generated.h"

UENUM(BlueprintType)
enum class EPCGExPathfindingGoalPickMethod : uint8
{
	SeedIndex UMETA(DisplayName = "Seed Index", Tooltip="Uses the seed index as goal index."),
	LocalAttribute UMETA(DisplayName = "Attribute", Tooltip="Uses a local attribute of the seed as goal index. Value is wrapped."),
	RandomPick UMETA(DisplayName = "Random Pick", Tooltip="Picks the goal randomly."),
	MultipleLocalAttribute UMETA(DisplayName = "Attribute (Multiple)", Tooltip="Uses a multiple local attribute of the seed as goal indices. Each seed will create multiple paths."),
	All UMETA(DisplayName = "All", Tooltip="Each seed will create a path for each goal."),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPathfindingGoalPickingSettings
{
	GENERATED_BODY()

	FPCGExPathfindingGoalPickingSettings()
	{
		Attributes.Empty();
		AttributeGetters.Empty();
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExPathfindingGoalPickMethod Method = EPCGExPathfindingGoalPickMethod::SeedIndex;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Wrap;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Method==EPCGExPathfindingGoalPickMethod::LocalAttribute", EditConditionHides))
	FPCGExInputDescriptorWithSingleField Attribute;
	PCGEx::FLocalSingleFieldGetter AttributeGetter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(TitleProperty="{HiddenDisplayName}", EditCondition="Method==EPCGExPathfindingGoalPickMethod::MultipleLocalAttribute", EditConditionHides))
	TArray<FPCGExInputDescriptorWithSingleField> Attributes;
	TArray<PCGEx::FLocalSingleFieldGetter> AttributeGetters;

	int32 MaxGoalIndex = -1;

	void PrepareForData(const UPCGPointData* InData)
	{
		MaxGoalIndex = InData->GetPoints().Num() - 1;
		if (Method == EPCGExPathfindingGoalPickMethod::MultipleLocalAttribute)
		{
			AttributeGetters.Reset(Attributes.Num());
			for (FPCGExInputDescriptorWithSingleField& Descriptor : Attributes)
			{
				PCGEx::FLocalSingleFieldGetter& Getter = AttributeGetters.Emplace_GetRef();
				Getter.Capture(Descriptor);
				Getter.Validate(InData);
			}
		}
		else if (Method == EPCGExPathfindingGoalPickMethod::LocalAttribute)
		{
			AttributeGetter.Capture(Attribute);
			Attribute.Validate(InData);
		}
	}

	int32 GetGoalIndex(const FPCGPoint& Seed, const int32 SeedIndex) const
	{
		int32 Index = -1;
		if (MaxGoalIndex > -1)
		{
			switch (Method)
			{
			default:
			case EPCGExPathfindingGoalPickMethod::SeedIndex:
				Index = SeedIndex;
				break;
			case EPCGExPathfindingGoalPickMethod::LocalAttribute:
				Index = AttributeGetter.GetValueSafe(Seed, -1);
				break;
			case EPCGExPathfindingGoalPickMethod::RandomPick:

				Index = static_cast<int32>(PCGExMath::Remap(
					FMath::PerlinNoise3D(PCGExMath::CWWrap(Seed.Transform.GetLocation() * 0.001, FVector(-1), FVector(1))),
					-1, 1, 0, MaxGoalIndex));
				break;
			case EPCGExPathfindingGoalPickMethod::MultipleLocalAttribute:
				break;
			case EPCGExPathfindingGoalPickMethod::All:
				break;
			}

			PCGEx::SanitizeIndex(Index, MaxGoalIndex, IndexSafety);
		}

		return Index;
	}

	void GetGoalIndices(const FPCGPoint& Seed, TArray<int32>& OutIndices)
	{
		if (Method == EPCGExPathfindingGoalPickMethod::MultipleLocalAttribute)
		{
			OutIndices.Reset(AttributeGetters.Num());
			for (const PCGEx::FLocalSingleFieldGetter& Getter : AttributeGetters)
			{
				int32 Index = Getter.GetValueSafe(Seed, -1);
				PCGEx::SanitizeIndex(Index, MaxGoalIndex, IndexSafety);
				OutIndices.Add(Index);
			}
		}
		else if (Method == EPCGExPathfindingGoalPickMethod::All)
		{
			OutIndices.Reset(MaxGoalIndex + 1);
			for (int i = 0; i <= MaxGoalIndex; i++) { OutIndices.Add(i); }
		}
	}

	bool IsMultiPick() const
	{
		return Method == EPCGExPathfindingGoalPickMethod::MultipleLocalAttribute ||
			Method == EPCGExPathfindingGoalPickMethod::All;
	}

	void PrintDisplayNames()
	{
		Attribute.PrintDisplayName();
		for (FPCGExInputDescriptorWithSingleField& Descriptor : Attributes) { Descriptor.PrintDisplayName(); }
	}
};


namespace PCGExPathfinding
{
	const FName SourceSeedsLabel = TEXT("Seeds");
	const FName SourceGoalsLabel = TEXT("Goals");
}
