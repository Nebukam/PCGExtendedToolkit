// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/External/PCGExMeshImportDetails.h"

#include "Data/External/PCGExMeshCommon.h"
#include "Helpers/PCGExAttributeMapHelpers.h"
#include "Helpers/PCGExMetaHelpers.h"

bool FPCGExGeoMeshImportDetails::Validate(FPCGExContext* InContext)
{
	if (bImportUVs)
	{
		PCGExAttributeMapHelpers::BuildMap(InContext, PCGExMesh::Labels::SourceUVImportRulesLabel, UVChannels);

		if (UVChannels.IsEmpty())
		{
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Import UV channel is true, but there is no import details."));
			return true;
		}

		TSet<FName> UniqueNames;
		UVChannelIndex.Reserve(UVChannels.Num());
		for (const TPair<FName, int32>& Pair : UVChannels)
		{
			if (Pair.Value < 0)
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("A channel mapping has an illegal channel index (< 0) and will be ignored."));
				continue;
			}

			if (Pair.Value > 7)
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("A channel mapping has an illegal channel index (> 7) and will be ignored."));
				continue;
			}

			bool bNameAlreadyExists = false;
			UniqueNames.Add(Pair.Key, &bNameAlreadyExists);
			if (bNameAlreadyExists)
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("A channel name is used more than once. Only the first entry will be used."));
				continue;
			}

			if (!PCGExMetaHelpers::IsWritableAttributeName(Pair.Key))
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("A channel name is not a valid attribute name, it will be ignored."));
				continue;
			}

			UVChannelId.Add(FPCGAttributeIdentifier(Pair.Key, PCGMetadataDomainID::Elements));
			UVChannelIndex.Add(Pair.Value);
		}
	}

	return true;
}

bool FPCGExGeoMeshImportDetails::WantsImport() const
{
	return bImportVertexColor || !UVChannels.IsEmpty();
}
