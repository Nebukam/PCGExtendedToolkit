// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPointIO.h"

#include "Helpers/PCGAsync.h"

UPCGExPointIO::UPCGExPointIO(): In(nullptr), Out(nullptr), NumPoints(-1)
{
	// Initialize other members as needed
}

void UPCGExPointIO::InitializeOut(PCGExIO::EInitMode InitOut)
{
	switch (InitOut)
	{
	case PCGExIO::EInitMode::NoOutput:
		break;
	case PCGExIO::EInitMode::NewOutput:
		Out = NewObject<UPCGPointData>();
		if (In) { Out->InitializeFromData(In); }
		break;
	case PCGExIO::EInitMode::DuplicateInput:
		if (In)
		{
			Out = Cast<UPCGPointData>(In->DuplicateData(true));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Initialize::Duplicate, but no Input."));
		}
		break;
	case PCGExIO::EInitMode::Forward:
		Out = In;
		break;
	default: ;
	}
	if (In) { NumPoints = In->GetPoints().Num(); }
}

void UPCGExPointIO::BuildIndices()
{
	if (bMetadataEntryDirty) { BuildMetadataEntries(); }
	if (!bIndicesDirty) { return; }
	FWriteScopeLock WriteLock(MapLock);
	const TArray<FPCGPoint>& Points = Out->GetPoints();
	IndicesMap.Empty(NumPoints);
	for (int i = 0; i < NumPoints; i++) { IndicesMap.Add(Points[i].MetadataEntry, i); }
	bIndicesDirty = false;
}

void UPCGExPointIO::BuildMetadataEntries()
{
	if (!bMetadataEntryDirty) { return; }
	TArray<FPCGPoint>& Points = Out->GetMutablePoints();
	for (int i = 0; i < NumPoints; i++)
	{
		FPCGPoint& Point = Points[i];
		Out->Metadata->InitializeOnSet(Point.MetadataEntry, In->GetPoint(i).MetadataEntry, In->Metadata);
	}
	bMetadataEntryDirty = false;
	bIndicesDirty = true;
}

void UPCGExPointIO::BuildMetadataEntriesAndIndices()
{
	if (bMetadataEntryDirty) { BuildMetadataEntries(); }
	if (!bIndicesDirty) { return; }
	FWriteScopeLock WriteLock(MapLock);
	TArray<FPCGPoint>& Points = Out->GetMutablePoints();
	IndicesMap.Empty(NumPoints);
	for (int i = 0; i < NumPoints; i++)
	{
		FPCGPoint& Point = Points[i];
		Out->Metadata->InitializeOnSet(Point.MetadataEntry, In->GetPoint(i).MetadataEntry, In->Metadata);
		IndicesMap.Add(Point.MetadataEntry, i);
	}
	bIndicesDirty = false;
}

void UPCGExPointIO::ClearIndices()
{
	IndicesMap.Empty();
}

int32 UPCGExPointIO::GetIndex(PCGMetadataEntryKey Key) const
{
	FReadScopeLock ReadLock(MapLock);
	return *(IndicesMap.Find(Key));
}

template <class InitializeFunc, class ProcessElementFunc>
bool UPCGExPointIO::OutputParallelProcessing(
	FPCGContext* Context,
	InitializeFunc&& Initialize,
	ProcessElementFunc&& LoopBody,
	const int32 ChunkSize,
	const bool bForceSync)
{
	if (bForceSync)
	{
		Initialize(this);
		const int32 NumIterations = Out->GetPoints().Num();
		for (int i = 0; i < NumIterations; i++)
		{
			const FPCGPoint& Point = Out->GetPoint(i);
			LoopBody(Point, i, this);
		}
		return true;
	}

	auto InnerInitialize = [&]()
	{
		Initialize(this);
	};

	auto InnerBodyLoop = [&](const int32 ReadIndex, const int32 WriteIndex)
	{
		FReadScopeLock ReadLock(MapLock);
		const FPCGPoint& Point = Out->GetPoints()[ReadIndex];
		LoopBody(Point, ReadIndex, this);
		//if (bIndicesDirty) { LoopBody(Point, ReadIndex, this); }
		//else { LoopBody(Point, *(IndicesMap.Find(Point.MetadataEntry)), this); }
		return true;
	};

	return FPCGAsync::AsyncProcessingOneToOneEx(&(Context->AsyncState), NumPoints, InnerInitialize, InnerBodyLoop, true, ChunkSize);
}

