// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPointIO.h"

#include "PCGExMT.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

namespace PCGExData
{
	void FPointIO::InitializeOutput(const PCGExData::EInit InitOut)
	{
		switch (InitOut)
		{
		case PCGExData::EInit::NoOutput:
			break;
		case PCGExData::EInit::NewOutput:
			Out = NewObject<UPCGPointData>();
			if (In) { Out->InitializeFromData(In); }
			break;
		case PCGExData::EInit::DuplicateInput:
			check(In)
			Out = Cast<UPCGPointData>(In->DuplicateData(true));
			break;
		case PCGExData::EInit::Forward:
			check(In)
			Out = const_cast<UPCGPointData*>(In);
			break;
		default: ;
		}

		if (In) { NumInPoints = In->GetPoints().Num(); }
		else { NumInPoints = 0; }
	}

	const UPCGPointData* FPointIO::GetIn() const { return In; }
	int32 FPointIO::GetNum() const { return NumInPoints; }

	FPCGAttributeAccessorKeysPoints* FPointIO::GetInKeys()
	{
		if (!InKeys && In) { InKeys = new FPCGAttributeAccessorKeysPoints(In->GetPoints()); }
		return InKeys;
	}

	UPCGPointData* FPointIO::GetOut() const { return Out; }

	FPCGAttributeAccessorKeysPoints* FPointIO::GetOutKeys()
	{
		if (!OutKeys && Out)
		{
			const TArrayView<FPCGPoint> View(Out->GetMutablePoints());
			OutKeys = new FPCGAttributeAccessorKeysPoints(View);
		}
		return OutKeys;
	}


	void FPointIO::InitPoint(FPCGPoint& Point, PCGMetadataEntryKey FromKey) const
	{
		Out->Metadata->InitializeOnSet(Point.MetadataEntry, FromKey, In->Metadata);
	}

	void FPointIO::InitPoint(FPCGPoint& Point, const FPCGPoint& FromPoint) const
	{
		Out->Metadata->InitializeOnSet(Point.MetadataEntry, FromPoint.MetadataEntry, In->Metadata);
	}

	void FPointIO::InitPoint(FPCGPoint& Point) const
	{
		Out->Metadata->InitializeOnSet(Point.MetadataEntry);
	}

	FPCGPoint& FPointIO::CopyPoint(const FPCGPoint& FromPoint, int32& OutIndex) const
	{
		FWriteScopeLock WriteLock(PointsLock);
		TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
		FPCGPoint& Pt = MutablePoints.Add_GetRef(FromPoint);
		OutIndex = MutablePoints.Num() - 1;
		InitPoint(Pt, FromPoint);
		return Pt;
	}

	FPCGPoint& FPointIO::NewPoint(int32& OutIndex) const
	{
		FWriteScopeLock WriteLock(PointsLock);
		TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
		FPCGPoint& Pt = MutablePoints.Emplace_GetRef();
		OutIndex = MutablePoints.Num() - 1;
		InitPoint(Pt);
		return Pt;
	}

	void FPointIO::AddPoint(FPCGPoint& Point, int32& OutIndex, bool bInit = true) const
	{
		FWriteScopeLock WriteLock(PointsLock);
		TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
		MutablePoints.Add(Point);
		OutIndex = MutablePoints.Num() - 1;
		if (bInit) { Out->Metadata->InitializeOnSet(Point.MetadataEntry); }
	}

	void FPointIO::AddPoint(FPCGPoint& Point, int32& OutIndex, const FPCGPoint& FromPoint) const
	{
		FWriteScopeLock WriteLock(PointsLock);
		TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
		MutablePoints.Add(Point);
		OutIndex = MutablePoints.Num() - 1;
		InitPoint(Point, FromPoint);
	}

	UPCGPointData* FPointIO::NewEmptyOutput() const
	{
		return PCGExPointIO::NewEmptyPointData(In);
	}

	UPCGPointData* FPointIO::NewEmptyOutput(FPCGContext* Context, FName PinLabel) const
	{
		UPCGPointData* OutData = PCGExPointIO::NewEmptyPointData(Context, PinLabel.IsNone() ? DefaultOutputLabel : PinLabel, In);
		return OutData;
	}

	void FPointIO::Cleanup()
	{
		PCGEX_DELETE(InKeys)
		PCGEX_DELETE(OutKeys)
	}

	FPointIO::~FPointIO()
	{
		Cleanup();
		In = nullptr;
		Out = nullptr;
	}

	void FPointIO::BuildMetadataEntries()
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

	bool FPointIO::OutputTo(FPCGContext* Context, const bool bEmplace)
	{
		if (!Out || Out->GetPoints().Num() == 0) { return false; }

		if (!bEmplace)
		{
			if (!In)
			{
				Cleanup();
				return false;
			}

			FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Add_GetRef(Source);
			OutputRef.Data = Out;
			OutputRef.Pin = DefaultOutputLabel;
		}
		else
		{
			FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Emplace_GetRef();
			OutputRef.Data = Out;
			OutputRef.Pin = DefaultOutputLabel;
		}

		Cleanup();
		return true;
	}

