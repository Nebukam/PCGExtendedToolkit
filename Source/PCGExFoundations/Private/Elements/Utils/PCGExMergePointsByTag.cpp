// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/Utils/PCGExMergePointsByTag.h"


#include "Data/PCGExDataTags.h"
#include "Utils/PCGExPointIOMerger.h"

#define LOCTEXT_NAMESPACE "PCGExMergePointsByTagElement"
#define PCGEX_NAMESPACE MergePointsByTag

namespace PCPGExMergePointsByTag
{
	FMergeList::FMergeList()
	{
	}

	void FMergeList::Merge(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager, const FPCGExCarryOverDetails* InCarryOverDetails)
	{
		if (IOs.IsEmpty()) { return; }

		const TSharedPtr<PCGExData::FPointIO> CompositeIO = IOs[0];
		PCGEX_INIT_IO_VOID(CompositeIO, PCGExData::EIOInit::New)

		CompositeDataFacade = MakeShared<PCGExData::FFacade>(CompositeIO.ToSharedRef());

		Merger = MakeShared<FPCGExPointIOMerger>(CompositeDataFacade.ToSharedRef());
		Merger->Append(IOs);
		Merger->MergeAsync(TaskManager, InCarryOverDetails);
	}

	void FMergeList::Write(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager) const
	{
		CompositeDataFacade->WriteFastest(TaskManager);
	}

	FTagBucket::FTagBucket(const FString& InTag)
		: Tag(InTag)
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
			for (TSet<FString> FlattenedTags = IO->Tags->Flatten(); FString Tag : FlattenedTags)
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

				PCGEX_MAKE_SHARED(NewBucket, FTagBucket, Tag)
				BucketsMap.Add(Tag, Buckets.Add(NewBucket));
				AddToReverseMap(IO, NewBucket);
				bDistributed = true;
				NewBucket->IOs.Add(IO);
			}
		}

		if (!bDistributed) { PCGEX_INIT_IO_VOID(IO, PCGExData::EIOInit::Forward) }
	}

	void FTagBuckets::AddToReverseMap(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<FTagBucket>& Bucket)
	{
		if (const TSharedPtr<TSet<TSharedPtr<FTagBucket>>>* BucketSet = ReverseBucketsMap.Find(IO.Get()))
		{
			(*BucketSet)->Add(Bucket);
			return;
		}

		PCGEX_MAKE_SHARED(NewBucketSet, TSet<TSharedPtr<FTagBucket>>)
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
				Buckets.Sort([&](const TSharedPtr<FTagBucket>& A, const TSharedPtr<FTagBucket>& B)
				{
					int32 RatingA = Priorities.Find(A->Tag);
					int32 RatingB = Priorities.Find(B->Tag);
					if (RatingA < 0) { RatingA = MAX_int32; }
					if (RatingB < 0) { RatingB = MAX_int32; }
					return RatingA == RatingB ? A->IOs.Num() < B->IOs.Num() : RatingA < RatingB;
				});
			}
			else
			{
				Buckets.Sort([&](const TSharedPtr<FTagBucket>& A, const TSharedPtr<FTagBucket>& B)
				{
					int32 RatingA = Priorities.Find(A->Tag);
					int32 RatingB = Priorities.Find(B->Tag);
					if (RatingA < 0) { RatingA = MAX_int32; }
					if (RatingB < 0) { RatingB = MAX_int32; }
					return RatingA == RatingB ? A->IOs.Num() > B->IOs.Num() : RatingA < RatingB;
				});
			}
		}


		TSet<TSharedPtr<PCGExData::FPointIO>> Distributed;

		switch (Mode)
		{
		default: ;
		case EPCGExMergeByTagOverlapResolutionMode::Strict: for (const TSharedPtr<FTagBucket>& Bucket : Buckets)
			{
				if (Bucket->IOs.IsEmpty()) { continue; }
				bool bAlreadyDistributed;

				if (Bucket->IOs.Num() == 1)
				{
					TSharedPtr<PCGExData::FPointIO> IO = Bucket->IOs[0];

					Distributed.Add(IO, &bAlreadyDistributed);
					if (bAlreadyDistributed) { continue; }

					PCGEX_INIT_IO_VOID(IO, PCGExData::EIOInit::Forward)

					Bucket->IOs.Empty();

					continue;
				}

				PCGEX_MAKE_SHARED(NewMergeList, FMergeList)
				for (const TSharedPtr<PCGExData::FPointIO>& IO : Bucket->IOs)
				{
					Distributed.Add(IO, &bAlreadyDistributed);
					if (bAlreadyDistributed) { continue; }

					NewMergeList->IOs.Add(IO);
				}

				if (!NewMergeList->IOs.IsEmpty()) { OutLists.Add(NewMergeList); }
				Bucket->IOs.Empty();
			}
			break;
		case EPCGExMergeByTagOverlapResolutionMode::ImmediateOverlap: for (const TSharedPtr<FTagBucket>& Bucket : Buckets)
			{
				if (Bucket->IOs.IsEmpty()) { continue; }
				bool bAlreadyDistributed;

				PCGEX_MAKE_SHARED(NewMergeList, FMergeList)

				for (const TSharedPtr<PCGExData::FPointIO>& IO : Bucket->IOs)
				{
					Distributed.Add(IO, &bAlreadyDistributed);
					if (bAlreadyDistributed) { continue; }

					NewMergeList->IOs.Add(IO);

					// Get all buckets that share this IO
					if (const TSharedPtr<TSet<TSharedPtr<FTagBucket>>>* OverlappingBuckets = ReverseBucketsMap.Find(IO.Get()))
					{
						for (const TSharedPtr<FTagBucket>& OverlappingBucket : (**OverlappingBuckets))
						{
							if (OverlappingBucket == Bucket) { continue; }
							for (const TSharedPtr<PCGExData::FPointIO>& OtherIO : OverlappingBucket->IOs)
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
					if (NewMergeList->IOs.Num() == 1) { PCGEX_INIT_IO_VOID(NewMergeList->IOs[0], PCGExData::EIOInit::Forward) }
					continue;
				}

				OutLists.Add(NewMergeList);
			}
			break;
		}
	}
}

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

bool FPCGExMergePointsByTagElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMergePointsByTagElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(MergePointsByTag)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (Settings->Mode == EPCGExMergeByTagOverlapResolutionMode::Flatten)
		{
			for (const TSharedPtr<PCGExData::FPointIO>& IO : Context->MainPoints->Pairs)
			{
				TArray<FString> Tags = IO->Tags->Flatten().Array();
				Context->TagFilters.Prune(Tags);

				if (Tags.IsEmpty())
				{
					switch (Settings->FallbackBehavior)
					{
					default: case EPCGExMergeByTagFallbackBehavior::Omit: break;
					case EPCGExMergeByTagFallbackBehavior::Merge: if (!Context->FallbackMergeList) { Context->FallbackMergeList = MakeShared<PCPGExMergePointsByTag::FMergeList>(); }
						Context->FallbackMergeList->IOs.Add(IO);
						break;
					case EPCGExMergeByTagFallbackBehavior::Forward: IO->InitializeOutput(PCGExData::EIOInit::Forward);
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
			PCGEX_MAKE_SHARED(Buckets, PCPGExMergePointsByTag::FTagBuckets)
			for (const TSharedPtr<PCGExData::FPointIO>& IO : Context->MainPoints->Pairs) { Buckets->Distribute(Context, IO, Context->TagFilters); }
			Buckets->BuildMergeLists(Context, Settings->Mode, Context->MergeLists, Settings->ResolutionPriorities, Settings->SortDirection);
		}

		{
			TSharedPtr<PCGExMT::FTaskManager> TaskManager = Context->GetTaskManager();
			Context->SetState(PCPGExMergePointsByTag::State_MergingData);
			PCGEX_ASYNC_GROUP_CHKD_RET(TaskManager, MergeAsync, true)

			if (Context->FallbackMergeList) { Context->FallbackMergeList->Merge(TaskManager, &Context->CarryOverDetails); }
			for (const TSharedPtr<PCPGExMergePointsByTag::FMergeList>& List : Context->MergeLists)
			{
				MergeAsync->AddSimpleCallback([List, TaskManager, Det = Context->CarryOverDetails] { List->Merge(TaskManager, &Det); });
			}

			MergeAsync->StartSimpleCallbacks();
		}
	}

	PCGEX_ON_ASYNC_STATE_READY(PCPGExMergePointsByTag::State_MergingData)
	{
		Context->SetState(PCGExCommon::States::State_Writing);

		TSharedPtr<PCGExMT::FTaskManager> TaskManager = Context->GetTaskManager();
		PCGEX_SCHEDULING_SCOPE(TaskManager, true)
		for (const TSharedPtr<PCPGExMergePointsByTag::FMergeList>& List : Context->MergeLists) { List->Write(TaskManager); }
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExCommon::States::State_Writing)
	{
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
