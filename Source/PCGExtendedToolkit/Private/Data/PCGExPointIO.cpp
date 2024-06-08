// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPointIO.h"

#include "PCGExMT.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

namespace PCGExData
{
#pragma region FPointIO

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
		//case EInit::Forward:
		case EInit::DuplicateInput:
			check(In)
			Out = Cast<UPCGPointData>(In->DuplicateData(true));
			break;

		case EInit::Forward: // Seems to be creating a lot of weird issues
			check(In)
			Out = const_cast<UPCGPointData*>(In);
			break;

		default: ;
		}
	}

	const UPCGPointData* FPointIO::GetData(const ESource InSource) const { return InSource == ESource::In ? In : Out; }
	const UPCGPointData* FPointIO::GetIn() const { return In; }
	UPCGPointData* FPointIO::GetOut() const { return Out; }
	const UPCGPointData* FPointIO::GetOutIn() const { return Out ? Out : In; }
	const UPCGPointData* FPointIO::GetInOut() const { return In ? In : Out; }

	int32 FPointIO::GetNum() const
	{
		return In ? In->GetPoints().Num() : Out ? Out->GetPoints().Num() : -1;
	}

	int32 FPointIO::GetOutNum() const
	{
		return Out && !Out->GetPoints().IsEmpty() ? Out->GetPoints().Num() : In ? In->GetPoints().Num() : -1;
	}

	FPCGAttributeAccessorKeysPoints* FPointIO::CreateInKeys()
	{
		if (InKeys) { return InKeys; }
		if (RootIO) { InKeys = RootIO->CreateInKeys(); }
		else { InKeys = new FPCGAttributeAccessorKeysPoints(In->GetPoints()); }
		return InKeys;
	}

	FPCGAttributeAccessorKeysPoints* FPointIO::GetInKeys() const { return InKeys; }

	void FPointIO::PrintInKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap)
	{
		CreateInKeys();
		const TArray<FPCGPoint>& PointList = In->GetPoints();
		InMap.Empty(PointList.Num());
		for (int i = 0; i < PointList.Num(); i++) { InMap.Add(PointList[i].MetadataEntry, i); }
	}

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

	void FPointIO::PrintOutKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap, const bool bInitializeOnSet = false)
	{
		if (bInitializeOnSet)
		{
			TArray<FPCGPoint>& PointList = Out->GetMutablePoints();
			InMap.Empty(PointList.Num());
			for (int i = 0; i < PointList.Num(); i++)
			{
				FPCGPoint& Point = PointList[i];
				Out->Metadata->InitializeOnSet(Point.MetadataEntry);
				InMap.Add(Point.MetadataEntry, i);
			}
			CreateOutKeys();
		}
		else
		{
			CreateOutKeys();
			const TArray<FPCGPoint>& PointList = Out->GetPoints();
			InMap.Empty(PointList.Num());
			for (int i = 0; i < PointList.Num(); i++) { InMap.Add(PointList[i].MetadataEntry, i); }
		}
	}

	FPCGAttributeAccessorKeysPoints* FPointIO::CreateKeys(ESource InSource)
	{
		return InSource == ESource::In ? CreateInKeys() : CreateOutKeys();
	}

	FPCGAttributeAccessorKeysPoints* FPointIO::GetKeys(ESource InSource) const
	{
		return InSource == ESource::In ? GetInKeys() : GetOutKeys();
	}

	void FPointIO::InitPoint(FPCGPoint& Point, const PCGMetadataEntryKey FromKey) const
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
		OutIndex = MutablePoints.Num();
		FPCGPoint& Pt = MutablePoints.Add_GetRef(FromPoint);
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

	void FPointIO::AddPoint(FPCGPoint& Point, int32& OutIndex, const bool bInit = true) const
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

	void FPointIO::SetNumInitialized(const int32 NumPoints, const bool bForceInit) const
	{
		TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
		MutablePoints.SetNum(NumPoints);
		if (bForceInit)
		{
			for (int i = 0; i < NumPoints; i++)
			{
				Out->Metadata->InitializeOnSet(MutablePoints[i].MetadataEntry);
			}
		}
		else
		{
			for (int i = 0; i < NumPoints; i++)
			{
				if (MutablePoints[i].MetadataEntry != PCGInvalidEntryKey) { continue; }
				Out->Metadata->InitializeOnSet(MutablePoints[i].MetadataEntry);
			}
		}
	}

	UPCGPointData* FPointIO::NewEmptyOutput() const { return PCGExPointIO::NewEmptyPointData(In); }

	UPCGPointData* FPointIO::NewEmptyOutput(FPCGContext* Context, const FName PinLabel) const
	{
		UPCGPointData* OutData = PCGExPointIO::NewEmptyPointData(Context, PinLabel.IsNone() ? DefaultOutputLabel : PinLabel, In);
		return OutData;
	}

	void FPointIO::CleanupKeys()
	{
		if (!RootIO) { PCGEX_DELETE(InKeys) }
		else { InKeys = nullptr; }

		PCGEX_DELETE(OutKeys)
	}

	FPointIO::~FPointIO()
	{
		CleanupKeys();

		PCGEX_DELETE(Tags)

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

	bool FPointIO::OutputTo(FPCGContext* Context)
	{
		if (bEnabled && Out && Out->GetPoints().Num() > 0)
		{
			FPCGTaggedData* TaggedOutput = &Context->OutputData.TaggedData.Emplace_GetRef();
			TaggedOutput->Data = Out;
			TaggedOutput->Pin = DefaultOutputLabel;
			Tags->Dump(TaggedOutput->Tags);

			CleanupKeys();
			return true;
		}

		if (Out && Out != In)
		{
			PCGEX_DELETE_UOBJECT(Out)
			Out = nullptr;
		}

		CleanupKeys();
		return false;
	}

	bool FPointIO::OutputTo(FPCGContext* Context, const int32 MinPointCount, const int32 MaxPointCount)
	{
		if (Out)
		{
			const int64 OutNumPoints = Out->GetPoints().Num();

			if ((MinPointCount >= 0 && OutNumPoints < MinPointCount) ||
				(MaxPointCount >= 0 && OutNumPoints > MaxPointCount))
			{
				CleanupKeys();
				if (Out != In) { PCGEX_DELETE_UOBJECT(Out) }
				Out = nullptr;
			}
			else
			{
				return OutputTo(Context);
			}
		}
		return false;
	}

	void FPointIO::Flatten() const
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
		if (Out && In != Out) { Out->Metadata->Flatten(); }
#endif
	}

