// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPointIO.h"

#include "PCGExContext.h"
#include "PCGExMT.h"
#include "Data/PCGExPointData.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

namespace PCGExData
{
#pragma region FPointIO

	FPoint::FPoint(const uint64 Hash)
		: Index(PCGEx::H64A(Hash)), IO(PCGEx::H64A(Hash))
	{
	}

	FPoint::FPoint(const int32 InIndex, const int32 InIO)
		: Index(InIndex), IO(InIO)
	{
	}

	FPoint::FPoint(const TSharedPtr<FPointIO> InIO, const uint32 InIndex)
		: Index(InIndex), IO(InIO->IOIndex)
	{
	}

	FMutablePoint::FMutablePoint(UPCGBasePointData* InData, const int32 InIndex, const int32 InIO)
		: FPoint(InIndex, InIO), Data(InData)
	{
	}

	FMutablePoint::FMutablePoint(const TSharedPtr<FPointIO>& InFacade, const int32 InIndex)
		: FPoint(InFacade, InIndex), Data(InFacade->GetOut())
	{
	}

	FConstPoint::FConstPoint(const FMutablePoint& InPoint)
		: FConstPoint(InPoint.Data, InPoint.Index)
	{
	}

	FConstPoint::FConstPoint(const UPCGBasePointData* InData, const uint64 Hash)
		: FPoint(Hash), Data(InData)
	{
	}

	FConstPoint::FConstPoint(const UPCGBasePointData* InData, const int32 InIndex, const int32 InIO)
		: FPoint(InIndex, InIO), Data(InData)
	{
	}

	FConstPoint::FConstPoint(const TSharedPtr<FPointIO>& InFacade, const int32 InIndex)
		: FPoint(InFacade, InIndex), Data(InFacade->GetIn())
	{
	}

	FMutablePoint::FMutablePoint(UPCGBasePointData* InData, const uint64 Hash)
		: FPoint(Hash), Data(InData)
	{
	}

	void FPointIO::SetInfos(
		const int32 InIndex,
		const FName InOutputPin,
		const TSet<FString>* InTags)
	{
		IOIndex = InIndex;
		OutputPin = InOutputPin;
		NumInPoints = In ? In->GetNumPoints() : 0;

		if (InTags) { Tags = MakeShared<FTags>(*InTags); }
		else if (!Tags) { Tags = MakeShared<FTags>(); }
	}

	bool FPointIO::InitializeOutput(const EIOInit InitOut)
	{
		FPCGContext::FSharedContext<FPCGExContext> SharedContext(ContextHandle);

		if (!SharedContext.Get()) { return false; }
		if (IsValid(Out) && Out != In)
		{
			SharedContext.Get()->ManagedObjects->Destroy(Out);
			Out = nullptr;
		}

		OutKeys.Reset();

		bMutable = false;

		if (InitOut == EIOInit::None) { return true; }

		if (InitOut == EIOInit::Forward)
		{
			check(In);
			Out = const_cast<UPCGBasePointData*>(In);
			return true;
		}

		bMutable = true;

		if (InitOut == EIOInit::New)
		{
			if (In)
			{
				UObject* GenericInstance = SharedContext.Get()->ManagedObjects->New<UObject>(GetTransientPackage(), In->GetClass());
				Out = Cast<UPCGBasePointData>(GenericInstance);

				// Input type was not a PointData child, should not happen.
				check(Out)

				Out->InitializeFromData(In);
			}
			else
			{
				Out = SharedContext.Get()->ManagedObjects->New<PCGEX_NEW_POINT_DATA_TYPE>();
			}

			return true;
		}

		if (InitOut == EIOInit::Duplicate)
		{
			check(In)
			Out = SharedContext.Get()->ManagedObjects->DuplicateData<PCGEX_NEW_POINT_DATA_TYPE>(In);
		}

		return true;
	}

	const UPCGBasePointData* FPointIO::GetOutIn(EIOSide& OutSide) const
	{
		if (Out)
		{
			OutSide = EIOSide::Out;
			return Out;
		}

		if (In)
		{
			OutSide = EIOSide::In;
			return In;
		}

		return nullptr;
	}

	const UPCGBasePointData* FPointIO::GetInOut(EIOSide& OutSide) const
	{
		if (In)
		{
			OutSide = EIOSide::In;
			return In;
		}

		if (Out)
		{
			OutSide = EIOSide::Out;
			return Out;
		}

		return nullptr;
	}

