// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExValencyOrbitalSet.h"

#include "Math/PCGExMath.h"

bool UPCGExValencyOrbitalSet::Validate(TArray<FText>& OutErrors) const
{
	bool bValid = true;

	if (Orbitals.Num() > 255)
	{
		OutErrors.Add(FText::Format(
			NSLOCTEXT("PCGExValency", "TooManyOrbitals", "Orbital set has {0} orbitals, maximum is 255."),
			FText::AsNumber(Orbitals.Num())));
		bValid = false;
	}

	// Check for duplicate bitmasks and invalid refs
	TSet<int64> SeenBitmasks;

	for (int32 i = 0; i < Orbitals.Num(); ++i)
	{
		const FPCGExValencyOrbitalEntry& Entry = Orbitals[i];

		// Validate BitmaskRef
		FVector Direction;
		int64 Bitmask;
		if (!Entry.GetDirectionAndBitmask(Direction, Bitmask))
		{
			OutErrors.Add(FText::Format(
				NSLOCTEXT("PCGExValency", "InvalidBitmaskRef", "Orbital {0}: BitmaskRef failed to resolve. Check that Source collection and Identifier are valid."),
				FText::AsNumber(i)));
			bValid = false;
			continue;
		}

		// Check for zero direction
		if (Direction.IsNearlyZero())
		{
			OutErrors.Add(FText::Format(
				NSLOCTEXT("PCGExValency", "ZeroDirection", "Orbital {0} ({1}): Direction is zero or nearly zero."),
				FText::AsNumber(i),
				FText::FromName(Entry.GetOrbitalName())));
			bValid = false;
		}

		// Check for duplicate bitmasks
		if (SeenBitmasks.Contains(Bitmask))
		{
			OutErrors.Add(FText::Format(
				NSLOCTEXT("PCGExValency", "DuplicateBitmask", "Orbital {0} ({1}): Duplicate bitmask value {2}. Each orbital must have a unique bitmask."),
				FText::AsNumber(i),
				FText::FromName(Entry.GetOrbitalName()),
				FText::AsNumber(Bitmask)));
			bValid = false;
		}
		else
		{
			SeenBitmasks.Add(Bitmask);
		}
	}

	return bValid;
}

uint8 UPCGExValencyOrbitalSet::FindMatchingOrbital(const FVector& InDirection, bool bUseTransform, const FTransform& InTransform) const
{
	if (Orbitals.Num() == 0)
	{
		return PCGExValency::NO_ORBITAL_MATCH;
	}

	const double DotThreshold = PCGExMath::DegreesToDot(AngleThreshold);

	FVector TestDirection = InDirection.GetSafeNormal();
	if (bUseTransform && bTransformDirection)
	{
		TestDirection = InTransform.InverseTransformVectorNoScale(TestDirection);
	}

	int32 BestIndex = -1;
	double BestDot = DotThreshold;

	for (int32 i = 0; i < Orbitals.Num(); ++i)
	{
		const FPCGExValencyOrbitalEntry& Entry = Orbitals[i];

		FVector OrbitalDirection;
		int64 Bitmask;
		if (!Entry.GetDirectionAndBitmask(OrbitalDirection, Bitmask))
		{
			continue;
		}

		OrbitalDirection = OrbitalDirection.GetSafeNormal();
		const double Dot = FVector::DotProduct(TestDirection, OrbitalDirection);

		if (Dot >= BestDot)
		{
			BestDot = Dot;
			BestIndex = i;
		}
	}

	return BestIndex >= 0 ? static_cast<uint8>(BestIndex) : PCGExValency::NO_ORBITAL_MATCH;
}

#if WITH_EDITOR
void UPCGExValencyOrbitalSet::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Could add validation feedback here
}
#endif

//////// FOrbitalCache

bool PCGExValency::FOrbitalCache::BuildFrom(const UPCGExValencyOrbitalSet* OrbitalSet)
{
	if (!OrbitalSet || OrbitalSet->Orbitals.Num() == 0)
	{
		return false;
	}

	const int32 NumOrbitals = OrbitalSet->Orbitals.Num();
	Directions.SetNum(NumOrbitals);
	Bitmasks.SetNum(NumOrbitals);

	// Pre-compute dot threshold from angle
	DotThreshold = PCGExMath::DegreesToDot(OrbitalSet->AngleThreshold);
	bTransformOrbital = OrbitalSet->bTransformDirection;

	// Resolve all orbital directions and bitmasks upfront
	for (int32 i = 0; i < NumOrbitals; ++i)
	{
		const FPCGExValencyOrbitalEntry& Entry = OrbitalSet->Orbitals[i];

		FVector Direction;
		int64 Bitmask;
		if (!Entry.GetDirectionAndBitmask(Direction, Bitmask))
		{
			// Failed to resolve - clear and return false
			Directions.Empty();
			Bitmasks.Empty();
			return false;
		}

		Directions[i] = Direction.GetSafeNormal();
		Bitmasks[i] = Bitmask;
	}

	return true;
}

uint8 PCGExValency::FOrbitalCache::FindMatchingOrbital(const FVector& InDirection, bool bUseTransform, const FTransform& InTransform) const
{
	if (Directions.Num() == 0)
	{
		return NO_ORBITAL_MATCH;
	}

	FVector TestDirection = InDirection.GetSafeNormal();
	if (bUseTransform && bTransformOrbital)
	{
		TestDirection = InTransform.InverseTransformVectorNoScale(TestDirection);
	}

	int32 BestIndex = -1;
	double BestDot = DotThreshold;

	for (int32 i = 0; i < Directions.Num(); ++i)
	{
		const double Dot = FVector::DotProduct(TestDirection, Directions[i]);

		if (Dot >= BestDot)
		{
			BestDot = Dot;
			BestIndex = i;
		}
	}

	return BestIndex >= 0 ? static_cast<uint8>(BestIndex) : NO_ORBITAL_MATCH;
}