template <class InitializeFunc, class ProcessElementFunc>
bool UPCGExPointIO::InputParallelProcessing(
	FPCGContext* Context,
	InitializeFunc&& Initialize,
	ProcessElementFunc&& LoopBody,
	const int32 ChunkSize,
	bool bForceSync)
{
	if (bForceSync)
	{
		Initialize(this);
		const int32 NumIterations = In->GetPoints().Num();
		for (int i = 0; i < NumIterations; i++)
		{
			const FPCGPoint& Point = In->GetPoints()[i];
			LoopBody(Point, i, this);
		}
		return true;
	}

	auto InnerInitialize = [&]()
	{
		Initialize(this);
	};

	auto InnerBodyLoop = [&](const int32 ReadIndex, const int32 WriteIndex)
	{
		FReadScopeLock ReadLock(MapLock);
		const FPCGPoint& Point = In->GetPoint(ReadIndex);
		LoopBody(Point, ReadIndex, this);
		return true;
	};

	return FPCGAsync::AsyncProcessingOneToOneEx(&(Context->AsyncState), NumPoints, InnerInitialize, InnerBodyLoop, true, ChunkSize);
}

bool UPCGExPointIO::OutputTo(FPCGContext* Context, bool bEmplace)
{
	if (!Out || Out->GetPoints().Num() == 0) { return false; }

	if (!bEmplace)
	{
		if (!In)
		{
			UE_LOG(LogTemp, Error, TEXT("OutputTo, bEmplace==false but no Input."));
			return false;
		}

		FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Add_GetRef(Source);
		OutputRef.Data = Out;
		OutputRef.Pin = DefaultOutputLabel;
		Output = OutputRef;
	}
	else
	{
		FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Emplace_GetRef();
		OutputRef.Data = Out;
		OutputRef.Pin = DefaultOutputLabel;
		Output = OutputRef;
	}
	return true;
}

bool UPCGExPointIO::OutputTo(FPCGContext* Context, bool bEmplace, const int64 MinPointCount, const int64 MaxPointCount)
{
	if (!Out) { return false; }

	const int64 OutNumPoints = Out->GetPoints().Num();
	if (MinPointCount >= 0 && OutNumPoints < MinPointCount) { return false; }
	if (MaxPointCount >= 0 && OutNumPoints > MaxPointCount) { return false; }

	return OutputTo(Context, bEmplace);
}

UPCGExPointIOGroup::UPCGExPointIOGroup()
{
}

UPCGExPointIOGroup::UPCGExPointIOGroup(FPCGContext* Context, FName InputLabel, PCGExIO::EInitMode InitOut)
	: UPCGExPointIOGroup::UPCGExPointIOGroup()
{
	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(InputLabel);
	Initialize(Context, Sources, InitOut);
}

UPCGExPointIOGroup::UPCGExPointIOGroup(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, PCGExIO::EInitMode InitOut)
	: UPCGExPointIOGroup::UPCGExPointIOGroup()
{
	Initialize(Context, Sources, InitOut);
}

void UPCGExPointIOGroup::Initialize(
	FPCGContext* Context, TArray<FPCGTaggedData>& Sources,
	PCGExIO::EInitMode InitOut)
{
	Pairs.Empty(Sources.Num());
	for (FPCGTaggedData& Source : Sources)
	{
		UPCGPointData* MutablePointData = GetMutablePointData(Context, Source);
		if (!MutablePointData || MutablePointData->GetPoints().Num() == 0) { continue; }
		Emplace_GetRef(Source, MutablePointData, InitOut);
	}
}

void UPCGExPointIOGroup::Initialize(
	FPCGContext* Context, TArray<FPCGTaggedData>& Sources,
	PCGExIO::EInitMode InitOut,
	const TFunction<bool(UPCGPointData*)>& ValidateFunc,
	const TFunction<void(UPCGExPointIO*)>& PostInitFunc)
{
	Pairs.Empty(Sources.Num());
	for (FPCGTaggedData& Source : Sources)
	{
		UPCGPointData* MutablePointData = GetMutablePointData(Context, Source);
		if (!MutablePointData || MutablePointData->GetPoints().Num() == 0) { continue; }
		if (!ValidateFunc(MutablePointData)) { continue; }
		UPCGExPointIO* NewPointIO = Emplace_GetRef(Source, MutablePointData, InitOut);
		PostInitFunc(NewPointIO);
	}
}

