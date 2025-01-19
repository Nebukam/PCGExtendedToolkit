// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExAttributeHasher.h"

namespace PCGEx
{
	FAttributeHasher::FAttributeHasher(const FPCGExAttributeHashConfig& InConfig)
		: Config(InConfig)
	{
	}

	bool FAttributeHasher::Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InPointIO)
	{
		NumValues = InPointIO->GetNum();

		if (NumValues <= 0) { return false; }

		ValuesGetter = MakeShared<TAttributeBroadcaster<PCGExTypeHash>>();
		if (!ValuesGetter->Prepare(Config.SourceAttribute, InPointIO))
		{
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Missing attribute {0}."), FText::FromName(Config.SourceAttribute.GetName())));
			return false;
		}

		if (!RequiresCompilation())
		{
			const PCGExTypeHash A = ValuesGetter->SoftGet(InPointIO->GetInPointRef(0), 0);
			const PCGExTypeHash B = ValuesGetter->SoftGet(InPointIO->GetInPointRef(NumValues - 1), 0);

			if (Config.Scope == EPCGExDataHashScope::First) { OutHash = A; }
			else if (Config.Scope == EPCGExDataHashScope::Last) { OutHash = B; }
			else if (Config.Scope == EPCGExDataHashScope::FirstAndLast)
			{
				if (Config.bSortInputValues)
				{
					if (Config.Sorting == EPCGExSortDirection::Ascending) { OutHash = A < B ? HashCombineFast(A, B) : HashCombineFast(B, A); }
					else { OutHash = A < B ? HashCombineFast(B, A) : HashCombineFast(A, B); }
				}
				else { OutHash = HashCombineFast(A, B); }
			}
		}
		else
		{
			UniqueValues.Reserve(NumValues);
			UniqueIndices.Reserve(NumValues);

			Values.SetNumUninitialized(NumValues);
			CombinedHashUnique = OutHash;
		}


		return true;
	}

	bool FAttributeHasher::RequiresCompilation()
	{
		switch (Config.Scope)
		{
		case EPCGExDataHashScope::All:
		case EPCGExDataHashScope::Uniques:
			return true;
		case EPCGExDataHashScope::FirstAndLast:
		case EPCGExDataHashScope::First:
		case EPCGExDataHashScope::Last:
			return false;
		}

		return false;
	}

	void FAttributeHasher::Compile(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, PCGExMT::FSimpleCallback&& InCallback)
	{
		CompleteCallback = InCallback;

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, CompileHash)
		CompileHash->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE, AsyncManager]()
			{
				PCGEX_ASYNC_THIS
				This->OnCompilationComplete();
			};

		CompileHash->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				This->CompileScope(Scope);
			};

		CompileHash->StartSubLoops(NumValues, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}

	void FAttributeHasher::CompileScope(const PCGExMT::FScope& Scope)
	{
		ValuesGetter->Fetch(Values, Scope);
		for (int i = Scope.Start; i < Scope.End; i++)
		{
			PCGExTypeHash H = Values[i];

			OutHash = HashCombineFast(OutHash, H);

			bool bAlreadySet = false;
			UniqueValues.Add(H, &bAlreadySet);
			if (!bAlreadySet)
			{
				CombinedHashUnique = HashCombineFast(CombinedHashUnique, H);
				UniqueIndices.Add(i);
			}
		}
	}

	void FAttributeHasher::OnCompilationComplete()
	{
		if (Config.Scope == EPCGExDataHashScope::All)
		{
			if (Config.bSortInputValues)
			{
				OutHash = 0;
				if (Config.Sorting == EPCGExSortDirection::Ascending) { Values.Sort([](const uint32 A, const uint32 B) { return A < B; }); }
				else { Values.Sort([](const uint32 A, const uint32 B) { return A > B; }); }

				for (const uint32 C : Values) { OutHash = HashCombineFast(OutHash, C); }
			}
		}
		else if (Config.Scope == EPCGExDataHashScope::Uniques)
		{
			if (!Config.bSortInputValues)
			{
				OutHash = CombinedHashUnique;
			}
			else
			{
				OutHash = 0;
				if (Config.Sorting == EPCGExSortDirection::Ascending) { UniqueIndices.Sort([&](const int32 A, const int32 B) { return Values[A] < Values[B]; }); }
				else { UniqueIndices.Sort([&](const int32 A, const int32 B) { return Values[A] > Values[B]; }); }

				for (const int32 Index : UniqueIndices) { OutHash = HashCombineFast(OutHash, Values[Index]); }
			}
		}

		if (CompleteCallback) { CompleteCallback(); }
	}
}
