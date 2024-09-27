// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/PCGExPointIOMerger.h"

#include "PCGExGlobalSettings.h"
#include "Data/PCGExDataFilter.h"


FPCGExPointIOMerger::FPCGExPointIOMerger(TSharedPtr<PCGExData::FPointIO> OutMergedData)
{
	CompositeIO = OutMergedData;
}

FPCGExPointIOMerger::~FPCGExPointIOMerger()
{
}

void FPCGExPointIOMerger::Append(const TSharedPtr<PCGExData::FPointIO>& InData)
{
	const int32 NumPoints = InData->GetNum();

	if (NumPoints <= 0) { return; }

	IOSources.Add(InData);
	Scopes.Add(PCGEx::H64(NumCompositePoints, NumPoints));
	NumCompositePoints += NumPoints;
}

void FPCGExPointIOMerger::Append(const TArray<TSharedPtr<PCGExData::FPointIO>>& InData)
{
	for (const TSharedPtr<PCGExData::FPointIO>& PointIO : InData) { Append(PointIO); }
}

void FPCGExPointIOMerger::Append(PCGExData::FPointIOCollection* InCollection)
{
	for (const TSharedPtr<PCGExData::FPointIO> PointIO : InCollection->Pairs) { Append(PointIO); }
}

void FPCGExPointIOMerger::Merge(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const FPCGExCarryOverDetails* InCarryOverDetails)
{
	CompositeIO->InitializeNum(NumCompositePoints);
	TArray<FPCGPoint>& MutablePoints = CompositeIO->GetOut()->GetMutablePoints();
	InCarryOverDetails->Filter(CompositeIO.Get());

	TMap<FName, EPCGMetadataTypes> ExpectedTypes;

	const int32 NumSources = IOSources.Num();

	for (int i = 0; i < NumSources; ++i)
	{
		const TSharedPtr<PCGExData::FPointIO> Source = IOSources[i];
		CompositeIO->Tags->Append(Source->Tags.ToSharedRef());
		Source->CreateInKeys();

		const TArray<FPCGPoint>& SourcePoints = Source->GetIn()->GetPoints();

		const uint32 StartIndex = PCGEx::H64A(Scopes[i]);

		// Copy source points -- TODO : could be made async if we split in two steps (merge points then merge attributes)
		for (int j = 0; j < SourcePoints.Num(); ++j)
		{
			const int32 TargetIndex = StartIndex + j;
			const PCGMetadataEntryKey Key = MutablePoints[TargetIndex].MetadataEntry;
			MutablePoints[TargetIndex] = SourcePoints[j];
			MutablePoints[TargetIndex].MetadataEntry = Key;
		}

		// Discover attributes
		UPCGMetadata* Metadata = Source->GetIn()->Metadata;
		TArray<PCGEx::FAttributeIdentity> SourceAttributes;
		PCGEx::FAttributeIdentity::Get(Metadata, SourceAttributes);
		for (PCGEx::FAttributeIdentity SourceAtt : SourceAttributes)
		{
			FString StrName = SourceAtt.Name.ToString();
			if (!InCarryOverDetails->Attributes.Test(StrName)) { continue; }

			const EPCGMetadataTypes* ExpectedType = ExpectedTypes.Find(SourceAtt.Name);
			if (!ExpectedType)
			{
				ExpectedTypes.Add(SourceAtt.Name, SourceAtt.UnderlyingType);

				PCGMetadataAttribute::CallbackWithRightType(
					static_cast<uint16>(SourceAtt.UnderlyingType), [&](auto DummyValue)
					{
						using T = decltype(DummyValue);
						TSharedPtr<PCGEx::TAttributeWriter<T>> Writer;

						if (InCarryOverDetails->bPreserveAttributesDefaultValue)
						{
							const FPCGMetadataAttribute<T>* SourceAttribute = Metadata->GetConstTypedAttribute<T>(SourceAtt.Name);
							Writer = MakeShared<PCGEx::TAttributeWriter<T>>(SourceAtt.Name, SourceAttribute->GetValue(PCGDefaultValueKey), SourceAtt.bAllowsInterpolation);
						}

						if (!Writer) { Writer = MakeShared<PCGEx::TAttributeWriter<T>>(SourceAtt.Name, T{}, SourceAtt.bAllowsInterpolation); }
						Writers.Add(StaticCastSharedPtr<PCGEx::FAttributeIOBase>(Writer));
						UniqueIdentities.Add(SourceAtt);
					});

				continue;
			}

			if (*ExpectedType != SourceAtt.UnderlyingType)
			{
				// Type mismatch; TODO Log warning
			}
		}
	}

	InCarryOverDetails->Filter(CompositeIO.Get());
	CompositeIO->CreateOutKeys();

	for (int i = 0; i < UniqueIdentities.Num(); ++i) { AsyncManager->Start<PCGExPointIOMerger::FWriteAttributeTask>(i, CompositeIO, SharedThis(this)); }
}

void FPCGExPointIOMerger::Write()
{
	for (int i = 0; i < UniqueIdentities.Num(); ++i)
	{
		PCGMetadataAttribute::CallbackWithRightType(
			UniqueIdentities[i].GetTypeId(), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				TSharedPtr<PCGEx::TAttributeWriter<T>> Writer = StaticCastSharedPtr<PCGEx::TAttributeWriter<T>>(Writers[i]);
				Writer->Write();
			});
	}

	Writers.Empty();
}

void FPCGExPointIOMerger::Write(TSharedPtr<PCGExMT::FTaskManager> AsyncManager)
{
	for (int i = 0; i < UniqueIdentities.Num(); ++i)
	{
		PCGMetadataAttribute::CallbackWithRightType(
			UniqueIdentities[i].GetTypeId(), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				TSharedPtr<PCGEx::TAttributeWriter<T>> Writer = StaticCastSharedPtr<PCGEx::TAttributeWriter<T>>(Writers[i]);
				PCGExMT::Write(AsyncManager, Writer);
			});
	}

	Writers.Empty();
}

namespace PCGExPointIOMerger
{
	bool FWriteAttributeTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		const PCGEx::FAttributeIdentity& Identity = Merger->UniqueIdentities[TaskIndex];
		TSharedPtr<PCGEx::FAttributeIOBase> Writer = Merger->Writers[TaskIndex];

		PCGMetadataAttribute::CallbackWithRightType(
			Identity.GetTypeId(), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				TSharedPtr<PCGEx::TAttributeWriter<T>> TypedWriter = StaticCastSharedPtr<PCGEx::TAttributeWriter<T>>(Writer);
				TypedWriter->BindAndSetNumUninitialized(PointIO);

				for (int i = 0; i < Merger->IOSources.Num(); ++i)
				{
					TSharedPtr<PCGExData::FPointIO> SourceIO = Merger->IOSources[i];
					const FPCGMetadataAttributeBase* Attribute = SourceIO->GetIn()->Metadata->GetConstAttribute(Identity.Name);

					if (!Attribute) { continue; }                            // Missing attribute
					if (!Identity.IsA(Attribute->GetTypeId())) { continue; } // Type mismatch

					if (SourceIO->GetNum() < GetDefault<UPCGExGlobalSettings>()->SmallPointsSize)
					{
						ScopeMerge(Merger->Scopes[i], Identity, SourceIO, TypedWriter);
					}
					else
					{
						AsyncManager->Start<FWriteAttributeScopeTask<T>>(-1, SourceIO, Merger->Scopes[i], Identity, TypedWriter);
					}
				}
			});

		return true;
	}
}
