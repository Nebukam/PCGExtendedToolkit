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
		Tags = InTags ? new FTags(*InTags) : Tags ? Tags : new FTags();
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
				PCGEX_NEW_FROM(UObject, GenericInstance, In)

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
				FGCScopeGuard GCGuarded;
				Out = NewObject<UPCGPointData>();
				Out->AddToRoot();
			}

			return;
		}

		if (InitOut == EInit::DuplicateInput)
		{
			check(In)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
			Out = Cast<UPCGPointData>(In->DuplicateData(true));
#else
			Out = Cast<UPCGPointData>(In->DuplicateData(Context, true));
#endif
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

	bool FPointIO::OutputToContext()
	{
		if (bEnabled && Out && Out->GetPoints().Num() > 0)
		{
			if (In && Out == In) { Context->FutureOutput(DefaultOutputLabel, Out, Tags->ToSet()); }
			else { Context->FutureRootedOutput(DefaultOutputLabel, Out, Tags->ToSet()); }
			bWritten = true;
			return true;
		}

		return false;
	}

	bool FPointIO::OutputToContext(const int32 MinPointCount, const int32 MaxPointCount)
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
				return OutputToContext();
			}
		}
		return false;
	}

#pragma endregion

#pragma region FPointIOCollection

	FPointIOCollection::FPointIOCollection(FPCGExContext* InContext): Context(InContext)
	{
	}

	FPointIOCollection::FPointIOCollection(FPCGExContext* InContext, const FName InputLabel, const EInit InitOut)
		: FPointIOCollection(InContext)
	{
		TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(InputLabel);
		Initialize(Sources, InitOut);
	}

	FPointIOCollection::FPointIOCollection(FPCGExContext* InContext, TArray<FPCGTaggedData>& Sources, const EInit InitOut)
		: FPointIOCollection(InContext)
	{
		Initialize(Sources, InitOut);
	}

	FPointIOCollection::~FPointIOCollection() { Flush(); }

	void FPointIOCollection::Initialize(
		TArray<FPCGTaggedData>& Sources,
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
		FPointIO* NewIO = new FPointIO(Context, In);
		NewIO->SetInfos(Pairs.Add(NewIO), DefaultOutputLabel, Tags);
		NewIO->InitializeOutput(InitOut);
		return NewIO;
	}

	FPointIO* FPointIOCollection::Emplace_GetRef(const EInit InitOut)
	{
		FWriteScopeLock WriteLock(PairsLock);
		FPointIO* NewIO = new FPointIO(Context);
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

	FPointIO* FPointIOCollection::AddUnsafe(FPointIO* PointIO)
	{
		PointIO->SetInfos(Pairs.Add(PointIO), DefaultOutputLabel);
		return PointIO;
	}

	void FPointIOCollection::AddUnsafe(const TArray<FPointIO*>& IOs)
	{
		if (IOs.IsEmpty()) { return; }
		Pairs.Reserve(Pairs.Num() + IOs.Num());
		for (FPointIO* IO : IOs)
		{
			if (!IO) { continue; }
			AddUnsafe(IO);
		}
	}

	/**
	 * Write valid outputs to Context' tagged data
	 */
	void FPointIOCollection::OutputToContext()
	{
		Sort();
		Context->FutureReserve(Pairs.Num());
		for (FPointIO* Pair : Pairs) { Pair->OutputToContext(); }
	}

	/**
	 * Write valid outputs to Context' tagged data
	 * @param MinPointCount
	 * @param MaxPointCount
	 */
	void FPointIOCollection::OutputToContext(const int32 MinPointCount, const int32 MaxPointCount)
	{
		Sort();
		Context->FutureReserve(Pairs.Num());
		for (FPointIO* Pair : Pairs) { Pair->OutputToContext(MinPointCount, MaxPointCount); }
	}

	void FPointIOCollection::Sort()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPointIOCollection::Sort);
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

	void FPointIOCollection::PruneNullEntries(const bool bUpdateIndices)
	{
		const int32 MaxPairs = Pairs.Num();
		int32 WriteIndex = 0;
		if (!bUpdateIndices)
		{
			for (int32 i = 0; i < MaxPairs; ++i) { if (Pairs[i]) { Pairs[WriteIndex++] = Pairs[i]; } }
		}
		else
		{
			for (int32 i = 0; i < MaxPairs; ++i)
			{
				if (Pairs[i])
				{
					Pairs[i]->IOIndex = WriteIndex;
					Pairs[WriteIndex++] = Pairs[i];
				}
			}
		}
		Pairs.SetNum(WriteIndex);
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
