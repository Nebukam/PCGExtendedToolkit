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

	void FMergeList::Merge(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const FPCGExCarryOverDetails* InCarryOverDetails)
	{
		if (IOs.IsEmpty()) { return; }

		const TSharedPtr<PCGExData::FPointIO> CompositeIO = IOs[0];
		CompositeIO->InitializeOutput(AsyncManager->Context, PCGExData::EInit::NewOutput);

		CompositeIODataFacade = MakeShared<PCGExData::FFacade>(CompositeIO.ToSharedRef());

		Merger = MakeShared<FPCGExPointIOMerger>(CompositeIODataFacade.ToSharedRef());
		Merger->Append(IOs);
		Merger->Merge(AsyncManager, InCarryOverDetails);
	}

	void FMergeList::Write(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) const
	{
		CompositeIODataFacade->Write(AsyncManager);
	}

	FTagBucket::FTagBucket(const FString& InTag): Tag(InTag)
	{
	}

	FTagBuckets::FTagBuckets()
	{
	}

	void FTagBuckets::Distribute(FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIO>& IO, const FPCGExNameFiltersDetails& Filters)
	{
		bool bDistributed = false;
		if (!IO->Tags->IsEmpty())
		{
			for (TSet<FString> FlattenedTags = IO->Tags->ToSet(); FString Tag : FlattenedTags)
			{
				if (!Filters.Test(Tag)) { continue; }

				if (const int32* BucketIndex = BucketsMap.Find(Tag))
				{
					TSharedPtr<FTagBucket> ExistingBucket = Buckets[*BucketIndex];
					ExistingBucket->IOs.Add(IO);
					AddToReverseMap(IO, ExistingBucket);
					bDistributed = true;
					continue;
				}

				TSharedPtr<FTagBucket> NewBucket = MakeShared<FTagBucket>(Tag);
				BucketsMap.Add(Tag, Buckets.Add(NewBucket));
				AddToReverseMap(IO, NewBucket);
				bDistributed = true;
				NewBucket->IOs.Add(IO);
			}
		}

		if (!bDistributed) { IO->InitializeOutput(InContext, PCGExData::EInit::Forward); }
	}

	void FTagBuckets::AddToReverseMap(TSharedPtr<PCGExData::FPointIO> IO, const TSharedPtr<FTagBucket>& Bucket)
	{
		if (const TSharedPtr<TSet<TSharedPtr<FTagBucket>>>* BucketSet = ReverseBucketsMap.Find(IO.Get()))
		{
			(*BucketSet)->Add(Bucket);
			return;
		}

		const TSharedPtr<TSet<TSharedPtr<FTagBucket>>> NewBucketSet = MakeShared<TSet<TSharedPtr<FTagBucket>>>();
		NewBucketSet->Add(Bucket);
		ReverseBucketsMap.Add(IO.Get(), NewBucketSet);
	}

	void FTagBuckets::BuildMergeLists(FPCGExContext* InContext, const EPCGExMergeByTagOverlapResolutionMode Mode, TArray<TSharedPtr<FMergeList>>& OutLists, const TArray<FString>& Priorities, const EPCGExSortDirection SortDirection)
	{
		if (Priorities.IsEmpty())
		{
			if (SortDirection == EPCGExSortDirection::Ascending)
			{
				Buckets.Sort([&](const TSharedPtr<FTagBucket>& A, const TSharedPtr<FTagBucket>& B) { return A->IOs.Num() < B->IOs.Num(); });
			}
			else
			{
				Buckets.Sort([&](const TSharedPtr<FTagBucket>& A, const TSharedPtr<FTagBucket>& B) { return A->IOs.Num() > B->IOs.Num(); });
			}
		}
		else
		{
			if (SortDirection == EPCGExSortDirection::Ascending)
			{
				Buckets.Sort(
					[&](const TSharedPtr<FTagBucket>& A, const TSharedPtr<FTagBucket>& B)
					{
						int32 RatingA = Priorities.Find(A->Tag);
						int32 RatingB = Priorities.Find(B->Tag);
						if (RatingA < 0) { RatingA = TNumericLimits<int32>::Max(); }
						if (RatingB < 0) { RatingB = TNumericLimits<int32>::Max(); }
						return RatingA == RatingB ? A->IOs.Num() < B->IOs.Num() : RatingA < RatingB;
					});
			}
			else
			{
				Buckets.Sort(
					[&](const TSharedPtr<FTagBucket>& A, const TSharedPtr<FTagBucket>& B)
					{
						int32 RatingA = Priorities.Find(A->Tag);
						int32 RatingB = Priorities.Find(B->Tag);
						if (RatingA < 0) { RatingA = TNumericLimits<int32>::Max(); }
						if (RatingB < 0) { RatingB = TNumericLimits<int32>::Max(); }
						return RatingA == RatingB ? A->IOs.Num() > B->IOs.Num() : RatingA < RatingB;
					});
			}
		}


		TSet<TSharedPtr<PCGExData::FPointIO>> Distributed;

		switch (Mode)
		{
		default: ;
		case EPCGExMergeByTagOverlapResolutionMode::Strict:
			for (const TSharedPtr<FTagBucket> Bucket : Buckets)
			{
				if (Bucket->IOs.IsEmpty()) { continue; }
				bool bAlreadyDistributed;

				if (Bucket->IOs.Num() == 1)
				{
					TSharedPtr<PCGExData::FPointIO> IO = Bucket->IOs[0];

					Distributed.Add(IO, &bAlreadyDistributed);
					if (bAlreadyDistributed) { continue; }

					IO->InitializeOutput(InContext, PCGExData::EInit::Forward);
					Bucket->IOs.Empty();

					continue;
				}

				TSharedPtr<FMergeList> NewMergeList = MakeShared<FMergeList>();
				for (TSharedPtr<PCGExData::FPointIO> IO : Bucket->IOs)
				{
					Distributed.Add(IO, &bAlreadyDistributed);
					if (bAlreadyDistributed) { continue; }

					NewMergeList->IOs.Add(IO);
				}

				if (!NewMergeList->IOs.IsEmpty()) { OutLists.Add(NewMergeList); }
				Bucket->IOs.Empty();
			}
			break;
		case EPCGExMergeByTagOverlapResolutionMode::ImmediateOverlap:
			for (TSharedPtr<FTagBucket> Bucket : Buckets)
			{
				if (Bucket->IOs.IsEmpty()) { continue; }
				bool bAlreadyDistributed;

				TSharedPtr<FMergeList> NewMergeList = MakeShared<FMergeList>();

				for (TSharedPtr<PCGExData::FPointIO> IO : Bucket->IOs)
				{
					Distributed.Add(IO, &bAlreadyDistributed);
					if (bAlreadyDistributed) { continue; }

					NewMergeList->IOs.Add(IO);

					// Get all buckets that share this IO
					if (TSharedPtr<TSet<TSharedPtr<FTagBucket>>>* OverlappingBuckets = ReverseBucketsMap.Find(IO.Get()))
					{
						for (TSharedPtr<FTagBucket> OverlappingBucket : (**OverlappingBuckets))
						{
							if (OverlappingBucket == Bucket) { continue; }
							for (TSharedPtr<PCGExData::FPointIO> OtherIO : OverlappingBucket->IOs)
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
					if (NewMergeList->IOs.Num() == 1) { NewMergeList->IOs[0]->InitializeOutput(InContext, PCGExData::EInit::Forward); }
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

bool FPCGExMergePointsByTagElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(MergePointsByTag)

	PCGEX_FWD(TagFilters)
	Context->TagFilters.Init();

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

	return true;
}

bool FPCGExMergePointsByTagElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMergePointsByTagElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(MergePointsByTag)
	PCGEX_EXECUTION_CHECK

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (Settings->Mode == EPCGExMergeByTagOverlapResolutionMode::Flatten)
		{
			for (TSharedPtr<PCGExData::FPointIO> IO : Context->MainPoints->Pairs)
			{
				TArray<FString> Tags = IO->Tags->ToSet().Array();
				Context->TagFilters.Prune(Tags);

				if (Tags.IsEmpty())
				{
					switch (Settings->FallbackBehavior)
					{
					default:
					case EPCGExMergeByTagFallbackBehavior::Omit:
						break;
					case EPCGExMergeByTagFallbackBehavior::Merge:
						if (!Context->FallbackMergeList) { Context->FallbackMergeList = MakeShared<PCPGExMergePointsByTag::FMergeList>(); }
						Context->FallbackMergeList->IOs.Add(IO);
						break;
					case EPCGExMergeByTagFallbackBehavior::Forward:
						IO->InitializeOutput(Context, PCGExData::EInit::Forward);
						break;
					}
					continue;
				}

				Tags.Sort([&](const FString& A, const FString& B) { return A > B; });
				const uint32 Hash = GetTypeHash(FString::Join(Tags, TEXT("")));

				const TSharedPtr<PCPGExMergePointsByTag::FMergeList>* MergeListPtr = Context->MergeMap.Find(Hash);
				TSharedPtr<PCPGExMergePointsByTag::FMergeList> MergeList = nullptr;
				if (!MergeListPtr)
				{
					MergeList = MakeShared<PCPGExMergePointsByTag::FMergeList>();
					Context->MergeMap.Add(Hash, MergeList);
					Context->MergeLists.Add(MergeList);
				}
				else
				{
					MergeList = *MergeListPtr;
				}

				MergeList->IOs.Add(IO);
			}
		}
		else
		{
			// Bucket IOs
			TSharedPtr<PCPGExMergePointsByTag::FTagBuckets> Buckets = MakeShared<PCPGExMergePointsByTag::FTagBuckets>();
			for (TSharedPtr<PCGExData::FPointIO> IO : Context->MainPoints->Pairs) { Buckets->Distribute(Context, IO, Context->TagFilters); }
			Buckets->BuildMergeLists(Context, Settings->Mode, Context->MergeLists, Settings->ResolutionPriorities, Settings->SortDirection);
		}

		if (Context->FallbackMergeList) { Context->FallbackMergeList->Merge(Context->GetAsyncManager(), &Context->CarryOverDetails); }
		for (TSharedPtr<PCPGExMergePointsByTag::FMergeList> List : Context->MergeLists) { List->Merge(Context->GetAsyncManager(), &Context->CarryOverDetails); }
		Context->SetAsyncState(PCGExData::State_MergingData);
	}

	if (Context->IsState(PCGExData::State_MergingData))
	{
		PCGEX_ASYNC_WAIT

		for (TSharedPtr<PCPGExMergePointsByTag::FMergeList> List : Context->MergeLists) { List->Write(Context->GetAsyncManager()); }
		Context->SetAsyncState(PCGExMT::State_Writing);
	}

	if (Context->IsState(PCGExMT::State_Writing))
	{
		PCGEX_ASYNC_WAIT

		Context->MainPoints->StageOutputs();
		Context->Done();
	}

	return Context->TryComplete();
}

namespace PCGExMergePointsByTag
{
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
