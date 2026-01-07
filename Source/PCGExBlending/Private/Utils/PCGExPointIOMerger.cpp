// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Utils/PCGExPointIOMerger.h"

#include "Data/PCGExDataTags.h"
#include "Details/PCGExBlendingDetails.h"

namespace PCGExPointIOMerger
{
	template <typename T>
	class FWriteAttributeScopeTask final : public PCGExMT::FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FWriteAttributeScopeTask)

		FWriteAttributeScopeTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO, const FMergeScope& InScope, const FIdentityRef& InIdentity, const TSharedPtr<PCGExData::TBuffer<T>>& InOutBuffer)
			: FTask(), PointIO(InPointIO), Scope(InScope), Identity(InIdentity), OutBuffer(InOutBuffer)
		{
		}

		const TSharedPtr<PCGExData::FPointIO> PointIO;
		const FMergeScope Scope;
		const FIdentityRef Identity;
		const TSharedPtr<PCGExData::TBuffer<T>> OutBuffer;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager) override
		{
			ScopeMerge<T>(Scope, Identity, PointIO, OutBuffer);
		}
	};

#define PCGEX_TPL(_TYPE, _NAME, ...) template class FWriteAttributeScopeTask<_TYPE>;

	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)

#undef PCGEX_TPL

	class FCopyAttributeTask final : public PCGExMT::FPCGExIndexedTask
	{
	public:
		FCopyAttributeTask(const int32 InTaskIndex, const TSharedPtr<FPCGExPointIOMerger>& InMerger)
			: FPCGExIndexedTask(InTaskIndex), Merger(InMerger)
		{
		}

		TSharedPtr<FPCGExPointIOMerger> Merger;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager) override
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FCopyAttributeTask::ExecuteTask);

			const FIdentityRef& Identity = Merger->UniqueIdentities[TaskIndex];

			PCGExMetaHelpers::ExecuteWithRightType(Identity.UnderlyingType, [&](auto DummyValue)
			{
				using T = decltype(DummyValue);

				TSharedPtr<PCGExData::TBuffer<T>> Buffer = Merger->UnionDataFacade->GetWritable(
					Merger->WantsDataToElements() ? Identity.ElementsIdentifier : Identity.Identifier,
					Identity.bInitDefault ? static_cast<const FPCGMetadataAttribute<T>*>(Identity.Attribute)->GetValue(PCGDefaultValueKey) : T{},
					Identity.bAllowsInterpolation, PCGExData::EBufferInit::New);

				for (int i = 0; i < Merger->IOSources.Num(); i++)
				{
					TSharedPtr<PCGExData::FPointIO> SourceIO = Merger->IOSources[i];
					const FPCGMetadataAttributeBase* Attribute = SourceIO->GetIn()->Metadata->GetConstAttribute(Identity.Identifier);

					if (!Attribute) { continue; }                            // Missing attribute
					if (!Identity.IsA(Attribute->GetTypeId())) { continue; } // Type mismatch

					PCGEX_LAUNCH_INTERNAL(FWriteAttributeScopeTask<T>, SourceIO, Merger->Scopes[i], Identity, Buffer)
				}
			});
		}
	};
}

PCGExPointIOMerger::FIdentityRef::FIdentityRef()
	: FAttributeIdentity()
{
}

PCGExPointIOMerger::FIdentityRef::FIdentityRef(const FIdentityRef& Other)
	: FAttributeIdentity(Other)
{
}

PCGExPointIOMerger::FIdentityRef::FIdentityRef(const FAttributeIdentity& Other)
	: FAttributeIdentity(Other)
{
}

PCGExPointIOMerger::FIdentityRef::FIdentityRef(const FName InName, const EPCGMetadataTypes InUnderlyingType, const bool InAllowsInterpolation)
	: FAttributeIdentity(InName, InUnderlyingType, InAllowsInterpolation)
{
}

FPCGExPointIOMerger::FPCGExPointIOMerger(const TSharedRef<PCGExData::FFacade>& InUnionDataFacade)
	: UnionDataFacade(InUnionDataFacade)
{
}