UPCGExPointIO* UPCGExPointIOGroup::Emplace_GetRef(
	const UPCGExPointIO& PointIO,
	const PCGExIO::EInitMode InitOut)
{
	return Emplace_GetRef(PointIO.Source, PointIO.In, InitOut);
}

UPCGExPointIO* UPCGExPointIOGroup::Emplace_GetRef(
	const FPCGTaggedData& Source, UPCGPointData* In,
	const PCGExIO::EInitMode InitOut)
{
	//FWriteScopeLock WriteLock(PairsLock);

	UPCGExPointIO* Pair = NewObject<UPCGExPointIO>();

	{
		FWriteScopeLock WriteLock(PairsLock);
		Pairs.Add(Pair);
	}

	Pair->DefaultOutputLabel = DefaultOutputLabel;
	Pair->Source = Source;
	Pair->In = In;
	Pair->NumPoints = Pair->In->GetPoints().Num();

	Pair->InitializeOut(InitOut);
	return Pair;
}

/**
 * Write valid outputs to Context' tagged data
 * @param Context
 * @param bEmplace Emplace will create a new entry no matter if a Source is set, otherwise will match the In.Source. 
 */
void UPCGExPointIOGroup::OutputTo(FPCGContext* Context, const bool bEmplace)
{
	for (UPCGExPointIO* Pair : Pairs)
	{
		Pair->OutputTo(Context, bEmplace);
	}
}

/**
 * Write valid outputs to Context' tagged data
 * @param Context
 * @param bEmplace Emplace will create a new entry no matter if a Source is set, otherwise will match the In.Source.
 * @param MinPointCount
 * @param MaxPointCount 
 */
void UPCGExPointIOGroup::OutputTo(FPCGContext* Context, bool bEmplace, const int64 MinPointCount, const int64 MaxPointCount)
{
	for (UPCGExPointIO* Pair : Pairs)
	{
		Pair->OutputTo(Context, bEmplace, MinPointCount, MaxPointCount);
	}
}

void UPCGExPointIOGroup::ForEach(const TFunction<void(UPCGExPointIO*, const int32)>& BodyLoop)
{
	for (int i = 0; i < Pairs.Num(); i++)
	{
		UPCGExPointIO* PIOPair = Pairs[i];
		BodyLoop(PIOPair, i);
	}
}

template <typename InitializeFunc, typename ProcessElementFunc>
bool UPCGExPointIOGroup::OutputsParallelProcessing(
	FPCGContext* Context,
	InitializeFunc&& Initialize,
	ProcessElementFunc&& LoopBody,
	int32 ChunkSize,
	bool bForceSync)
{
	const int32 NumPairs = Pairs.Num();

	if (!bProcessing)
	{
		bProcessing = true;
		PairProcessingStatuses.Empty(NumPairs);
		for (int i = 0; i < NumPairs; i++) { PairProcessingStatuses.Add(false); }
	}

	int32 NumPairsDone = 0;

	for (int i = 0; i < NumPairs; i++)
	{
		bool bState = PairProcessingStatuses[i];
		if (!bState)
		{
			bState = Pairs[i]->OutputParallelProcessing(Context, Initialize, LoopBody, ChunkSize, bForceSync);
			if (bState) { PairProcessingStatuses[i] = bState; }
		}
		if (bState) { NumPairsDone++; }
	}

	if (NumPairs == NumPairsDone)
	{
		bProcessing = false;
		return true;
	}
	else
	{
		return false;
	}
}

template <typename InitializeFunc, typename ProcessElementFunc>
bool UPCGExPointIOGroup::InputsParallelProcessing(
	FPCGContext* Context,
	InitializeFunc&& Initialize,
	ProcessElementFunc&& LoopBody,
	int32 ChunkSize,
	bool bForceSync)
{
	const int32 NumPairs = Pairs.Num();

	if (!bProcessing)
	{
		bProcessing = true;
		PairProcessingStatuses.Empty(NumPairs);
		for (int i = 0; i < NumPairs; i++) { PairProcessingStatuses.Add(false); }
	}

	int32 NumPairsDone = 0;

	for (int i = 0; i < NumPairs; i++)
	{
		bool bState = PairProcessingStatuses[i];
		if (!bState)
		{
			bState = Pairs[i]->InputParallelProcessing(Context, Initialize, LoopBody, ChunkSize, bForceSync);
			if (bState) { PairProcessingStatuses[i] = bState; }
		}
		if (bState) { NumPairsDone++; }
	}

	if (NumPairs == NumPairsDone)
	{
		bProcessing = false;
		return true;
	}
	else
	{
		return false;
	}
}
