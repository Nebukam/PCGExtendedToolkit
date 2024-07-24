// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExMergePointsByTag.h"

#include "Data/PCGExPointIOMerger.h"

#define LOCTEXT_NAMESPACE "PCGExMergePointsByTagElement"
#define PCGEX_NAMESPACE MergePointsByTag

namespace PCPGExMergePointsByTag
{
	FMergeList::FMergeList()
	{
	}

	FMergeList::~FMergeList()
	{
		IOs.Empty();
		PCGEX_DELETE(Merger)
	}

	void FMergeList::Merge(PCGExMT::FTaskManager* AsyncManager, const FPCGExCarryOverDetails* InCarryOverDetails)
	{
		CompositeIO = IOs[0];
		CompositeIO->InitializeOutput(PCGExData::EInit::NewOutput);

		Merger = new FPCGExPointIOMerger(CompositeIO);
		Merger->Append(IOs);
		Merger->Merge(AsyncManager, InCarryOverDetails);
	}

	void FMergeList::Write(PCGExMT::FTaskManager* AsyncManager) const
	{
		Merger->Write(AsyncManager);
	}

	FTagBucket::FTagBucket(const FString& InTag): Tag(InTag)
	{
	}

	FTagBucket::~FTagBucket()
	{
		IOs.Empty();
	}

	FTagBuckets::FTagBuckets()
	{
	}

	FTagBuckets::~FTagBuckets()
	{
		PCGEX_DELETE_TARRAY(Buckets)
		BucketsMap.Empty();

		TArray<PCGExData::FPointIO*> Keys;
		ReverseBucketsMap.GetKeys(Keys);
		for (const PCGExData::FPointIO* Key : Keys)
		{
			const TSet<FTagBucket*>* BucketSet = ReverseBucketsMap[Key];
			PCGEX_DELETE(BucketSet)
		}

		ReverseBucketsMap.Empty();
	}

	void FTagBuckets::Distribute(PCGExData::FPointIO* IO, const FPCGExNameFiltersDetails& Filters)
	{
		bool bDistributed = false;
		if (!IO->Tags->IsEmpty())
		{
			for (TSet<FString> FlattenedTags = IO->Tags->ToSet(); FString Tag : FlattenedTags)
			{
				if (!Filters.Test(Tag)) { continue; }

				if (const int32* BucketIndex = BucketsMap.Find(Tag))
				{
					FTagBucket* ExistingBucket = Buckets[*BucketIndex];
					ExistingBucket->IOs.Add(IO);
					AddToReverseMap(IO, ExistingBucket);
					bDistributed = true;
					continue;
				}

				FTagBucket* NewBucket = new FTagBucket(Tag);
				BucketsMap.Add(Tag, Buckets.Add(NewBucket));
				AddToReverseMap(IO, NewBucket);
				bDistributed = true;
				NewBucket->IOs.Add(IO);
			}
		}

		if (!bDistributed) { IO->InitializeOutput(PCGExData::EInit::Forward); }
	}

	void FTagBuckets::AddToReverseMap(PCGExData::FPointIO* IO, FTagBucket* Bucket)
	{
		if (TSet<FTagBucket*>** BucketSet = ReverseBucketsMap.Find(IO))
		{
			(*BucketSet)->Add(Bucket);
			return;
		}

		TSet<FTagBucket*>* NewBucketSet = new TSet<FTagBucket*>();
		NewBucketSet->Add(Bucket);
		ReverseBucketsMap.Add(IO, NewBucketSet);
	}

