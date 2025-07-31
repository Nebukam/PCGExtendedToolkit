// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#define DBL_INTERSECTION_TOLERANCE 0.1
#define DBL_COLLOCATION_TOLERANCE 0.1
#define DBL_COMPARE_TOLERANCE 0.01

namespace PCGExData
{
	template<typename T>
	class TDataValue;
}

namespace PCGExCommon
{
	using DataIDType = TSharedPtr<PCGExData::TDataValue<int32>>;
	using ContextState = uint64;

	const FString PCGExPrefix = TEXT("PCGEx/");
	
#define PCGEX_CTX_STATE(_NAME) const PCGExCommon::ContextState _NAME = GetTypeHash(FName(#_NAME));

	PCGEX_CTX_STATE(State_Preparation)
	PCGEX_CTX_STATE(State_LoadingAssetDependencies)
	PCGEX_CTX_STATE(State_AsyncPreparation)
	PCGEX_CTX_STATE(State_FacadePreloading)

	PCGEX_CTX_STATE(State_InitialExecution)
	PCGEX_CTX_STATE(State_ReadyForNextPoints)
	PCGEX_CTX_STATE(State_ProcessingPoints)

	PCGEX_CTX_STATE(State_WaitingOnAsyncWork)
	PCGEX_CTX_STATE(State_Done)

	PCGEX_CTX_STATE(State_Processing)
	PCGEX_CTX_STATE(State_Completing)
	PCGEX_CTX_STATE(State_Writing)

	PCGEX_CTX_STATE(State_UnionWriting)
}
