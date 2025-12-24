// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Core/PCGExAssetGrammar.h"

#include "UObject/Object.h"
#include "UObject/Package.h"
#include "Core/PCGExAssetCollection.h"
#include "Elements/Grammar/PCGSubdivisionBase.h"

double FPCGExAssetGrammarDetails::GetSize(const FBox& InBounds, TMap<const FPCGExAssetCollectionEntry*, double>* SizeCache) const
{
	const FVector S = InBounds.GetSize();

	switch (Size)
	{
	case EPCGExGrammarSizeReference::X: return S.X;
	case EPCGExGrammarSizeReference::Y: return S.Y;
	case EPCGExGrammarSizeReference::Z: return S.Z;
	case EPCGExGrammarSizeReference::Min: return FMath::Min3(S.X, S.Y, S.Z);
	case EPCGExGrammarSizeReference::Max: return FMath::Max3(S.X, S.Y, S.Z);
	case EPCGExGrammarSizeReference::Average: return (S.X + S.Y + S.Z) / 3;
	}

	return 0;
}

void FPCGExAssetGrammarDetails::Fix(const FBox& InBounds, FPCGSubdivisionSubmodule& OutSubmodule, TMap<const FPCGExAssetCollectionEntry*, double>* SizeCache) const
{
	OutSubmodule.Symbol = Symbol;
	OutSubmodule.DebugColor = DebugColor;
	OutSubmodule.bScalable = ScaleMode == EPCGExGrammarScaleMode::Flex;
	OutSubmodule.Size = GetSize(InBounds);
}

double FPCGExCollectionGrammarDetails::GetSize(const UPCGExAssetCollection* InCollection, TMap<const FPCGExAssetCollectionEntry*, double>* SizeCache) const
{
	if (SizeMode == EPCGExCollectionGrammarSize::Fixed)
	{
		return Size;
	}

	UPCGExAssetCollection* Collection = const_cast<UPCGExAssetCollection*>(InCollection);
	const PCGExAssetCollection::FCache* Cache = Collection->LoadCache();
	const int32 NumEntries = Cache->Main->Order.Num();

	double CompoundSize = 0;
	if (SizeMode == EPCGExCollectionGrammarSize::Min)
	{
		CompoundSize = MAX_dbl;

		for (int i = 0; i < NumEntries; i++)
		{
			if (FPCGExEntryAccessResult Result = Collection->GetEntryAt(i); Result.IsValid())
			{
				CompoundSize = FMath::Min(CompoundSize, Result.Entry->GetGrammarSize(Result.Host, SizeCache));
			}
		}
	}
	else if (SizeMode == EPCGExCollectionGrammarSize::Max)
	{
		CompoundSize = 0;

		for (int i = 0; i < NumEntries; i++)
		{
			if (FPCGExEntryAccessResult Result = Collection->GetEntryAt(i); Result.IsValid())
			{
				CompoundSize = FMath::Max(CompoundSize, Result.Entry->GetGrammarSize(Result.Host, SizeCache));
			}
		}
	}
	else if (SizeMode == EPCGExCollectionGrammarSize::Average)
	{
		double NumSamples = 0;

		for (int i = 0; i < NumEntries; i++)
		{
			if (FPCGExEntryAccessResult Result = Collection->GetEntryAt(i); Result.IsValid())
			{
				CompoundSize += Result.Entry->GetGrammarSize(Result.Host, SizeCache);
				NumSamples++;
			}
		}

		CompoundSize /= NumSamples;
	}

	return CompoundSize;
}

void FPCGExCollectionGrammarDetails::Fix(const UPCGExAssetCollection* InCollection, FPCGSubdivisionSubmodule& OutSubmodule, TMap<const FPCGExAssetCollectionEntry*, double>* SizeCache) const
{
	OutSubmodule.Symbol = Symbol;
	OutSubmodule.DebugColor = DebugColor;
	OutSubmodule.Size = GetSize(InCollection);
	OutSubmodule.bScalable = ScaleMode == EPCGExGrammarScaleMode::Flex;
}
