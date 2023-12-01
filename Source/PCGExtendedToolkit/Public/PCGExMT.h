// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGContext.h"
#include "Helpers/PCGAsync.h"
#include "Metadata/PCGMetadataCommon.h"

namespace PCGExMT
{

	typedef int64 AsyncState;

	constexpr AsyncState State_Setup = TNumericLimits<int64>::Min();
	constexpr AsyncState State_ReadyForNextPoints = 1;
	constexpr AsyncState State_ProcessingPoints = 2;
	constexpr AsyncState State_WaitingOnAsyncWork = 3;
	constexpr AsyncState State_Done = TNumericLimits<int64>::Max();
	

	struct PCGEXTENDEDTOOLKIT_API FTaskInfos
	{
		FTaskInfos()
		{
		}

		FTaskInfos(int32 InIndex, PCGMetadataEntryKey InKey, int32 InAttempt = 0):
			Index(InIndex), Key(InKey), Attempt(InAttempt)
		{
		}

		FTaskInfos(const FTaskInfos& Other):
			Index(Other.Index), Key(Other.Key), Attempt(Other.Attempt)
		{
		}

		FTaskInfos GetRetry() const { return FTaskInfos(Index, Key, Attempt + 1); }

		int32 Index = -1;
		PCGMetadataEntryKey Key = PCGInvalidEntryKey;
		int32 Attempt = 0;
	};

	/**
	 * 
	 * @tparam LoopBodyFunc 
	 * @param Context The context containing the AsyncState
	 * @param NumIterations The number of calls that will be done to the provided function, also an upper bound on the number of data generated.
	 * @param Initialize Called once when the processing starts
	 * @param LoopBody Signature: bool(int32 ReadIndex, int32 WriteIndex).
	 * @param ChunkSize Size of the chunks to cut the input data with
	 * @param bForceSync
	 * @return 
	 */
	static bool ParallelForLoop
		(
		FPCGContext* Context,
		const int32 NumIterations,
		TFunction<void()>&& Initialize,
		TFunction<void(int32)>&& LoopBody,
		const int32 ChunkSize = 32,
		bool bForceSync = false)
	{
		if (bForceSync)
		{
			Initialize();
			for (int i = 0; i < NumIterations; i++) { LoopBody(i); }
			return true;
		}

		auto InnerBodyLoop = [&](int32 ReadIndex, int32 WriteIndex)
		{
			LoopBody(ReadIndex);
			return true;
		};
		return FPCGAsync::AsyncProcessingOneToOneEx(&(Context->AsyncState), NumIterations, Initialize, InnerBodyLoop, true, ChunkSize);
	}

	static bool ParallelForLoop
		(
		FPCGContext* Context,
		const int32 NumIterations,
		TFunction<void(int32)>&& LoopBody,
		const int32 ChunkSize = 32,
		bool bForceSync = false)
	{
		if (bForceSync)
		{
			for (int i = 0; i < NumIterations; i++) { LoopBody(i); }
			return true;
		}

		auto InnerBodyLoop = [&](int32 ReadIndex, int32 WriteIndex)
		{
			LoopBody(ReadIndex);
			return true;
		};
		return FPCGAsync::AsyncProcessingOneToOneEx(
			&(Context->AsyncState), NumIterations, []()
			{
			}, InnerBodyLoop, true, ChunkSize);
	}
}
