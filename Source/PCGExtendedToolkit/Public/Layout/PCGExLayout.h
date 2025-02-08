// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExLayout.generated.h"

UENUM()
enum class EPCGExBinSeedMode : uint8
{
	UVWConstant       = 0 UMETA(DisplayName = "UVW (Constant)", ToolTip="A constant bound-relative position"),
	UVWAttribute      = 1 UMETA(DisplayName = "UVW", ToolTip="A per-bin bound-relative position"),
	PositionConstant  = 2 UMETA(DisplayName = "Position (Constant)", ToolTip="A constant world position"),
	PositionAttribute = 3 UMETA(DisplayName = "Position (Attribute)", ToolTip="A per-bin world position"),
};

namespace PCGExLayout
{
	const FName SourceBinsLabel = TEXT("Bins");
	const FName OutputBinsLabel = TEXT("Bins");
	const FName OutputDiscardedLabel = TEXT("Discarded");

	struct FItem
	{
		int32 Index = 0;
		FBox Box = FBox(ForceInit);
		FVector Padding = FVector::ZeroVector;
	};

	struct FSpace
	{
		FBox Box = FBox(ForceInit);
		FVector Size = FVector::ZeroVector;
		FVector CoG = FVector::ZeroVector;
		double DistanceScore = 0;
		double Volume = 0;

		explicit FSpace(const FBox& InBox, const FVector& InSeed) : Box(InBox)
		{
			Volume = Box.GetVolume();
			Size = Box.GetSize();

			for (int C = 0; C < 3; C++) { CoG[C] = FMath::Clamp(InSeed[C], Box.Min[C], Box.Max[C]); }
			DistanceScore = FVector::DistSquared(InSeed, CoG);
		}

		~FSpace() = default;

		bool CanFit(const FVector& InTestSize) const;
		void Expand(FBox& InBox, const FVector& Expansion) const;
		FVector Inflate(FBox& InBox, const FVector& Thresholds) const;
	};
}
