// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

class UPCGPointData;

namespace PCGExGraph
{
	struct PCGEXTENDEDTOOLKIT_API FCachedSocketData
	{
		FCachedSocketData()
		{
			Neighbors.Empty();
		}

		int64 Index = -1;
		TArray<FSocketMetadata> Neighbors;
	};

	// Detail stored in a attribute array
	class PCGEXTENDEDTOOLKIT_API Helpers
	{
	public:
		/**
		 * Assume the edge already is neither None nor Unique, since another socket has been found.
		 * @param StartSocket 
		 * @param EndSocket 
		 * @return 
		 */
		static EPCGExEdgeType GetEdgeType(const FSocketInfos& StartSocket, const FSocketInfos& EndSocket)
		{
			if (StartSocket.Matches(EndSocket))
			{
				if (EndSocket.Matches(StartSocket))
				{
					return EPCGExEdgeType::Complete;
				}
				return EPCGExEdgeType::Match;
			}
			if (StartSocket.Socket->SocketIndex == EndSocket.Socket->SocketIndex)
			{
				// We check for mirror AFTER checking for shared/match, since Mirror can be considered a legal match by design
				// in which case we don't want to flag this as Mirrored.
				return EPCGExEdgeType::Mirror;
			}
			return EPCGExEdgeType::Shared;
		}
	};
}