	void FTagBuckets::BuildMergeLists(EPCGExMergeByTagOverlapResolutionMode Mode, TArray<FMergeList*>& OutLists, const TArray<FString>& Priorities)
	{
		Buckets.Sort(
			[&](const FTagBucket& A, const FTagBucket& B)
			{
				int32 RatingA = Priorities.Find(A.Tag);
				int32 RatingB = Priorities.Find(B.Tag);
				if (RatingA < 0) { RatingA = TNumericLimits<int32>::Max(); }
				if (RatingB < 0) { RatingB = TNumericLimits<int32>::Max(); }
				return RatingA < RatingB;
			});

		TSet<PCGExData::FPointIO*> Distributed;

		switch (Mode)
		{
		default: ;
		case EPCGExMergeByTagOverlapResolutionMode::Strict:
			for (FTagBucket* Bucket : Buckets)
			{
				if (Bucket->IOs.IsEmpty()) { continue; }
				bool bAlreadyDistributed;

				if (Bucket->IOs.Num() == 1)
				{
					PCGExData::FPointIO* IO = Bucket->IOs[0];

					Distributed.Add(IO, &bAlreadyDistributed);
					if (bAlreadyDistributed) { continue; }

					IO->InitializeOutput(PCGExData::EInit::Forward);
					Bucket->IOs.Empty();

					continue;
				}

				FMergeList* NewMergeList = new FMergeList();
				OutLists.Add(NewMergeList);
				for (PCGExData::FPointIO* IO : Bucket->IOs)
				{
					Distributed.Add(IO, &bAlreadyDistributed);
					if (bAlreadyDistributed) { continue; }

					NewMergeList->IOs.Add(IO);
				}

				Bucket->IOs.Empty();
			}
			break;
		case EPCGExMergeByTagOverlapResolutionMode::ImmediateOverlap:
			for (FTagBucket* Bucket : Buckets)
			{
				if (Bucket->IOs.IsEmpty()) { continue; }
				bool bAlreadyDistributed;

				FMergeList* NewMergeList = new FMergeList();

				for (PCGExData::FPointIO* IO : Bucket->IOs)
				{
					Distributed.Add(IO, &bAlreadyDistributed);
					if (bAlreadyDistributed) { continue; }

					NewMergeList->IOs.Add(IO);

					// Get all buckets that share this IO
					if (TSet<FTagBucket*>** OverlappingBuckets = ReverseBucketsMap.Find(IO))
					{
						for (FTagBucket* OverlappingBucket : (**OverlappingBuckets))
						{
							if (OverlappingBucket == Bucket) { continue; }
							for (PCGExData::FPointIO* OtherIO : OverlappingBucket->IOs)
							{
								Distributed.Add(OtherIO, &bAlreadyDistributed);
								if (bAlreadyDistributed) { continue; }

								NewMergeList->IOs.Add(OtherIO);
							}
							OverlappingBucket->IOs.Empty();
						}
					}
				}

				if (NewMergeList->IOs.Num() <= 1)
				{
					if (NewMergeList->IOs.Num() == 1) { NewMergeList->IOs[0]->InitializeOutput(PCGExData::EInit::Forward); }
					PCGEX_DELETE(NewMergeList)
					continue;
				}

				OutLists.Add(NewMergeList);
			}
			break;
		}
	}
}

PCGExData::EInit UPCGExMergePointsByTagSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(MergePointsByTag)

FPCGExMergePointsByTagContext::~FPCGExMergePointsByTagContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_DELETE_TARRAY(MergeLists)
}

bool FPCGExMergePointsByTagElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(MergePointsByTag)

	PCGEX_FWD(TagFilters)
	Context->TagFilters.Init();

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

	PCPGExMergePointsByTag::FTagBuckets* Buckets = new PCPGExMergePointsByTag::FTagBuckets();
	for (PCGExData::FPointIO* IO : Context->MainPoints->Pairs) { Buckets->Distribute(IO, Context->TagFilters); }
	Buckets->BuildMergeLists(Settings->Mode, Context->MergeLists, Settings->ResolutionPriorities);
	PCGEX_DELETE(Buckets)

	return true;
}

bool FPCGExMergePointsByTagElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMergePointsByTagElement::Execute);

	PCGEX_CONTEXT(MergePointsByTag)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		for (PCPGExMergePointsByTag::FMergeList* List : Context->MergeLists) { List->Merge(Context->GetAsyncManager(), &Context->CarryOverDetails); }
		Context->SetAsyncState(PCGExData::State_MergingData);
	}

	if (Context->IsState(PCGExData::State_MergingData))
	{
		PCGEX_ASYNC_WAIT

		for (PCPGExMergePointsByTag::FMergeList* List : Context->MergeLists) { List->Write(Context->GetAsyncManager()); }
		Context->SetAsyncState(PCGExMT::State_Writing);
	}

	if (Context->IsState(PCGExMT::State_Writing))
	{
		PCGEX_ASYNC_WAIT

		Context->MainPoints->OutputToContext();
		Context->Done();
	}

	return Context->TryComplete();
}

namespace PCGExMergePointsByTag
{
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