FPCGExPointIOMerger::~FPCGExPointIOMerger()
{
}

PCGExPointIOMerger::FMergeScope& FPCGExPointIOMerger::Append(const TSharedPtr<PCGExData::FPointIO>& InData, const PCGExMT::FScope ReadScope, const PCGExMT::FScope WriteScope)
{
	const int32 NumPoints = InData->GetNum();

	check(ReadScope.IsValid());
	check(NumPoints > 0);
	check(ReadScope.End <= NumPoints);
	check(ReadScope.Count == WriteScope.Count)

	IOSources.Add(InData);

	PCGExPointIOMerger::FMergeScope& Scope = Scopes.Emplace_GetRef();
	Scope.Read = ReadScope;
	Scope.Write = WriteScope;

	MaxNumElements = FMath::Max(MaxNumElements, ReadScope.End);
	NumCompositePoints = FMath::Max(NumCompositePoints, WriteScope.End);

	EnumAddFlags(AllocateProperties, InData->GetAllocations());
	return Scope;
}

PCGExPointIOMerger::FMergeScope& FPCGExPointIOMerger::Append(const TSharedPtr<PCGExData::FPointIO>& InData, const PCGExMT::FScope ReadScope)
{
	check(InData->GetNum() >= ReadScope.Count);

	const int32 NumPoints = ReadScope.Count;

	if (NumPoints <= 0) { return NullScope; }

	const PCGExMT::FScope WriteScope = PCGExMT::FScope(NumCompositePoints, NumPoints);

	return Append(InData, ReadScope, WriteScope);
}

PCGExPointIOMerger::FMergeScope& FPCGExPointIOMerger::Append(const TSharedPtr<PCGExData::FPointIO>& InData)
{
	const int32 NumPoints = InData->GetNum();

	if (NumPoints <= 0) { return NullScope; }

	const PCGExMT::FScope ReadScope = PCGExMT::FScope(0, NumPoints);
	const PCGExMT::FScope WriteScope = PCGExMT::FScope(NumCompositePoints, NumPoints);

	return Append(InData, ReadScope, WriteScope);
}

void FPCGExPointIOMerger::Append(const TArray<TSharedPtr<PCGExData::FPointIO>>& InData)
{
	for (const TSharedPtr<PCGExData::FPointIO>& PointIO : InData) { Append(PointIO); }
}

