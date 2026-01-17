// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExValenceSocketCollection.h"

#include "Math/PCGExMath.h"

bool UPCGExValenceSocketCollection::Validate(TArray<FText>& OutErrors) const
{
	bool bValid = true;

	if (Sockets.Num() > 255)
	{
		OutErrors.Add(FText::Format(
			NSLOCTEXT("PCGExValence", "TooManySockets", "Socket collection has {0} sockets, maximum is 255."),
			FText::AsNumber(Sockets.Num())));
		bValid = false;
	}

	// Check for duplicate bitmasks and invalid refs
	TSet<int64> SeenBitmasks;

	for (int32 i = 0; i < Sockets.Num(); ++i)
	{
		const FPCGExValenceSocketEntry& Entry = Sockets[i];

		// Validate BitmaskRef
		FVector Direction;
		int64 Bitmask;
		if (!Entry.GetDirectionAndBitmask(Direction, Bitmask))
		{
			OutErrors.Add(FText::Format(
				NSLOCTEXT("PCGExValence", "InvalidBitmaskRef", "Socket {0}: BitmaskRef failed to resolve. Check that Source collection and Identifier are valid."),
				FText::AsNumber(i)));
			bValid = false;
			continue;
		}

		// Check for zero direction
		if (Direction.IsNearlyZero())
		{
			OutErrors.Add(FText::Format(
				NSLOCTEXT("PCGExValence", "ZeroDirection", "Socket {0} ({1}): Direction is zero or nearly zero."),
				FText::AsNumber(i),
				FText::FromName(Entry.GetSocketName())));
			bValid = false;
		}

		// Check for duplicate bitmasks
		if (SeenBitmasks.Contains(Bitmask))
		{
			OutErrors.Add(FText::Format(
				NSLOCTEXT("PCGExValence", "DuplicateBitmask", "Socket {0} ({1}): Duplicate bitmask value {2}. Each socket must have a unique bitmask."),
				FText::AsNumber(i),
				FText::FromName(Entry.GetSocketName()),
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

uint8 UPCGExValenceSocketCollection::FindMatchingSocket(const FVector& InDirection, bool bUseTransform, const FTransform& InTransform) const
{
	if (Sockets.Num() == 0)
	{
		return PCGExValence::NO_SOCKET_MATCH;
	}

	const double DotThreshold = PCGExMath::DegreesToDot(AngleThreshold);

	FVector TestDirection = InDirection.GetSafeNormal();
	if (bUseTransform && bTransformDirection)
	{
		TestDirection = InTransform.InverseTransformVectorNoScale(TestDirection);
	}

	int32 BestIndex = -1;
	double BestDot = DotThreshold;

	for (int32 i = 0; i < Sockets.Num(); ++i)
	{
		const FPCGExValenceSocketEntry& Entry = Sockets[i];

		FVector SocketDirection;
		int64 Bitmask;
		if (!Entry.GetDirectionAndBitmask(SocketDirection, Bitmask))
		{
			continue;
		}

		SocketDirection = SocketDirection.GetSafeNormal();
		const double Dot = FVector::DotProduct(TestDirection, SocketDirection);

		if (Dot >= BestDot)
		{
			BestDot = Dot;
			BestIndex = i;
		}
	}

	return BestIndex >= 0 ? static_cast<uint8>(BestIndex) : PCGExValence::NO_SOCKET_MATCH;
}

#if WITH_EDITOR
void UPCGExValenceSocketCollection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Could add validation feedback here
}
#endif