	bool FPointIO::OutputTo(FPCGContext* Context, bool bEmplace, const int64 MinPointCount, const int64 MaxPointCount)
	{
		if (!Out) { return false; }

		const int64 OutNumPoints = Out->GetPoints().Num();
		if (MinPointCount >= 0 && OutNumPoints < MinPointCount) { return false; }
		if (MaxPointCount >= 0 && OutNumPoints > MaxPointCount) { return false; }

		return OutputTo(Context, bEmplace);
	}

	FPointIOGroup::FPointIOGroup()
	{
	}

	FPointIOGroup::FPointIOGroup(FPCGContext* Context, FName InputLabel, PCGExData::EInit InitOut)
		: FPointIOGroup()
	{
		TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(InputLabel);
		Initialize(Context, Sources, InitOut);
	}

	FPointIOGroup::FPointIOGroup(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, PCGExData::EInit InitOut)
		: FPointIOGroup()
	{
		Initialize(Context, Sources, InitOut);
	}

	FPointIOGroup::~FPointIOGroup()
	{
		Flush();
	}

	void FPointIOGroup::Initialize(
		FPCGContext* Context, TArray<FPCGTaggedData>& Sources,
		const PCGExData::EInit InitOut)
	{
		Pairs.Empty(Sources.Num());
		for (FPCGTaggedData& Source : Sources)
		{
			const UPCGPointData* MutablePointData = PCGExData::GetMutablePointData(Context, Source);
			if (!MutablePointData || MutablePointData->GetPoints().Num() == 0) { continue; }
			Emplace_GetRef(Source, MutablePointData, InitOut);
		}
	}

	void FPointIOGroup::Initialize(
		FPCGContext* Context, TArray<FPCGTaggedData>& Sources,
		const PCGExData::EInit InitOut,
		const TFunction<bool(UPCGPointData*)>& ValidateFunc,
		const TFunction<void(PCGExData::FPointIO&)>& PostInitFunc)
	{
		Pairs.Empty(Sources.Num());
		for (FPCGTaggedData& Source : Sources)
		{
			UPCGPointData* MutablePointData = PCGExData::GetMutablePointData(Context, Source);
			if (!MutablePointData || MutablePointData->GetPoints().Num() == 0) { continue; }
			if (!ValidateFunc(MutablePointData)) { continue; }
			PCGExData::FPointIO& PointIO = Emplace_GetRef(Source, MutablePointData, InitOut);
			PostInitFunc(PointIO);
		}
	}

	PCGExData::FPointIO& FPointIOGroup::Emplace_GetRef(
		const PCGExData::FPointIO& PointIO,
		const PCGExData::EInit InitOut)
	{
		return Emplace_GetRef(PointIO.Source, PointIO.GetIn(), InitOut);
	}

	PCGExData::FPointIO& FPointIOGroup::Emplace_GetRef(
		const FPCGTaggedData& Source,
		const UPCGPointData* In,
		const PCGExData::EInit InitOut)
	{
		FWriteScopeLock WriteLock(PairsLock);
		return Pairs.Emplace_GetRef(Source, In, DefaultOutputLabel, InitOut);
	}

	PCGExData::FPointIO& FPointIOGroup::Emplace_GetRef(
		const UPCGPointData* In,
		const PCGExData::EInit InitOut)
	{
		const FPCGTaggedData Source;
		FWriteScopeLock WriteLock(PairsLock);
		return Pairs.Emplace_GetRef(Source, In, DefaultOutputLabel, InitOut);
	}

	PCGExData::FPointIO& FPointIOGroup::Emplace_GetRef(const PCGExData::EInit InitOut)
	{
		FWriteScopeLock WriteLock(PairsLock);
		return Pairs.Emplace_GetRef(DefaultOutputLabel, InitOut);
	}

	/**
	 * Write valid outputs to Context' tagged data
	 * @param Context
	 * @param bEmplace Emplace will create a new entry no matter if a Source is set, otherwise will match the In.Source. 
	 */
	void FPointIOGroup::OutputTo(FPCGContext* Context, const bool bEmplace)
	{
		for (PCGExData::FPointIO& Pair : Pairs)
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
	void FPointIOGroup::OutputTo(FPCGContext* Context, bool bEmplace, const int64 MinPointCount, const int64 MaxPointCount)
	{
		for (PCGExData::FPointIO& Pair : Pairs)
		{
			Pair.OutputTo(Context, bEmplace, MinPointCount, MaxPointCount);
		}
	}

	void FPointIOGroup::ForEach(const TFunction<void(PCGExData::FPointIO&, const int32)>& BodyLoop)
	{
		for (int i = 0; i < Pairs.Num(); i++)
		{
			BodyLoop(Pairs[i], i);
		}
	}

	void FPointIOGroup::Flush()
	{
		Pairs.Empty();
	}
}
