// Fill out your copyright notice in the Description page of Project Settings.


// Cherry picker merges metadata from varied sources into one.
// Initially to handle metadata merging for Fuse Clusters

#include "Data/PCGExPointIOCherryPicker.h"

FPCGExPointIOCherryPicker::FPCGExPointIOCherryPicker(PCGExData::FPointIO& OutData)
{
	MergedData = &OutData;
	MergedPoints.Empty();
	Identities.Empty();
	AllowsInterpolation.Empty();
}

FPCGExPointIOCherryPicker::~FPCGExPointIOCherryPicker()
{
	for (const PCGEx::FAAttributeIO* Writer : WriterList) { delete Writer; }
	Writers.Empty();

	Identities.Empty();
	AllowsInterpolation.Empty();

	if (bCleanupInputs) { for (PCGExData::FPointIO* PointIO : MergedPoints) { PointIO->Cleanup(); } }
	MergedPoints.Empty();

	MergedData->Cleanup();
	MergedData = nullptr;
}

void FPCGExPointIOCherryPicker::Append(PCGExData::FPointIO& InData)
{
	MergedPoints.Add(&InData);

	TArray<PCGEx::FAttributeIdentity> NewIdentities;
	PCGEx::FAttributeIdentity::Get(InData.GetIn(), NewIdentities);
	for (PCGEx::FAttributeIdentity& NewIdentity : NewIdentities)
	{
		if (Identities.Find(NewIdentity.Name)) { continue; } //TODO: Will likely create issues if there is an attribute mismatch, resolve during merge.
		Identities.Add(NewIdentity.Name, NewIdentity);
		AllowsInterpolation.Add(NewIdentity.Name, InData.GetIn()->Metadata->GetConstAttribute(NewIdentity.Name)->AllowsInterpolation());
	}

	TotalPoints += InData.GetNum();

	InData.CreateInKeys();
}

void FPCGExPointIOCherryPicker::Append(const TArray<PCGExData::FPointIO*>& InData)
{
	for (const PCGExData::FPointIO* PointIO : InData) { Append(const_cast<PCGExData::FPointIO&>(*PointIO)); }
}

void FPCGExPointIOCherryPicker::Merge(FPCGExAsyncManager* AsyncManager, const bool CleanupInputs)
{
	bCleanupInputs = CleanupInputs;

	TArray<FPCGPoint>& MutablePoints = MergedData->GetOut()->GetMutablePoints();
	MutablePoints.SetNum(TotalPoints);

	int32 StartIndex = 0;

	for (const PCGExData::FPointIO* PointIO : MergedPoints)
	{
		const int32 NumPoints = PointIO->GetNum();
		for (int i = 0; i < NumPoints; i++)
		{
			FPCGPoint& Point = MutablePoints[StartIndex + i] = PointIO->GetInPoint(i);
			Point.MetadataEntry = PCGInvalidEntryKey;
		}

		StartIndex += NumPoints;
	}

	MergedData->CreateOutKeys();

	// Prepare writers for each attribute on the merged
	FPCGExPointIOCherryPicker* Merger = this;

	for (const TPair<FName, PCGEx::FAttributeIdentity>& Identity : Identities)
	{
		PCGMetadataAttribute::CallbackWithRightType(
			static_cast<uint16>(Identity.Value.UnderlyingType), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				PCGEx::TFAttributeWriter<T>* Writer = new PCGEx::TFAttributeWriter<T>(Identity.Key, T{}, *AllowsInterpolation.Find(Identity.Key));
				Writer->BindAndGet(*MergedData);

				Writers.Add(Identity.Key, Writer);
				WriterList.Add(Writer);

				int32 PointIndex = 0;
				for (PCGExData::FPointIO* PointIO : MergedPoints) { AsyncManager->Start<FPCGExAttributeCherryPickTask>(PointIndex++, PointIO, Merger, Identity.Key); }
			});
	}
}

void FPCGExPointIOCherryPicker::Write()
{
	for (const TPair<FName, PCGEx::FAAttributeIO*>& Pair : Writers)
	{
		PCGMetadataAttribute::CallbackWithRightType(
			static_cast<uint16>(Identities.Find(Pair.Key)->UnderlyingType), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				static_cast<PCGEx::TFAttributeWriter<T>*>(Pair.Value)->Write();
			});
	}
}

bool FPCGExAttributeCherryPickTask::ExecuteTask()
{
	const int32 NumPoints = PointIO->GetNum();

	int32 StartIndex = 0;
	for (int i = 0; i < TaskIndex; i++) { StartIndex += Merger->MergedPoints[i]->GetNum(); }

	PCGMetadataAttribute::CallbackWithRightType(
		static_cast<uint16>(Merger->Identities.Find(AttributeName)->UnderlyingType), [&](auto DummyValue)
		{
			using T = decltype(DummyValue);

			PCGEx::FAAttributeIO** WriterPtr = Merger->Writers.Find(AttributeName);
			if (!WriterPtr) { return; }

			PCGEx::TFAttributeReader<T>* Reader = new PCGEx::TFAttributeReader<T>(AttributeName);
			if (!Reader->Bind(*PointIO))
			{
				PCGEX_DELETE(Reader)
				return;
			}

			PCGEx::TFAttributeWriter<T>* Writer = static_cast<PCGEx::TFAttributeWriter<T>*>(*WriterPtr);

			for (int i = 0; i < NumPoints; i++) { Writer->Values[StartIndex + i] = Reader->Values[i]; }

			PCGEX_DELETE(Reader)
		});

	return true;
}
