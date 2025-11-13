// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Collections/PCGExMeshGrammar.h"

#include "UObject/Object.h"
#include "UObject/Package.h"
#include "Collections/PCGExAssetCollection.h"
#include "Collections/PCGExMeshCollection.h"
#include "Details/PCGExDetailsDistances.h"
#include "Details/PCGExDetailsSettings.h"
#include "Elements/Grammar/PCGSubdivisionBase.h"

void FPCGExMeshGrammarDetails::Fix(const FBox& InBounds, FPCGSubdivisionSubmodule& OutSubmodule) const
{
	OutSubmodule.Symbol = Symbol;
	OutSubmodule.DebugColor = DebugColor;
	OutSubmodule.bScalable = ScaleMode == EPCGExGrammarScaleMode::Flex;

	const FVector S = InBounds.GetSize();
	switch (Size)
	{
	case EPCGExGrammarSizeReference::X:
		OutSubmodule.Size = S.X;
		break;
	case EPCGExGrammarSizeReference::Y:
		OutSubmodule.Size = S.X;
		break;
	case EPCGExGrammarSizeReference::Z:
		OutSubmodule.Size = S.Y;
		break;
	case EPCGExGrammarSizeReference::Min:
		OutSubmodule.Size = FMath::Min3(S.X, S.Y, S.Z);
		break;
	case EPCGExGrammarSizeReference::Max:
		OutSubmodule.Size = FMath::Max3(S.X, S.Y, S.Z);
		break;
	case EPCGExGrammarSizeReference::Average:
		OutSubmodule.Size = (S.X + S.Y + S.Z) / 3;
		break;
	}
}

void FPCGExMeshCollectionGrammarDetails::Fix(const UPCGExMeshCollection* InCollection, FPCGSubdivisionSubmodule& OutSubmodule) const
{
	
	if (SizeMode == EPCGExCollectionGrammarSize::Fixed)
	{
		
	}
}

namespace PCGExMeshGrammar
{
	void FixModule(const FPCGExMeshCollectionEntry* Entry, const UPCGExMeshCollection* Collection, FPCGSubdivisionSubmodule& OutSubmodule)
	{
		if (Entry->bIsSubCollection)
		{
			if (Entry->bOverrideSubCollectionGrammar)
			{
				// Entry settings override sub collection internal settings		
			}
			else
			{
				// Entry settings grabs sub collection internal settings
			}
		}
		else
		{
			if (Entry->GrammarSource == EPCGExEntryVariationMode::Global
				|| Collection->GlobalGrammarMode == EPCGExGlobalVariationRule::Overrule)
			{
				// Grab parent collection global mesh settings
				Collection->GlobalMeshGrammar.Fix(Entry->Staging.Bounds, OutSubmodule);
			}
			else
			{
				Entry->MeshGrammar.Fix(Entry->Staging.Bounds, OutSubmodule);
			}
		}
	}
}
