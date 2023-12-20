// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPointIO.h"

#include "PCGExMT.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

namespace PCGExData
{
	void FPointIO::InitializeOutput(const EInit InitOut)
	{
		switch (InitOut)
		{
		case EInit::NoOutput:
			break;
		case EInit::NewOutput:
			Out = NewObject<UPCGPointData>();
			if (In) { Out->InitializeFromData(In); }
		//else { Out->CreateEmptyMetadata(); }
			break;
		case EInit::DuplicateInput:
			check(In)
			Out = Cast<UPCGPointData>(In->DuplicateData(true));
			break;
		case EInit::Forward:
			check(In)
			Out = const_cast<UPCGPointData*>(In);
			break;
		default: ;
		}
	}

	const UPCGPointData* FPointIO::GetIn() const { return In; }
	int32 FPointIO::GetNum() const { return In ? In->GetPoints().Num() : Out ? Out->GetPoints().Num() : -1; }

	FPCGAttributeAccessorKeysPoints* FPointIO::CreateInKeys()
	{
		if (InKeys) { return InKeys; }
		if (RootIO) { InKeys = RootIO->CreateInKeys(); }
		else { InKeys = new FPCGAttributeAccessorKeysPoints(In->GetPoints()); }
		return InKeys;
	}

	FPCGAttributeAccessorKeysPoints* FPointIO::GetInKeys() const { return InKeys; }

	UPCGPointData* FPointIO::GetOut() const { return Out; }

	FPCGAttributeAccessorKeysPoints* FPointIO::CreateOutKeys()
	{
		if (!OutKeys)
		{
			const TArrayView<FPCGPoint> View(Out->GetMutablePoints());
			OutKeys = new FPCGAttributeAccessorKeysPoints(View);
		}
		return OutKeys;
	}

	FPCGAttributeAccessorKeysPoints* FPointIO::GetOutKeys() const { return OutKeys; }


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
		if (!RootIO) { PCGEX_DELETE(InKeys) }
		else { InKeys = nullptr; }

		PCGEX_DELETE(OutKeys)
	}

	FPointIO::~FPointIO()
	{
		Cleanup();
		RootIO = nullptr;
		In = nullptr;
		Out = nullptr;
	}

	FPointIO& FPointIO::Branch()
	{
		FPointIO& Branch = *(new FPointIO(Source, GetIn(), DefaultOutputLabel, EInit::NewOutput));
		Branch.RootIO = this;
		return Branch;
	}

	bool FPointIO::OutputTo(FPCGContext* Context, const bool bEmplace)
	{
		bool bSuccess = false;

		if (Out && Out->GetPoints().Num() > 0)
		{
			if (!bEmplace)
			{
				if (In)
				{
					FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Add_GetRef(Source);
					OutputRef.Data = Out;
					OutputRef.Pin = DefaultOutputLabel;
					bSuccess = true;
				}
			}
			else
			{
				FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Emplace_GetRef();
				OutputRef.Data = Out;
				OutputRef.Pin = DefaultOutputLabel;
				bSuccess = true;
			}
		}

		if (!bSuccess && Out)
		{
			Out->ConditionalBeginDestroy();
			Out = nullptr;
		}

		Cleanup();
		return bSuccess;
	}

	bool FPointIO::OutputTo(FPCGContext* Context, bool bEmplace, const int32 MinPointCount, const int32 MaxPointCount)
	{
		if (Out)
		{
			const int64 OutNumPoints = Out->GetPoints().Num();

			if ((MinPointCount >= 0 && OutNumPoints < MinPointCount) ||
				(MaxPointCount >= 0 && OutNumPoints > MaxPointCount))
			{
				Cleanup();
				Out->ConditionalBeginDestroy();
				Out = nullptr;
			}
			else
			{
				return OutputTo(Context, bEmplace);
			}
		}
		return false;
	}

	FPointIOGroup::FPointIOGroup()
	{
	}

	FPointIOGroup::FPointIOGroup(FPCGContext* Context, FName InputLabel, EInit InitOut)
		: FPointIOGroup()
	{
		TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(InputLabel);
		Initialize(Context, Sources, InitOut);
	}

	FPointIOGroup::FPointIOGroup(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, EInit InitOut)
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
		const EInit InitOut)
	{
		Pairs.Empty(Sources.Num());
		for (FPCGTaggedData& Source : Sources)
		{
			const UPCGPointData* MutablePointData = PCGExPointIO::GetMutablePointData(Context, Source);
			if (!MutablePointData || MutablePointData->GetPoints().Num() == 0) { continue; }
			Emplace_GetRef(Source, MutablePointData, InitOut);
		}
	}

	FPointIO& FPointIOGroup::Emplace_GetRef(
		const FPointIO& PointIO,
		const EInit InitOut)
	{
		FPointIO& Branch = Emplace_GetRef(PointIO.Source, PointIO.GetIn(), InitOut);
		Branch.RootIO = const_cast<FPointIO*>(&PointIO);
		return Branch;
	}

	FPointIO& FPointIOGroup::Emplace_GetRef(
		const FPCGTaggedData& Source,
		const UPCGPointData* In,
		const EInit InitOut)
	{
		FWriteScopeLock WriteLock(PairsLock);
		return *Pairs.Add_GetRef(new FPointIO(Source, In, DefaultOutputLabel, InitOut));
	}

	FPointIO& FPointIOGroup::Emplace_GetRef(
		const UPCGPointData* In,
		const EInit InitOut)
	{
		const FPCGTaggedData Source;
		FWriteScopeLock WriteLock(PairsLock);
		return *Pairs.Add_GetRef(new FPointIO(Source, In, DefaultOutputLabel, InitOut));
	}

	FPointIO& FPointIOGroup::Emplace_GetRef(const EInit InitOut)
	{
		FWriteScopeLock WriteLock(PairsLock);
		return *Pairs.Add_GetRef(new FPointIO(DefaultOutputLabel, InitOut));
	}

	/**
	 * Write valid outputs to Context' tagged data
	 * @param Context
	 * @param bEmplace Emplace will create a new entry no matter if a Source is set, otherwise will match the In.Source. 
	 */
	void FPointIOGroup::OutputTo(FPCGContext* Context, const bool bEmplace)
	{
		for (FPointIO* Pair : Pairs) { Pair->OutputTo(Context, bEmplace); }
	}

	/**
	 * Write valid outputs to Context' tagged data
	 * @param Context
	 * @param bEmplace Emplace will create a new entry no matter if a Source is set, otherwise will match the In.Source.
	 * @param MinPointCount
	 * @param MaxPointCount 
	 */
	void FPointIOGroup::OutputTo(FPCGContext* Context, bool bEmplace, const int32 MinPointCount, const int32 MaxPointCount)
	{
		for (FPointIO* Pair : Pairs) { Pair->OutputTo(Context, bEmplace, MinPointCount, MaxPointCount); }
	}

	void FPointIOGroup::ForEach(const TFunction<void(FPointIO&, const int32)>& BodyLoop)
	{
		for (int i = 0; i < Pairs.Num(); i++) { BodyLoop(*Pairs[i], i); }
	}

	void FPointIOGroup::Flush()
	{
		PCGEX_DELETE_TARRAY(Pairs)
	}
}
