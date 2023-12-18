// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/PCGExPointIOMerger.h"

FPCGExPointIOMerger::FPCGExPointIOMerger(PCGExData::FPointIO& OutData)
{
	MergedData = &OutData;
	MergedPoints.Empty();
	Identities.Empty();
	AllowsInterpolation.Empty();
}

FPCGExPointIOMerger::~FPCGExPointIOMerger()
{
	MergedData = nullptr;
	MergedPoints.Empty();
	Identities.Empty();
	AllowsInterpolation.Empty();
}

void FPCGExPointIOMerger::Append(PCGExData::FPointIO& InData)
{
	MergedPoints.Add(&InData);
	TArray<PCGEx::FAttributeIdentity> NewIdentities;
	PCGEx::FAttributeIdentity::Get(InData.GetIn(), NewIdentities);
	for (PCGEx::FAttributeIdentity& NewIdentity : NewIdentities)
	{
		if (PCGEx::FAttributeIdentity* ExistingIdentity = Identities.Find(NewIdentity.Name)) { continue; } //TODO: Will likely create issues if there is an attribute missmatch, resolve during merge.
		Identities.Add(NewIdentity.Name, NewIdentity);
		AllowsInterpolation.Add(NewIdentity.Name, InData.GetIn()->Metadata->GetConstAttribute(NewIdentity.Name)->AllowsInterpolation());
	}
	TotalPoints += InData.GetNum();

	InData.CreateInKeys();
}

void FPCGExPointIOMerger::Append(const TArray<PCGExData::FPointIO*>& InData)
{
	for (const PCGExData::FPointIO* PointIO : InData) { Append(const_cast<PCGExData::FPointIO&>(*PointIO)); }
}

void FPCGExPointIOMerger::DoMerge()
{
	TArray<FPCGPoint>& MutablePoints = MergedData->GetOut()->GetMutablePoints();
	MutablePoints.SetNum(TotalPoints);

	MergedData->CreateOutKeys();

	int32 StartIndex = 0;
	for (const PCGExData::FPointIO* PointIO : MergedPoints)
	{
		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		for (int i = 0; i < InPoints.Num(); i++)
		{
			const int32 PtIndex = StartIndex + i;
			const PCGMetadataEntryKey ExistingKey = MutablePoints[PtIndex].MetadataEntry;
			MutablePoints[PtIndex] = InPoints[i];
			MutablePoints[PtIndex].MetadataEntry = ExistingKey;
		}
	}

	//TODO : Refactor this
	for (const TPair<FName, PCGEx::FAttributeIdentity>& Identity : Identities)
	{
		PCGMetadataAttribute::CallbackWithRightType(
			static_cast<uint16>(Identity.Value.UnderlyingType), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				PCGEx::TFAttributeWriter<T>* Writer = new PCGEx::TFAttributeWriter<T>(Identity.Key, T{}, *AllowsInterpolation.Find(Identity.Key));
				Writer->BindAndGet(*MergedData);

				StartIndex = 0;
				for (PCGExData::FPointIO* PointIO : MergedPoints)
				{
					const int32 NumPoints = PointIO->GetIn()->GetPoints().Num();

					PCGEx::TFAttributeReader<T>* Reader = new PCGEx::TFAttributeReader<T>(Identity.Key);
					if (Reader->Bind(*PointIO))
					{
						for (int i = 0; i < NumPoints; i++)
						{
							const int32 PtIndex = StartIndex + i;
							Writer->Values[PtIndex] = Reader->Values[i];
						}
					}

					PCGEX_DELETE(Reader)
					StartIndex += PointIO->GetNum();
				}

				Writer->Write();
				PCGEX_DELETE(Writer)
			});
	}

	for (PCGExData::FPointIO* PointIO : MergedPoints) { PointIO->Cleanup(); }
	MergedData->Cleanup();
}
