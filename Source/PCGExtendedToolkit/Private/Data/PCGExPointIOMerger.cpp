// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/PCGExPointIOMerger.h"

#include "Data/PCGExDataFilter.h"
#include "Paths/PCGExShiftPath.h"


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

FPCGExPointIOMerger::FPCGExPointIOMerger(const TSharedRef<PCGExData::FFacade>& InUnionDataFacade):
	UnionDataFacade(InUnionDataFacade)
{
}

FPCGExPointIOMerger::~FPCGExPointIOMerger()
{
}

PCGExMT::FScope FPCGExPointIOMerger::Append(const TSharedPtr<PCGExData::FPointIO>& InData)
{
	const int32 NumPoints = InData->GetNum();

	if (NumPoints <= 0) { return PCGExMT::FScope(); }

	const int32 Start = NumCompositePoints;
	IOSources.Add(InData);
	NumCompositePoints += NumPoints;

	return Scopes.Add_GetRef(PCGExMT::FScope(Start, NumPoints));
}

void FPCGExPointIOMerger::Append(const TArray<TSharedPtr<PCGExData::FPointIO>>& InData)
{
	for (const TSharedPtr<PCGExData::FPointIO>& PointIO : InData) { Append(PointIO); }
}

void FPCGExPointIOMerger::Append(const TSharedRef<PCGExData::FPointIOCollection>& InCollection)
{
	for (const TSharedPtr<PCGExData::FPointIO>& PointIO : InCollection->Pairs) { Append(PointIO); }
}

void FPCGExPointIOMerger::MergeAsync(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const FPCGExCarryOverDetails* InCarryOverDetails, const TSet<FName>* InIgnoredAttributes)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPointIOMerger::MergeAsync);
	
	UPCGBasePointData* OutPointData = UnionDataFacade->GetOut();
	PCGEx::SetNumPointsAllocated(OutPointData, NumCompositePoints);

	// TODO : We could not copy metadata if there's no attributes on any of the input data

	OutPointData->SetMetadataEntry(PCGInvalidEntryKey);
	
	InCarryOverDetails->Prune(&UnionDataFacade->Source.Get());

	TMap<FName, int32> ExpectedTypes;

	const int32 NumSources = IOSources.Num();

	for (int i = 0; i < NumSources; i++)
	{

		const PCGExMT::FScope SourceScope = Scopes[i]; 
		
		const TSharedPtr<PCGExData::FPointIO> Source = IOSources[i];
		UnionDataFacade->Source->Tags->Append(Source->Tags.ToSharedRef());

		Source->GetIn()->CopyPropertiesTo(OutPointData, 0, SourceScope.Start, SourceScope.Count, PCGEx::AllPointNativePropertiesButMeta);
		
		// Discover attributes
		UPCGMetadata* Metadata = Source->GetIn()->Metadata;
		PCGEx::FAttributeIdentity::ForEach(
			Metadata, [&](const PCGEx::FAttributeIdentity& SourceIdentity, const int32)
			{
				if (InIgnoredAttributes && InIgnoredAttributes->Contains(SourceIdentity.Name)) { return; }

				FString StrName = SourceIdentity.Name.ToString();
				if (!InCarryOverDetails->Attributes.Test(StrName)) { return; }

				// TODO : Get attributes in the task

				const int32* ExpectedType = ExpectedTypes.Find(SourceIdentity.Name);
				if (!ExpectedType)
				{
					// No type expectations, we need to register a new attribute ref
					PCGExPointIOMerger::FIdentityRef SourceRef = SourceIdentity;
					SourceRef.Attribute = Metadata->GetConstAttribute(SourceIdentity.Name);
					SourceRef.bInitDefault = InCarryOverDetails->bPreserveAttributesDefaultValue;

					ExpectedTypes.Add(SourceRef.Name, UniqueIdentities.Add(SourceRef));

					return;
				}

				// Notify type/name mismatch if needed
				if (UniqueIdentities[*ExpectedType].UnderlyingType != SourceIdentity.UnderlyingType)
				{
					PCGE_LOG_C(Warning, GraphAndLog, AsyncManager->GetContext(), FText::Format(FTEXT("Mismatching attribute types for: {0}."), FText::FromName(SourceIdentity.Name)));
				}
			});
	}

	InCarryOverDetails->Prune(&UnionDataFacade->Source.Get());

	PCGEX_SHARED_THIS_DECL
	for (int i = 0; i < UniqueIdentities.Num(); i++)
	{
		PCGEX_LAUNCH(PCGExPointIOMerger::FCopyAttributeTask, i, ThisPtr)
	}
}

namespace PCGExPointIOMerger
{
	FCopyAttributeTask::FCopyAttributeTask(const int32 InTaskIndex, const TSharedPtr<FPCGExPointIOMerger>& InMerger)
		: FPCGExIndexedTask(InTaskIndex),
		  Merger(InMerger)
	{
	}

	void FCopyAttributeTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		const FIdentityRef& Identity = Merger->UniqueIdentities[TaskIndex];

		PCGEx::ExecuteWithRightType(
			Identity.UnderlyingType, [&](auto DummyValue)
			{
				using T = decltype(DummyValue);

				TSharedPtr<PCGExData::TBuffer<T>> Buffer;

				if (Identity.bInitDefault)
				{
					Buffer = Merger->UnionDataFacade->GetWritable(static_cast<const FPCGMetadataAttribute<T>*>(Identity.Attribute), PCGExData::EBufferInit::New);
				}

				if (!Buffer)
				{
					Buffer = Merger->UnionDataFacade->GetWritable(Identity.Name, T{}, Identity.bAllowsInterpolation, PCGExData::EBufferInit::New);
				}

				for (int i = 0; i < Merger->IOSources.Num(); i++)
				{
					TSharedPtr<PCGExData::FPointIO> SourceIO = Merger->IOSources[i];
					const FPCGMetadataAttributeBase* Attribute = SourceIO->GetIn()->Metadata->GetConstAttribute(Identity.Name);

					if (!Attribute) { continue; }                            // Missing attribute
					if (!Identity.IsA(Attribute->GetTypeId())) { continue; } // Type mismatch

					PCGEX_LAUNCH_INTERNAL(FWriteAttributeScopeTask<T>, SourceIO, Merger->Scopes[i], Identity, Buffer->GetOutValues())
				}
			});
	}
}
