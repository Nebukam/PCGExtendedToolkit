// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "..\..\Public\Data\PCGExPointIO.h"

#include "PCGExMT.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

void FPCGExPointIO::InitializeOutput(const PCGExPointIO::EInit InitOut)
{
	switch (InitOut)
	{
	case PCGExPointIO::EInit::NoOutput:
		break;
	case PCGExPointIO::EInit::NewOutput:
		Out = NewObject<UPCGPointData>();
		if (In) { Out->InitializeFromData(In); }
		break;
	case PCGExPointIO::EInit::DuplicateInput:
		check(In)
		Out = Cast<UPCGPointData>(In->DuplicateData(true));
		break;
	case PCGExPointIO::EInit::Forward:
		check(In)
		Out = const_cast<UPCGPointData*>(In);
		break;
	default: ;
	}

	if (In) { NumInPoints = In->GetPoints().Num(); }
	else { NumInPoints = 0; }
}

const UPCGPointData* FPCGExPointIO::GetIn() const { return In; }
int32 FPCGExPointIO::GetNum() const { return NumInPoints; }

FPCGAttributeAccessorKeysPoints* FPCGExPointIO::GetInKeys()
{
	if (!InKeys && In) { InKeys = new FPCGAttributeAccessorKeysPoints(In->GetPoints()); }
	return InKeys;
}

UPCGPointData* FPCGExPointIO::GetOut() const { return Out; }

FPCGAttributeAccessorKeysPoints* FPCGExPointIO::GetOutKeys()
{
	if (!OutKeys && Out)
	{
		const TArrayView<FPCGPoint> View(Out->GetMutablePoints());
		OutKeys = new FPCGAttributeAccessorKeysPoints(View);
	}
	return OutKeys;
}


void FPCGExPointIO::InitPoint(FPCGPoint& Point, PCGMetadataEntryKey FromKey) const
{
	Out->Metadata->InitializeOnSet(Point.MetadataEntry, FromKey, In->Metadata);
}

void FPCGExPointIO::InitPoint(FPCGPoint& Point, const FPCGPoint& FromPoint) const
{
	Out->Metadata->InitializeOnSet(Point.MetadataEntry, FromPoint.MetadataEntry, In->Metadata);
}

void FPCGExPointIO::InitPoint(FPCGPoint& Point) const
{
	Out->Metadata->InitializeOnSet(Point.MetadataEntry);
}

FPCGPoint& FPCGExPointIO::CopyPoint(const FPCGPoint& FromPoint, int32& OutIndex) const
{
	FWriteScopeLock WriteLock(PointsLock);
	TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
	FPCGPoint& Pt = MutablePoints.Add_GetRef(FromPoint);
	OutIndex = MutablePoints.Num() - 1;
	InitPoint(Pt, FromPoint);
	return Pt;
}

FPCGPoint& FPCGExPointIO::NewPoint(int32& OutIndex) const
{
	FWriteScopeLock WriteLock(PointsLock);
	TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
	FPCGPoint& Pt = MutablePoints.Emplace_GetRef();
	OutIndex = MutablePoints.Num() - 1;
	InitPoint(Pt);
	return Pt;
}

void FPCGExPointIO::AddPoint(FPCGPoint& Point, int32& OutIndex, bool bInit = true) const
{
	FWriteScopeLock WriteLock(PointsLock);
	TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
	MutablePoints.Add(Point);
	OutIndex = MutablePoints.Num() - 1;
	if (bInit) { Out->Metadata->InitializeOnSet(Point.MetadataEntry); }
}

void FPCGExPointIO::AddPoint(FPCGPoint& Point, int32& OutIndex, const FPCGPoint& FromPoint) const
{
	FWriteScopeLock WriteLock(PointsLock);
	TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
	MutablePoints.Add(Point);
	OutIndex = MutablePoints.Num() - 1;
	InitPoint(Point, FromPoint);
}

UPCGPointData* FPCGExPointIO::NewEmptyOutput() const
{
	return PCGExPointIO::NewEmptyPointData(In);
}

UPCGPointData* FPCGExPointIO::NewEmptyOutput(FPCGContext* Context, FName PinLabel) const
{
	UPCGPointData* OutData = PCGExPointIO::NewEmptyPointData(Context, PinLabel.IsNone() ? DefaultOutputLabel : PinLabel, In);
	return OutData;
}

void FPCGExPointIO::Cleanup() const
{
	delete InKeys; 
	delete OutKeys;
}

FPCGExPointIO::~FPCGExPointIO()
{
	Cleanup();
	In = nullptr;
	Out = nullptr;
}

void FPCGExPointIO::BuildMetadataEntries()
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

