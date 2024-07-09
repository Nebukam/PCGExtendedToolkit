// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPointIO.h"

#include "PCGExContext.h"
#include "PCGExMT.h"
#include "Data/PCGExPointData.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

namespace PCGExData
{
#pragma region FPointIO

	void FPointIO::SetInfos(
		const int32 InIndex,
		const FName InDefaultOutputLabel,
		const TSet<FString>* InTags)
	{
		IOIndex = InIndex;
		DefaultOutputLabel = InDefaultOutputLabel;
		NumInPoints = In ? In->GetPoints().Num() : 0;
		Tags = InTags ? new FTags(*InTags) : new FTags();
	}

	void FPointIO::InitializeOutput(const EInit InitOut)
	{
		if (Out != In) { PCGEX_DELETE_UOBJECT(Out) }
		PCGEX_DELETE(OutKeys)

		if (InitOut == EInit::NoOutput)
		{
			Out = nullptr;
			return;
		}

		if (InitOut == EInit::Forward)
		{
			check(In);
			Out = const_cast<UPCGPointData*>(In);
			return;
		}

		if (InitOut == EInit::NewOutput)
		{
			if (In)
			{
				UObject* GenericInstance = NewObject<UObject>(In->GetOuter(), In->GetClass());
				Out = Cast<UPCGPointData>(GenericInstance);

				// Input type was not a PointData child, should not happen.
				check(Out)

				Out->InitializeFromData(In);

				const UPCGExPointData* TypedInPointData = Cast<UPCGExPointData>(In);
				UPCGExPointData* TypedOutPointData = Cast<UPCGExPointData>(Out);

				if (TypedInPointData && TypedOutPointData)
				{
					TypedOutPointData->InitializeFromPCGExData(TypedInPointData, EInit::NewOutput);
				}
			}
			else
			{
				Out = NewObject<UPCGPointData>();
			}

			return;
		}

		if (InitOut == EInit::DuplicateInput)
		{
			check(In)
			Out = Cast<UPCGPointData>(In->DuplicateData(true));
		}
	}

	FPCGAttributeAccessorKeysPoints* FPointIO::CreateInKeys()
	{
		if (InKeys) { return InKeys; }
		if (RootIO) { InKeys = RootIO->CreateInKeys(); }
		else { InKeys = new FPCGAttributeAccessorKeysPoints(In->GetPoints()); }
		return InKeys;
	}

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

	FPCGAttributeAccessorKeysPoints* FPointIO::CreateKeys(const ESource InSource)
	{
		return InSource == ESource::In ? CreateInKeys() : CreateOutKeys();
	}

	void FPointIO::InitializeNum(const int32 NumPoints, const bool bForceInit) const
	{
		TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
		MutablePoints.SetNum(NumPoints);
		if (bForceInit)
		{
			for (FPCGPoint& Pt : MutablePoints) { Out->Metadata->InitializeOnSet(Pt.MetadataEntry); }
		}
		else
		{
			for (FPCGPoint& Pt : MutablePoints)
			{
				if (Pt.MetadataEntry != PCGInvalidEntryKey) { continue; }
				Out->Metadata->InitializeOnSet(Pt.MetadataEntry);
			}
		}
	}

	void FPointIO::CleanupKeys()
	{
		if (!RootIO) { PCGEX_DELETE(InKeys) }
		else { InKeys = nullptr; }

		PCGEX_DELETE(OutKeys)
	}

	FPointIO::~FPointIO()
	{
		PCGEX_LOG_DTR(FPointIO)

		CleanupKeys();

		PCGEX_DELETE(Tags)

		if (!bWritten)
		{
			// Delete unused outputs
			if (Out && Out != In) { PCGEX_DELETE_UOBJECT(Out) }
		}

		RootIO = nullptr;
		In = nullptr;
		Out = nullptr;
	}

	bool FPointIO::OutputTo(FPCGContext* Context)
	{
		FPCGExContext* PCGExContext = static_cast<FPCGExContext*>(Context);
		check(PCGExContext);

		if (bEnabled && Out && Out->GetPoints().Num() > 0)
		{
			PCGExContext->FutureOutput(DefaultOutputLabel, Out, Tags->ToSet());
			bWritten = true;
			return true;
		}

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
				// Nah
			}
			else
			{
				return OutputTo(Context);
			}
		}
		return false;
	}

#pragma endregion

#pragma region FPointIOCollection

	FPointIOCollection::FPointIOCollection()
	{
	}

	FPointIOCollection::FPointIOCollection(const FPCGContext* Context, const FName InputLabel, const EInit InitOut)
		: FPointIOCollection()
	{
		TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(InputLabel);
		Initialize(Context, Sources, InitOut);
	}

	FPointIOCollection::FPointIOCollection(const FPCGContext* Context, TArray<FPCGTaggedData>& Sources, const EInit InitOut)
		: FPointIOCollection()
	{
		Initialize(Context, Sources, InitOut);
	}

	FPointIOCollection::~FPointIOCollection() { Flush(); }

	void FPointIOCollection::Initialize(
		const FPCGContext* Context, TArray<FPCGTaggedData>& Sources,
		const EInit InitOut)
	{
		Pairs.Empty(Sources.Num());
		TSet<uint64> UniqueData;
		UniqueData.Reserve(Sources.Num());

		for (FPCGTaggedData& Source : Sources)
		{
			bool bIsAlreadyInSet;
			UniqueData.Add(Source.Data->UID, &bIsAlreadyInSet);
			if (bIsAlreadyInSet) { continue; } // Dedupe

			const UPCGPointData* MutablePointData = PCGExPointIO::GetMutablePointData(Context, Source);
			if (!MutablePointData || MutablePointData->GetPoints().Num() == 0) { continue; }
			Emplace_GetRef(MutablePointData, InitOut, &Source.Tags);
		}
		UniqueData.Empty();
	}

	FPointIO* FPointIOCollection::Emplace_GetRef(
		const UPCGPointData* In,
		const EInit InitOut,
		const TSet<FString>* Tags)
	{
		FWriteScopeLock WriteLock(PairsLock);
		FPointIO* NewIO = new FPointIO(In);
		NewIO->SetInfos(Pairs.Add(NewIO), DefaultOutputLabel, Tags);
		NewIO->InitializeOutput(InitOut);
		return NewIO;
	}

	FPointIO* FPointIOCollection::Emplace_GetRef(const EInit InitOut)
	{
		FWriteScopeLock WriteLock(PairsLock);
		FPointIO* NewIO = new FPointIO(nullptr);
		NewIO->SetInfos(Pairs.Add(NewIO), DefaultOutputLabel);
		NewIO->InitializeOutput(InitOut);
		return NewIO;
	}

	FPointIO* FPointIOCollection::Emplace_GetRef(const FPointIO* PointIO, const EInit InitOut)
	{
		FPointIO* Branch = Emplace_GetRef(PointIO->GetIn(), InitOut);
		Branch->Tags->Reset(*PointIO->Tags);
		Branch->RootIO = const_cast<FPointIO*>(PointIO);
		return Branch;
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

	void FPointIOCollection::Sort()
	{
		Pairs.Sort([](const FPointIO& A, const FPointIO& B) { return A.IOIndex < B.IOIndex; });
	}

	FBox FPointIOCollection::GetInBounds() const
	{
		FBox Bounds = FBox(ForceInit);
		for (const FPointIO* IO : Pairs) { Bounds += IO->GetIn()->GetBounds(); }
		return Bounds;
	}

	FBox FPointIOCollection::GetOutBounds() const
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