#pragma endregion

#pragma region FPointIOCollection

	FPointIOCollection::FPointIOCollection()
	{
	}

	FPointIOCollection::FPointIOCollection(FPCGContext* Context, const FName InputLabel, const EInit InitOut)
		: FPointIOCollection()
	{
		TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(InputLabel);
		Initialize(Context, Sources, InitOut);
	}

	FPointIOCollection::FPointIOCollection(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, const EInit InitOut)
		: FPointIOCollection()
	{
		Initialize(Context, Sources, InitOut);
	}

	FPointIOCollection::~FPointIOCollection() { Flush(); }

	void FPointIOCollection::Initialize(
		FPCGContext* Context, TArray<FPCGTaggedData>& Sources,
		const EInit InitOut)
	{
		Pairs.Empty(Sources.Num());
		TSet<uint64> UniqueData;
		UniqueData.Reserve(Sources.Num());
		for (FPCGTaggedData& Source : Sources)
		{
			if (UniqueData.Contains(Source.Data->UID)) { continue; } // Dedupe
			UniqueData.Add(Source.Data->UID);

			const UPCGPointData* MutablePointData = PCGExPointIO::GetMutablePointData(Context, Source);
			if (!MutablePointData || MutablePointData->GetPoints().Num() == 0) { continue; }
			Emplace_GetRef(Source, MutablePointData, InitOut);
		}
		UniqueData.Empty();
	}

	FPointIO& FPointIOCollection::Emplace_GetRef(
		const FPointIO& PointIO,
		const EInit InitOut)
	{
		FPointIO& Branch = Emplace_GetRef(PointIO.Source, PointIO.GetIn(), InitOut);
		Branch.Tags->Reset(*PointIO.Tags);
		Branch.RootIO = const_cast<FPointIO*>(&PointIO);
		return Branch;
	}

	FPointIO& FPointIOCollection::Emplace_GetRef(
		const FPCGTaggedData& Source,
		const UPCGPointData* In,
		const EInit InitOut)
	{
		FWriteScopeLock WriteLock(PairsLock);
		return *Pairs.Add_GetRef(new FPointIO(Source, In, DefaultOutputLabel, InitOut, Pairs.Num()));
	}

	FPointIO& FPointIOCollection::Emplace_GetRef(
		const UPCGPointData* In,
		const EInit InitOut)
	{
		const FPCGTaggedData Source;
		FWriteScopeLock WriteLock(PairsLock);
		return *Pairs.Add_GetRef(new FPointIO(Source, In, DefaultOutputLabel, InitOut, Pairs.Num()));
	}

	FPointIO& FPointIOCollection::Emplace_GetRef(const EInit InitOut)
	{
		FWriteScopeLock WriteLock(PairsLock);
		return *Pairs.Add_GetRef(new FPointIO(DefaultOutputLabel, InitOut, Pairs.Num()));
	}

	/**
	 * Write valid outputs to Context' tagged data
	 * @param Context
	 */
	void FPointIOCollection::OutputTo(FPCGContext* Context)
	{
		Sort();
		for (FPointIO* Pair : Pairs) { Pair->OutputTo(Context); }
	}

	/**
	 * Write valid outputs to Context' tagged data
	 * @param Context
	 * @param MinPointCount
	 * @param MaxPointCount
	 */
	void FPointIOCollection::OutputTo(FPCGContext* Context, const int32 MinPointCount, const int32 MaxPointCount)
	{
		Sort();
		for (FPointIO* Pair : Pairs) { Pair->OutputTo(Context, MinPointCount, MaxPointCount); }
	}

	void FPointIOCollection::ForEach(const TFunction<void(FPointIO&, const int32)>& BodyLoop)
	{
		for (int i = 0; i < Pairs.Num(); i++) { BodyLoop(*Pairs[i], i); }
	}

	void FPointIOCollection::Sort()
	{
		Pairs.Sort([](const FPointIO& A, const FPointIO& B) { return A.IOIndex < B.IOIndex; });
	}

	FBox FPointIOCollection::GetInBounds()
	{
		FBox Bounds = FBox(ForceInit);
		for (const FPointIO* IO : Pairs) { Bounds += IO->GetIn()->GetBounds(); }
		return Bounds;
	}

	FBox FPointIOCollection::GetOutBounds()
	{
		FBox Bounds = FBox(ForceInit);
		for (const FPointIO* IO : Pairs) { Bounds += IO->GetOut()->GetBounds(); }
		return Bounds;
	}

	void FPointIOCollection::Flush()
	{
		PCGEX_DELETE_TARRAY(Pairs)
	}

