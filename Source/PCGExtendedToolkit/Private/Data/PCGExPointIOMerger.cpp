// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/PCGExPointIOMerger.h"

#include "PCGExGlobalSettings.h"
#include "Data/PCGExDataFilter.h"


FPCGExPointIOMerger::FPCGExPointIOMerger(const TSharedRef<PCGExData::FFacade>& InCompoundDataFacade):
	CompoundDataFacade(InCompoundDataFacade)
{
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
	for (const TSharedPtr<PCGExData::FPointIO>& PointIO : InCollection->Pairs) { Append(PointIO); }
}

void FPCGExPointIOMerger::Merge(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const FPCGExCarryOverDetails* InCarryOverDetails)
{
	CompoundDataFacade->Source->InitializeNum(NumCompositePoints);
	TArray<FPCGPoint>& MutablePoints = CompoundDataFacade->GetOut()->GetMutablePoints();
	InCarryOverDetails->Filter(&CompoundDataFacade->Source.Get());

	TMap<FName, EPCGMetadataTypes> ExpectedTypes;

	const int32 NumSources = IOSources.Num();

	for (int i = 0; i < NumSources; ++i)
	{
		const TSharedPtr<PCGExData::FPointIO> Source = IOSources[i];
		CompoundDataFacade->Source->Tags->Append(Source->Tags.ToSharedRef());

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
						TSharedPtr<PCGExData::TBuffer<T>> Buffer;

						if (InCarryOverDetails->bPreserveAttributesDefaultValue)
						{
							// 'template' spec required for clang on mac, not sure why.
							// ReSharper disable once CppRedundantTemplateKeyword
							const FPCGMetadataAttribute<T>* SourceAttribute = Metadata->template GetConstTypedAttribute<T>(SourceAtt.Name);
							Buffer = CompoundDataFacade->GetWritable(SourceAttribute, false);
						}

						if (!Buffer) { Buffer = CompoundDataFacade->GetWritable(SourceAtt.Name, T{}, SourceAtt.bAllowsInterpolation, false); }
						Buffers.Add(StaticCastSharedPtr<PCGExData::FBufferBase>(Buffer));
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

	InCarryOverDetails->Filter(&CompoundDataFacade->Source.Get());

	for (int i = 0; i < UniqueIdentities.Num(); ++i) { AsyncManager->Start<PCGExPointIOMerger::FCopyAttributeTask>(i, CompoundDataFacade->Source, SharedThis(this)); }
}

namespace PCGExPointIOMerger
{
	bool FCopyAttributeTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		const PCGEx::FAttributeIdentity& Identity = Merger->UniqueIdentities[TaskIndex];
		const TSharedPtr<PCGExData::FBufferBase> Buffer = Merger->Buffers[TaskIndex];

		PCGMetadataAttribute::CallbackWithRightType(
			Identity.GetTypeId(), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				TSharedPtr<PCGExData::TBuffer<T>> TypedBuffer = StaticCastSharedPtr<PCGExData::TBuffer<T>>(Buffer);

				for (int i = 0; i < Merger->IOSources.Num(); ++i)
				{
					TSharedPtr<PCGExData::FPointIO> SourceIO = Merger->IOSources[i];
					const FPCGMetadataAttributeBase* Attribute = SourceIO->GetIn()->Metadata->GetConstAttribute(Identity.Name);

					if (!Attribute) { continue; }                            // Missing attribute
					if (!Identity.IsA(Attribute->GetTypeId())) { continue; } // Type mismatch

					AsyncManager->Start<FWriteAttributeScopeTask<T>>(-1, SourceIO, Merger->Scopes[i], Identity, TypedBuffer->GetOutValues());
				}
			});

		return true;
	}
}