	bool FPointIO::GetSource(const UPCGData* InData, EIOSide& OutSide) const
	{
		if (!InData) { return false; }

		if (InData == In)
		{
			OutSide = EIOSide::In;
			return true;
		}

		if (InData == Out)
		{
			OutSide = EIOSide::Out;
			return true;
		}

		return false;
	}

	TSharedPtr<FPCGAttributeAccessorKeysPointIndices> FPointIO::GetInKeys()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPointIO::GetInKeys);

		{
			FReadScopeLock ReadScopeLock(InKeysLock);
			if (InKeys) { return InKeys; }
		}

		{
			FWriteScopeLock WriteScopeLock(InKeysLock);
			if (InKeys) { return InKeys; }
			if (const TSharedPtr<FPointIO> PinnedRoot = RootIO.Pin()) { InKeys = PinnedRoot->GetInKeys(); }
			else { InKeys = MakeShared<FPCGAttributeAccessorKeysPointIndices>(In); }
		}

		return InKeys;
	}

	TSharedPtr<FPCGAttributeAccessorKeysPointIndices> FPointIO::GetOutKeys(const bool bEnsureValidKeys)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPointIO::GetOutKeys);

		check(Out)

		{
			FReadScopeLock ReadScopeLock(OutKeysLock);
			if (OutKeys) { return OutKeys; }
		}

		{
			FWriteScopeLock WriteScopeLock(OutKeysLock);
			if (OutKeys) { return OutKeys; }

			if (bEnsureValidKeys)
			{
				UPCGMetadata* OutMetadata = Out->Metadata;
				TPCGValueRange<int64> MetadataEntries = Out->GetMetadataEntryValueRange();
				for (int64& Key : MetadataEntries) { OutMetadata->InitializeOnSet(Key); }

				OutKeys = MakeShared<FPCGAttributeAccessorKeysPointIndices>(Out, false);
			}
			else
			{
				OutKeys = MakeShared<FPCGAttributeAccessorKeysPointIndices>(Out, true);
			}
		}

		return OutKeys;
	}

	void FPointIO::PrintOutKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap) const
	{
		const TPCGValueRange<int64> MetadataEntries = Out->GetMetadataEntryValueRange();
		InMap.Empty(MetadataEntries.Num());
		for (int i = 0; i < MetadataEntries.Num(); i++)
		{
			int64& Key = MetadataEntries[i];
			if (Key == PCGInvalidEntryKey) { Out->Metadata->InitializeOnSet(Key); }
			InMap.Add(Key, i);
		}
	}

	void FPointIO::PrintInKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap) const
	{
		const TConstPCGValueRange<int64> MetadataEntries = In->GetConstMetadataEntryValueRange();
		InMap.Empty(MetadataEntries.Num());
		for (int i = 0; i < MetadataEntries.Num(); i++) { InMap.Add(MetadataEntries[i], i); }
	}

	void FPointIO::PrintOutInKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap) const
	{
		if (Out) { PrintOutKeysMap(InMap); }
		else { PrintInKeysMap(InMap); }
	}

	void FPointIO::CopyProperties(const int32 ReadStartIndex, const int32 WriteStartIndex, const int32 Count, const EPCGPointNativeProperties Properties) const
	{
		In->CopyPropertiesTo(Out, ReadStartIndex, WriteStartIndex, Count, Properties);
	}

	void FPointIO::CopyProperties(const TArrayView<const int32>& ReadIndices, const TArrayView<const int32>& WriteIndices, const EPCGPointNativeProperties Properties) const
	{
		In->CopyPropertiesTo(Out, ReadIndices, WriteIndices, Properties);
	}

	void FPointIO::CopyProperties(const TArrayView<const int32>& ReadIndices, const EPCGPointNativeProperties Properties) const
	{
		check(Out->GetNumPoints() == ReadIndices.Num())

		TArray<int32> WriteIndices;
		PCGEx::ArrayOfIndices(WriteIndices, ReadIndices.Num());

		In->CopyPropertiesTo(Out, ReadIndices, WriteIndices, Properties);
	}

	void FPointIO::CopyPoints(const int32 ReadStartIndex, const int32 WriteStartIndex, const int32 Count) const
	{
		In->CopyPointsTo(Out, ReadStartIndex, WriteStartIndex, Count);
	}

	void FPointIO::CopyPoints(const TArrayView<const int32>& ReadIndices, const TArrayView<const int32>& WriteIndices) const
	{
		In->CopyPointsTo(Out, ReadIndices, WriteIndices);
	}

	void FPointIO::ClearCachedKeys()
	{
		InKeys.Reset();
		OutKeys.Reset();
	}

	FPointIO::~FPointIO() = default;

	bool FPointIO::StageOutput(FPCGExContext* TargetContext) const
	{
		// If this hits, it needs to be reported. It means a node is trying to output data that is meant to be transactional only
		check(!bTransactional)

		if (!IsEnabled() || !Out || (!bAllowEmptyOutput && Out->IsEmpty())) { return false; }

		TargetContext->StageOutput(OutputPin, Out, Tags->Flatten(), Out != In, bMutable);

		return true;
	}

	bool FPointIO::StageOutput(FPCGExContext* TargetContext, const int32 MinPointCount, const int32 MaxPointCount) const
	{
		if (Out)
		{
			const int64 OutNumPoints = Out->GetNumPoints();
			if ((MinPointCount >= 0 && OutNumPoints < MinPointCount) ||
				(MaxPointCount >= 0 && OutNumPoints > MaxPointCount))
			{
				return StageOutput(TargetContext);
			}
		}
		return false;
	}

	bool FPointIO::StageAnyOutput(FPCGExContext* TargetContext) const
	{
		if (!IsEnabled()) { return false; }

		if (bTransactional)
		{
			if (InitializationData)
			{
				UPCGData* MutableData = const_cast<UPCGData*>(InitializationData.Get());
				if (!MutableData) { return false; }

				TargetContext->StageOutput(OutputPin, MutableData, Tags->Flatten(), false, false);
				return true;
			}

			return false;
		}

		if (!Out || (!bAllowEmptyOutput && Out->IsEmpty())) { return false; }

		return true;
	}

	void FPointIO::Gather(const TArrayView<int32> InIndices) const
	{
		if (!Out) { return; }

		const int32 ReducedNum = InIndices.Num();

		if (ReducedNum == Out->GetNumPoints()) { return; }

		PCGEX_FOREACH_POINT_NATIVE_PROPERTY_GET(Out)

		Out->AllocateProperties(EPCGPointNativeProperties::All);

		for (int i = 0; i < ReducedNum; i++)
		{
#define PCGEX_VALUERANGE_GATHER(_NAME, _TYPE) _NAME##ValueRange[i] = _NAME##ValueRange[InIndices[i]];
			PCGEX_FOREACH_POINT_NATIVE_PROPERTY(PCGEX_VALUERANGE_GATHER)
#undef PCGEX_VALUERANGE_GATHER
		}

		Out->SetNumPoints(ReducedNum);
	}

	void FPointIO::DeleteAttribute(FName AttributeName) const
	{
		if (!Out) { return; }

		{
			FWriteScopeLock WriteScopeLock(AttributesLock);
			if (Out->Metadata->HasAttribute(AttributeName)) { Out->Metadata->DeleteAttribute(AttributeName); }
		}
	}