void FPCGExPointIOMerger::MergeAsync(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager, const FPCGExCarryOverDetails* InCarryOverDetails, const TSet<FName>* InIgnoredAttributes)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPointIOMerger::MergeAsync);

	bDataDomainToElements = InCarryOverDetails->bDataDomainToElements;

	InCarryOverDetails->Prune(&UnionDataFacade->Source.Get());
	TMap<FPCGAttributeIdentifier, int32> ExpectedTypes;

	const int32 NumSources = IOSources.Num();

	for (PCGExPointIOMerger::FMergeScope& Scope : Scopes)
	{
		if (Scope.bReverse)
		{
			if (ReverseIndices.IsEmpty())
			{
				// Create a single reverse array of indices the size of the maximum number of elements
				PCGExArrayHelpers::ArrayOfIndices(ReverseIndices, MaxNumElements);
				Algo::Reverse(ReverseIndices);
			}
			Scope.ReadIndices = MakeArrayView(ReverseIndices.GetData() + (ReverseIndices.Num() - Scope.Read.End), Scope.Read.Count);
		}
	}

	for (int i = 0; i < NumSources; i++)
	{
		const TSharedPtr<PCGExData::FPointIO> Source = IOSources[i];
		UnionDataFacade->Source->Tags->Append(Source->Tags.ToSharedRef());

		// Discover attributes
		UPCGMetadata* Metadata = Source->GetIn()->Metadata;
		PCGExData::FAttributeIdentity::ForEach(Metadata, [&](const PCGExData::FAttributeIdentity& SourceIdentity, const int32)
		{
			if (InIgnoredAttributes && InIgnoredAttributes->Contains(SourceIdentity.Identifier.Name)) { return; }

			FString StrName = SourceIdentity.Identifier.Name.ToString();
			if (!InCarryOverDetails->Attributes.Test(StrName)) { return; }

			const int32* ExpectedType = ExpectedTypes.Find(SourceIdentity.Identifier);
			if (!ExpectedType)
			{
				// No type expectations, we need to register a new attribute ref
				PCGExPointIOMerger::FIdentityRef& SourceRef = UniqueIdentities.Emplace_GetRef(SourceIdentity);
				SourceRef.Attribute = Metadata->GetConstAttribute(SourceIdentity.Identifier);
				SourceRef.bInitDefault = InCarryOverDetails->bPreserveAttributesDefaultValue;

				SourceRef.ElementsIdentifier.Name = SourceIdentity.Identifier.Name;
				SourceRef.ElementsIdentifier.MetadataDomain = PCGMetadataDomainID::Elements;

				ExpectedTypes.Add(SourceRef.Identifier, UniqueIdentities.Num() - 1);

				return;
			}

			// Notify type/name mismatch if needed
			if (UniqueIdentities[*ExpectedType].UnderlyingType != SourceIdentity.UnderlyingType)
			{
				PCGE_LOG_C(Warning, GraphAndLog, TaskManager->GetContext(), FText::Format(FTEXT("Mismatching attribute types for: {0}."), FText::FromName(SourceIdentity.Identifier.Name)));
			}
		});
	}

	InCarryOverDetails->Prune(&UnionDataFacade->Source.Get());

	UPCGBasePointData* OutPointData = UnionDataFacade->GetOut();
	const bool bHasAttributes = !UniqueIdentities.IsEmpty();
	if (bHasAttributes) { EnumAddFlags(AllocateProperties, EPCGPointNativeProperties::MetadataEntry); }

	PCGExPointArrayDataHelpers::SetNumPointsAllocated(OutPointData, NumCompositePoints, AllocateProperties);

	if (bHasAttributes) { OutPointData->SetMetadataEntry(PCGInvalidEntryKey); }

	PCGEX_ASYNC_GROUP_CHKD_VOID(TaskManager, CopyProperties)
	CopyProperties->OnIterationCallback = [PCGEX_ASYNC_THIS_CAPTURE](int32 Index, const PCGExMT::FScope& Scope)
	{
		PCGEX_ASYNC_THIS
		This->CopyProperties(Index);
	};

	if (bHasAttributes)
	{
		CopyProperties->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE, TaskManager]()
		{
			PCGEX_ASYNC_THIS
			TaskManager->Launch(This->UniqueIdentities.Num(), [&](int32 i)
			{
				PCGEX_MAKE_SHARED(Task, PCGExPointIOMerger::FCopyAttributeTask, i, This);
				return Task;
			});
		};
	}

	CopyProperties->StartIterations(NumSources, 1);
}

void FPCGExPointIOMerger::CopyProperties(const int32 Index)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPointIOMerger::CopyProperties);

	const PCGExPointIOMerger::FMergeScope& Scope = Scopes[Index];
	const TSharedPtr<PCGExData::FPointIO> Source = IOSources[Index];
	UnionDataFacade->Source->Tags->Append(Source->Tags.ToSharedRef());

	if (Scope.bReverse)
	{
		TArray<int32> TempWriteIndices;
		PCGExArrayHelpers::ArrayOfIndices(TempWriteIndices, Scope.Write.Count, Scope.Write.Start);

		Source->GetIn()->CopyPropertiesTo(UnionDataFacade->GetOut(), Scope.ReadIndices, TempWriteIndices, Source->GetAllocations() & ~EPCGPointNativeProperties::MetadataEntry);
	}
	else
	{
		Source->GetIn()->CopyPropertiesTo(UnionDataFacade->GetOut(), Scope.Read.Start, Scope.Write.Start, Scope.Write.Count, Source->GetAllocations() & ~EPCGPointNativeProperties::MetadataEntry);
	}
}