bool FPCGExPointIO::OutputTo(FPCGContext* Context, const bool bEmplace)
{
	if (!Out || Out->GetPoints().Num() == 0) { return false; }

	if (!bEmplace)
	{
		if (!In)		{			return false;		}

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

bool FPCGExPointIO::OutputTo(FPCGContext* Context, bool bEmplace, const int64 MinPointCount, const int64 MaxPointCount)
{
	if (!Out) { return false; }

	const int64 OutNumPoints = Out->GetPoints().Num();
	if (MinPointCount >= 0 && OutNumPoints < MinPointCount) { return false; }
	if (MaxPointCount >= 0 && OutNumPoints > MaxPointCount) { return false; }

	return OutputTo(Context, bEmplace);
}

FPCGExPointIOGroup::FPCGExPointIOGroup()
{
}

FPCGExPointIOGroup::FPCGExPointIOGroup(FPCGContext* Context, FName InputLabel, PCGExPointIO::EInit InitOut)
	: FPCGExPointIOGroup()
{
	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(InputLabel);
	Initialize(Context, Sources, InitOut);
}

FPCGExPointIOGroup::FPCGExPointIOGroup(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, PCGExPointIO::EInit InitOut)
	: FPCGExPointIOGroup()
{
	Initialize(Context, Sources, InitOut);
}

FPCGExPointIOGroup::~FPCGExPointIOGroup()
{
	Flush();
}

void FPCGExPointIOGroup::Initialize(
	FPCGContext* Context, TArray<FPCGTaggedData>& Sources,
	const PCGExPointIO::EInit InitOut)
{
	Pairs.Empty(Sources.Num());
	for (FPCGTaggedData& Source : Sources)
	{
		const UPCGPointData* MutablePointData = PCGExData::GetMutablePointData(Context, Source);
		if (!MutablePointData || MutablePointData->GetPoints().Num() == 0) { continue; }
		Emplace_GetRef(Source, MutablePointData, InitOut);
	}
}

void FPCGExPointIOGroup::Initialize(
	FPCGContext* Context, TArray<FPCGTaggedData>& Sources,
	const PCGExPointIO::EInit InitOut,
	const TFunction<bool(UPCGPointData*)>& ValidateFunc,
	const TFunction<void(FPCGExPointIO&)>& PostInitFunc)
{
	Pairs.Empty(Sources.Num());
	for (FPCGTaggedData& Source : Sources)
	{
		UPCGPointData* MutablePointData = PCGExData::GetMutablePointData(Context, Source);
		if (!MutablePointData || MutablePointData->GetPoints().Num() == 0) { continue; }
		if (!ValidateFunc(MutablePointData)) { continue; }
		FPCGExPointIO& PointIO = Emplace_GetRef(Source, MutablePointData, InitOut);
		PostInitFunc(PointIO);
	}
}

FPCGExPointIO& FPCGExPointIOGroup::Emplace_GetRef(
	const FPCGExPointIO& PointIO,
	const PCGExPointIO::EInit InitOut)
{
	return Emplace_GetRef(PointIO.Source, PointIO.GetIn(), InitOut);
}

FPCGExPointIO& FPCGExPointIOGroup::Emplace_GetRef(
	const FPCGTaggedData& Source,
	const UPCGPointData* In,
	const PCGExPointIO::EInit InitOut)
{
	FWriteScopeLock WriteLock(PairsLock);
	return Pairs.Emplace_GetRef(Source, In, DefaultOutputLabel, InitOut);
}

FPCGExPointIO& FPCGExPointIOGroup::Emplace_GetRef(
	const UPCGPointData* In,
	const PCGExPointIO::EInit InitOut)
{
	const FPCGTaggedData Source;
	FWriteScopeLock WriteLock(PairsLock);
	return Pairs.Emplace_GetRef(Source, In, DefaultOutputLabel, InitOut);
}

FPCGExPointIO& FPCGExPointIOGroup::Emplace_GetRef(const PCGExPointIO::EInit InitOut)
{
	FWriteScopeLock WriteLock(PairsLock);
	return Pairs.Emplace_GetRef(DefaultOutputLabel, InitOut);
}

/**
 * Write valid outputs to Context' tagged data
 * @param Context
 * @param bEmplace Emplace will create a new entry no matter if a Source is set, otherwise will match the In.Source. 
 */
void FPCGExPointIOGroup::OutputTo(FPCGContext* Context, const bool bEmplace)
{
	for (FPCGExPointIO& Pair : Pairs)
	{
		Pair.OutputTo(Context, bEmplace);
	}
}

/**
 * Write valid outputs to Context' tagged data
 * @param Context
 * @param bEmplace Emplace will create a new entry no matter if a Source is set, otherwise will match the In.Source.
 * @param MinPointCount
 * @param MaxPointCount 
 */
void FPCGExPointIOGroup::OutputTo(FPCGContext* Context, bool bEmplace, const int64 MinPointCount, const int64 MaxPointCount)
{
	for (FPCGExPointIO& Pair : Pairs)
	{
		Pair.OutputTo(Context, bEmplace, MinPointCount, MaxPointCount);
	}
}

void FPCGExPointIOGroup::ForEach(const TFunction<void(FPCGExPointIO&, const int32)>& BodyLoop)
{
	for (int i = 0; i < Pairs.Num(); i++)
	{
		BodyLoop(Pairs[i], i);
	}
}

void FPCGExPointIOGroup::Flush()
{
	Pairs.Empty();
}