#pragma endregion

#pragma region FPointIOCollection

	FPointIOCollection::FPointIOCollection(FPCGExContext* InContext, const bool bIsTransactional)
		: ContextHandle(InContext->GetOrCreateHandle()), bTransactional(bIsTransactional)
	{
		PCGEX_LOG_CTR(FPointIOCollection)
	}

	FPointIOCollection::FPointIOCollection(FPCGExContext* InContext, const FName InputLabel, const EIOInit InitOut, const bool bIsTransactional)
		: FPointIOCollection(InContext, bIsTransactional)
	{
		TArray<FPCGTaggedData> Sources = InContext->InputData.GetInputsByPin(InputLabel);
		Initialize(Sources, InitOut);
	}

	FPointIOCollection::FPointIOCollection(FPCGExContext* InContext, TArray<FPCGTaggedData>& Sources, const EIOInit InitOut, const bool bIsTransactional)
		: FPointIOCollection(InContext, bIsTransactional)
	{
		Initialize(Sources, InitOut);
	}

	FPointIOCollection::~FPointIOCollection() = default;

	void FPointIOCollection::Initialize(
		TArray<FPCGTaggedData>& Sources,
		const EIOInit InitOut)
	{
		FPCGContext::FSharedContext<FPCGExContext> SharedContext(ContextHandle);
		if (!SharedContext.Get()) { return; }

		Pairs.Empty(Sources.Num());
		TSet<uint64> UniqueData;
		UniqueData.Reserve(Sources.Num());

		for (int i = 0; i < Sources.Num(); i++)
		{
			FPCGTaggedData& Source = Sources[i];
			bool bIsAlreadyInSet;
			UniqueData.Add(Source.Data->UID, &bIsAlreadyInSet);
			if (bIsAlreadyInSet) { continue; } // Dedupe

			const UPCGBasePointData* SourcePointData = PCGExPointIO::GetPointData(SharedContext.Get(), Source);
			if (!SourcePointData && bTransactional)
			{
				// Only allowed for execution-time only
				// Otherwise find a way to ensure conversion is plugged to the outputs, pin-less
				check(InitOut == EIOInit::None)
				SourcePointData = PCGExPointIO::ToPointData(SharedContext.Get(), Source);
			}

			if (!SourcePointData || SourcePointData->IsEmpty()) { continue; }
			const TSharedPtr<FPointIO> NewIO = Emplace_GetRef(SourcePointData, InitOut, &Source.Tags);
			NewIO->bTransactional = bTransactional;
			NewIO->InitializationIndex = i;
			NewIO->InitializationData = Source.Data;
		}
		UniqueData.Empty();
	}

	TSharedPtr<FPointIO> FPointIOCollection::Emplace_GetRef(
		const UPCGBasePointData* In,
		const EIOInit InitOut,
		const TSet<FString>* Tags)
	{
		FWriteScopeLock WriteLock(PairsLock);
		TSharedPtr<FPointIO> NewIO = Pairs.Add_GetRef(MakeShared<FPointIO>(ContextHandle, In));
		NewIO->SetInfos(Pairs.Num() - 1, OutputPin, Tags);
		if (!NewIO->InitializeOutput(InitOut)) { return nullptr; }
		return NewIO;
	}

	TSharedPtr<FPointIO> FPointIOCollection::Emplace_GetRef(const EIOInit InitOut)
	{
		FWriteScopeLock WriteLock(PairsLock);
		TSharedPtr<FPointIO> NewIO = Pairs.Add_GetRef(MakeShared<FPointIO>(ContextHandle));
		NewIO->SetInfos(Pairs.Num() - 1, OutputPin);
		if (!NewIO->InitializeOutput(InitOut)) { return nullptr; }
		return NewIO;
	}

	TSharedPtr<FPointIO> FPointIOCollection::Emplace_GetRef(const TSharedPtr<FPointIO>& PointIO, const EIOInit InitOut)
	{
		TSharedPtr<FPointIO> Branch = Emplace_GetRef(PointIO->GetIn(), InitOut);
		if (!Branch) { return nullptr; }

		Branch->Tags->Reset(PointIO->Tags);
		Branch->RootIO = PointIO;
		return Branch;
	}

	TSharedPtr<FPointIO> FPointIOCollection::Insert_Unsafe(const int32 Index, const TSharedPtr<FPointIO>& PointIO)
	{
		check(!Pairs[Index]) // should be an empty spot
		Pairs[Index] = PointIO;
		PointIO->SetInfos(Index, OutputPin);
		return PointIO;
	}

	TSharedPtr<FPointIO> FPointIOCollection::Add_Unsafe(const TSharedPtr<FPointIO>& PointIO)
	{
		PointIO->SetInfos(Pairs.Add(PointIO), OutputPin);
		return PointIO;
	}

	void FPointIOCollection::Add_Unsafe(const TArray<TSharedPtr<FPointIO>>& IOs)
	{
		if (IOs.IsEmpty()) { return; }
		Pairs.Reserve(Pairs.Num() + IOs.Num());
		for (const TSharedPtr<FPointIO>& IO : IOs)
		{
			if (!IO) { continue; }
			Add_Unsafe(IO);
		}
	}

	void FPointIOCollection::IncreaseReserve(const int32 InIncreaseNum)
	{
		FWriteScopeLock WriteLock(PairsLock);
		Pairs.Reserve(Pairs.Max() + InIncreaseNum);
	}

	void FPointIOCollection::StageOutputs()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPointIOCollection::StageOutputs);

		FPCGContext::FSharedContext<FPCGExContext> SharedContext(ContextHandle);
		FPCGExContext* Context = SharedContext.Get();
		if (!Context) { return; }

		Sort();
		SharedContext.Get()->IncreaseStagedOutputReserve(Pairs.Num());
		for (int i = 0; i < Pairs.Num(); i++) { Pairs[i]->StageOutput(Context); }
	}

	void FPointIOCollection::StageOutputs(const int32 MinPointCount, const int32 MaxPointCount)
	{
		FPCGContext::FSharedContext<FPCGExContext> SharedContext(ContextHandle);
		FPCGExContext* Context = SharedContext.Get();
		if (!Context) { return; }

		Sort();
		SharedContext.Get()->IncreaseStagedOutputReserve(Pairs.Num());
		for (int i = 0; i < Pairs.Num(); i++) { Pairs[i]->StageOutput(Context, MinPointCount, MaxPointCount); }
	}

	void FPointIOCollection::StageAnyOutputs()
	{
		FPCGContext::FSharedContext<FPCGExContext> SharedContext(ContextHandle);
		FPCGExContext* Context = SharedContext.Get();
		if (!Context) { return; }

		Sort();
		SharedContext.Get()->IncreaseStagedOutputReserve(Pairs.Num());
		for (int i = 0; i < Pairs.Num(); i++) { Pairs[i]->StageAnyOutput(Context); }
	}

	void FPointIOCollection::Sort()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPointIOCollection::Sort);
		Pairs.Sort([](const TSharedPtr<FPointIO>& A, const TSharedPtr<FPointIO>& B) { return A->IOIndex < B->IOIndex; });
	}

	FBox FPointIOCollection::GetInBounds() const
	{
		FBox Bounds = FBox(ForceInit);
		for (const TSharedPtr<FPointIO>& IO : Pairs) { Bounds += IO->GetIn()->GetBounds(); }
		return Bounds;
	}

	FBox FPointIOCollection::GetOutBounds() const
	{
		FBox Bounds = FBox(ForceInit);
		for (const TSharedPtr<FPointIO>& IO : Pairs) { Bounds += IO->GetOut()->GetBounds(); }
		return Bounds;
	}

	void FPointIOCollection::PruneNullEntries(const bool bUpdateIndices)
	{
		const int32 MaxPairs = Pairs.Num();
		int32 WriteIndex = 0;
		if (!bUpdateIndices)
		{
			for (int32 i = 0; i < MaxPairs; i++) { if (Pairs[i]) { Pairs[WriteIndex++] = Pairs[i]; } }
		}
		else
		{
			for (int32 i = 0; i < MaxPairs; i++)
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
		Pairs.Empty();
	}

#pragma endregion

#pragma region FPointIOTaggedEntries

	void FPointIOTaggedEntries::Add(const TSharedRef<FPointIO>& Value)
	{
		Entries.AddUnique(Value);
		Value->Tags->Set(TagId, TagValue);
	}

	bool FPointIOTaggedDictionary::CreateKey(const TSharedRef<FPointIO>& PointIOKey)
	{
		IDType TagValue = PointIOKey->Tags->GetOrSet<int32>(TagId, PointIOKey->GetInOut()->GetUniqueID());
		for (const TSharedPtr<FPointIOTaggedEntries>& Binding : Entries)
		{
			// TagValue shouldn't exist already
			if (Binding->TagValue->Value == TagValue->Value) { return false; }
		}

		TagMap.Add(TagValue->Value, Entries.Add(MakeShared<FPointIOTaggedEntries>(TagId, TagValue)));
		return true;
	}

	bool FPointIOTaggedDictionary::TryAddEntry(const TSharedRef<FPointIO>& PointIOEntry)
	{
		const IDType TagValue = PointIOEntry->Tags->GetTypedValue<int32>(TagId);
		if (!TagValue) { return false; }

		if (const int32* Index = TagMap.Find(TagValue->Value))
		{
			Entries[*Index]->Add(PointIOEntry);
			return true;
		}

		return false;
	}

	TSharedPtr<FPointIOTaggedEntries> FPointIOTaggedDictionary::GetEntries(const int32 Key)
	{
		if (const int32* Index = TagMap.Find(Key)) { return Entries[*Index]; }
		return nullptr;
	}

	int32 PCGExPointIO::GetTotalPointsNum(const TArray<TSharedPtr<FPointIO>>& InIOs, const EIOSide InSide)
	{
		int32 TotalNum = 0;

		if (InSide == EIOSide::In)
		{
			for (const TSharedPtr<FPointIO>& IO : InIOs)
			{
				if (!IO || !IO->GetIn()) { continue; }
				TotalNum += IO->GetIn()->GetNumPoints();
			}
		}
		else
		{
			for (const TSharedPtr<FPointIO>& IO : InIOs)
			{
				if (!IO || !IO->GetOut()) { continue; }
				TotalNum += IO->GetOut()->GetNumPoints();
			}
		}

		return TotalNum;
	}

#pragma endregion
}
