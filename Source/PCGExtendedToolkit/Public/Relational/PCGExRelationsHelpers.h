// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGExRelationsProcessor.h"
#include "PCGExRelationsProcessor.h"
#include "Data/PCGExRelationsParamsData.h"
//#include "PCGExRelationsHelpers.generated.h"

class UPCGPointData;

namespace PCGExRelational
{

	const FName SourceParamsLabel = TEXT("RelationalParams");
	const FName OutputParamsLabel = TEXT("→");

	struct PCGEXTENDEDTOOLKIT_API FParamsInputs
	{
		FParamsInputs()
		{
			Params.Empty();
			ParamsSources.Empty();
		}

		FParamsInputs(FPCGContext* Context, FName InputLabel): FParamsInputs()
		{
			TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(InputLabel);
			Initialize(Context, Sources);
		}

		FParamsInputs(FPCGContext* Context, TArray<FPCGTaggedData>& Sources): FParamsInputs()
		{
			Initialize(Context, Sources);
		}

	public:
		TArray<UPCGExRelationsParamsData*> Params;
		TArray<FPCGTaggedData> ParamsSources;

		/**
		 * Initialize from Sources
		 * @param Context 
		 * @param Sources 
		 * @param bInitializeOutput 
		 */
		void Initialize(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, bool bInitializeOutput = false)
		{
			Params.Empty(Sources.Num());
			TSet<uint64> UniqueParams;
			for (FPCGTaggedData& Source : Sources)
			{
				const UPCGExRelationsParamsData* ParamsData = Cast<UPCGExRelationsParamsData>(Source.Data);
				if (!ParamsData) { continue; }
				if(UniqueParams.Contains(ParamsData->UID)){continue;}
				UniqueParams.Add(ParamsData->UID);
				Params.Add(const_cast<UPCGExRelationsParamsData*>(ParamsData));
				ParamsSources.Add(Source);
			}
			UniqueParams.Empty();
		}

		void ForEach(FPCGContext* Context, const TFunction<void(UPCGExRelationsParamsData*, const int32)>& BodyLoop)
		{
			for (int i = 0; i < Params.Num(); i++)
			{
				UPCGExRelationsParamsData* ParamsData = Params[i];
				BodyLoop(ParamsData, i);
			}
		}

		void OutputTo(FPCGContext* Context) const
		{
			for(int i = 0; i < ParamsSources.Num(); i++)
			{
				FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Add_GetRef(ParamsSources[i]);
				OutputRef.Pin = PCGExRelational::OutputParamsLabel;
				OutputRef.Data = Params[i];
			}
		}

		bool IsEmpty() const { return Params.IsEmpty(); }

		~FParamsInputs()
		{
			Params.Empty();
			ParamsSources.Empty();
		}
	};

	// Detail stored in a attribute array
	class PCGEXTENDEDTOOLKIT_API Helpers
	{
	public:

		/**
		 * Assume the relation already is neither None nor Unique, since another socket has been found.
		 * @param StartSocket 
		 * @param EndSocket 
		 * @return 
		 */
		static EPCGExRelationType GetRelationType(const FSocketInfos& StartSocket, const FSocketInfos& EndSocket)
		{
			if (StartSocket.Socket->MatchingSockets.Contains(EndSocket.Socket->SocketIndex))
			{
				if (EndSocket.Socket->MatchingSockets.Contains(StartSocket.Socket->SocketIndex))
				{
					return EPCGExRelationType::Complete;
				}
				else
				{
					return EPCGExRelationType::Match;
				}
			}
			else
			{
				if (StartSocket.Socket->SocketIndex == EndSocket.Socket->SocketIndex)
				{
					// We check for mirror AFTER checking for shared/match, since Mirror can be considered a legal match by design
					// in which case we don't want to flag this as Mirrored.
					return EPCGExRelationType::Mirror;
				}
				else
				{
					return EPCGExRelationType::Shared;
				}
			}
		}
	};
}