#pragma endregion

#pragma region FPointIOTaggedEntries

	void FPointIOTaggedEntries::Add(FPointIO* Value)
	{
		Entries.AddUnique(Value);
		Value->Tags->Set(TagId, TagValue);
	}

	bool FPointIOTaggedDictionary::CreateKey(const FPointIO& PointIOKey)
	{
		FString TagValue;
		if (!PointIOKey.Tags->GetValue(TagId, TagValue))
		{
			TagValue = FString::Printf(TEXT("%llu"), PointIOKey.GetInOut()->UID);
			PointIOKey.Tags->Set(TagId, TagValue);
		}

		bool bFoundDupe = false;
		for (const FPointIOTaggedEntries* Binding : Entries)
		{
			// TagValue shouldn't exist already
			if (Binding->TagValue == TagValue) { return false; }
		}

		FPointIOTaggedEntries* NewBinding = new FPointIOTaggedEntries(TagId, TagValue);
		TagMap.Add(TagValue, Entries.Add(NewBinding));
		return true;
	}

	bool FPointIOTaggedDictionary::TryAddEntry(FPointIO& PointIOEntry)
	{
		FString TagValue;
		if (!PointIOEntry.Tags->GetValue(TagId, TagValue)) { return false; }

		if (const int32* Index = TagMap.Find(TagValue))
		{
			FPointIOTaggedEntries* Key = Entries[*Index];
			Key->Add(&PointIOEntry);
			return true;
		}

		return false;
	}

	FPointIOTaggedEntries* FPointIOTaggedDictionary::GetEntries(const FString& Key)
	{
		if (const int32* Index = TagMap.Find(Key)) { return Entries[*Index]; }
		return nullptr;
	}

#pragma endregion
}
