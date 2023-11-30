// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPointIO.h"

#include "PCGExMT.h"
#include "Helpers/PCGAsync.h"

UPCGExPointIO::UPCGExPointIO(): In(nullptr), Out(nullptr), NumInPoints(-1)
{
	// Initialize other members as needed
}

void UPCGExPointIO::InitPoint(FPCGPoint& Point, PCGMetadataEntryKey FromKey) const
{
	Out->Metadata->InitializeOnSet(Point.MetadataEntry, FromKey, In->Metadata);
}

void UPCGExPointIO::InitPoint(FPCGPoint& Point, const FPCGPoint& FromPoint) const
{
	Out->Metadata->InitializeOnSet(Point.MetadataEntry, FromPoint.MetadataEntry, In->Metadata);
}

FPCGPoint& UPCGExPointIO::NewPoint(const FPCGPoint& FromPoint) const
{
	FWriteScopeLock WriteLock(PointsLock);
	FPCGPoint& Pt = Out->GetMutablePoints().Add_GetRef(FromPoint);
	InitPoint(Pt, FromPoint);
	return Pt;
}

void UPCGExPointIO::AddPoint(FPCGPoint& Point, bool bInit = true) const
{
	FWriteScopeLock WriteLock(PointsLock);
	Out->GetMutablePoints().Add(Point);
	if (bInit) { Out->Metadata->InitializeOnSet(Point.MetadataEntry); }
}

void UPCGExPointIO::AddPoint(FPCGPoint& Point, const FPCGPoint& FromPoint) const
{
	FWriteScopeLock WriteLock(PointsLock);
	Out->GetMutablePoints().Add(Point);
	InitPoint(Point, FromPoint);
}

UPCGPointData* UPCGExPointIO::NewEmptyOutput() const
{
	return PCGExIO::NewEmptyOutput(In);
}

UPCGPointData* UPCGExPointIO::NewEmptyOutput(FPCGContext* Context, FName PinLabel) const
{
	UPCGPointData* OutData = PCGExIO::NewEmptyOutput(Context, PinLabel.IsNone() ? DefaultOutputLabel : PinLabel, In);
	return OutData;
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
	if (In) { NumInPoints = In->GetPoints().Num(); }
}

void UPCGExPointIO::BuildIndices()
{
	if (bMetadataEntryDirty) { BuildMetadataEntries(); }
	if (!bIndicesDirty) { return; }
	FWriteScopeLock WriteLock(MapLock);
	const TArray<FPCGPoint>& Points = Out->GetPoints();
	IndicesMap.Empty(NumInPoints);
	for (int i = 0; i < NumInPoints; i++) { IndicesMap.Add(Points[i].MetadataEntry, i); }
	bIndicesDirty = false;
}

void UPCGExPointIO::BuildMetadataEntries()
{
	if (!bMetadataEntryDirty) { return; }
	TArray<FPCGPoint>& Points = Out->GetMutablePoints();
	for (int i = 0; i < NumInPoints; i++)
	{
		FPCGPoint& Point = Points[i];
		Out->Metadata->InitializeOnSet(Point.MetadataEntry, GetInPoint(i).MetadataEntry, In->Metadata);
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
	IndicesMap.Empty(NumInPoints);
	for (int i = 0; i < NumInPoints; i++)
	{
		FPCGPoint& Point = Points[i];
		Out->Metadata->InitializeOnSet(Point.MetadataEntry, GetInPoint(i).MetadataEntry, In->Metadata);
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
	: UPCGExPointIOGroup()
{
	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(InputLabel);
	Initialize(Context, Sources, InitOut);
}

UPCGExPointIOGroup::UPCGExPointIOGroup(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, PCGExIO::EInitMode InitOut)
	: UPCGExPointIOGroup()
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
		UPCGPointData* MutablePointData = PCGExIO::GetMutablePointData(Context, Source);
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
		UPCGPointData* MutablePointData = PCGExIO::GetMutablePointData(Context, Source);
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
	UPCGExPointIO* Pair = CreateNewPointIO(Source, In, DefaultOutputLabel, InitOut);

	{
		FWriteScopeLock WriteLock(PairsLock);
		Pairs.Add(Pair);
	}

	return Pair;
}

UPCGExPointIO* UPCGExPointIOGroup::Emplace_GetRef(
	UPCGPointData* In,
	const PCGExIO::EInitMode InitOut)
{
	const FPCGTaggedData TaggedData;
	UPCGExPointIO* Pair = CreateNewPointIO(TaggedData, In, DefaultOutputLabel, InitOut);

	{
		FWriteScopeLock WriteLock(PairsLock);
		Pairs.Add(Pair);
	}

	return Pair;
}

UPCGExPointIO* UPCGExPointIOGroup::Emplace_GetRef(const PCGExIO::EInitMode InitOut)
{
	UPCGExPointIO* Pair = CreateNewPointIO(DefaultOutputLabel, InitOut);

	{
		FWriteScopeLock WriteLock(PairsLock);
		Pairs.Add(Pair);
	}

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
