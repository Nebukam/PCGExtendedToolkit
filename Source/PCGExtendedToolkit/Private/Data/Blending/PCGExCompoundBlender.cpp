// Fill out your copyright notice in the Description page of Project Settings.


// Cherry picker merges metadata from varied sources into one.
// Initially to handle metadata merging for Fuse Clusters

#include "Data/Blending/PCGExCompoundBlender.h"

#include "Data/PCGExData.h"

namespace PCGExDataBlending
{
	FCompoundBlender::FCompoundBlender(FPCGExBlendingSettings* InBlendingSettings)
	{
		BlendingSettings = InBlendingSettings;
		Sources.Empty();
		UniqueIdentitiesMap.Empty();
	}

	FCompoundBlender::~FCompoundBlender()
	{
		Cleanup();

		UniqueIdentitiesMap.Empty();
		UniqueIdentitiesList.Empty();
		AllowsInterpolation.Empty();

		for (TMap<FName, FPCGMetadataAttributeBase*>* List : PerSourceAttMap)
		{
			List->Empty();
			delete List;
		}

		Sources.Empty();
		PerSourceAttMap.Empty();

		for (TMap<FName, FDataBlendingOperationBase*>* List : PerSourceOpsMap)
		{
			PCGEX_DELETE_TMAP((*List), FName)
			delete List;
		}

		PerSourceOpsMap.Empty();
	}

	void FCompoundBlender::Cleanup()
	{
		PCGEX_DELETE_TARRAY(Writers)
		for (int i = 0; i < CachedOperations.Num(); i++) { CachedOperations[i].Empty(); }
		CachedOperations.Empty();
	}

	void FCompoundBlender::AddSource(PCGExData::FPointIO& InData)
	{
		Sources.Add(&InData);

		TArray<PCGEx::FAttributeIdentity> TempSrcIdentities;
		PCGEx::FAttributeIdentity::Get(InData.GetIn()->Metadata, TempSrcIdentities);
		BlendingSettings->Filter(TempSrcIdentities);

		TMap<FName, FPCGMetadataAttributeBase*>* SrcAttMap = new TMap<FName, FPCGMetadataAttributeBase*>();
		PerSourceAttMap.Add(SrcAttMap);

		TMap<FName, FDataBlendingOperationBase*>* SrcOpsMap = new TMap<FName, FDataBlendingOperationBase*>();
		PerSourceOpsMap.Add(SrcOpsMap);

		UPCGMetadata* SourceMetadata = InData.GetIn()->Metadata;

		for (PCGEx::FAttributeIdentity& Identity : TempSrcIdentities)
		{
			FPCGMetadataAttributeBase* Attribute = SourceMetadata->GetMutableAttribute(Identity.Name);
			if (!Attribute) { continue; }

			if (const PCGEx::FAttributeIdentity* ExistingIdentity = UniqueIdentitiesMap.Find(Identity.Name))
			{
				if (Identity.UnderlyingType != ExistingIdentity->UnderlyingType)
				{
					// Type mismatch, ignore
					continue;
				}
			}
			else
			{
				UniqueIdentitiesMap.Add(Identity.Name, Identity);
				UniqueIdentitiesList.Add(Identity);
				AllowsInterpolation.Add(Identity.Name, InData.GetIn()->Metadata->GetConstAttribute(Identity.Name)->AllowsInterpolation());
			}

			SrcAttMap->Add(Identity.Name, Attribute);
			const EPCGExDataBlendingType* TypePtr = BlendingSettings->AttributesOverrides.Find(Identity.Name);
			SrcOpsMap->Add(Identity.Name, CreateOperation(TypePtr ? *TypePtr : BlendingSettings->DefaultBlending, Identity));
		}

		InData.CreateInKeys();

		TempSrcIdentities.Empty();
	}

	void FCompoundBlender::AddSources(const PCGExData::FPointIOGroup& InDataGroup)
	{
		for (PCGExData::FPointIO* IOPair : InDataGroup.Pairs) { AddSource(*IOPair); }
	}

	void FCompoundBlender::Merge(
		FPCGExAsyncManager* AsyncManager,
		PCGExData::FPointIO* TargetData,
		PCGExData::FIdxCompoundList* CompoundList,
		const FPCGExDistanceSettings& DistSettings)
	{
		Cleanup();

		TargetData->CreateOutKeys();

		// Create blending operations

		for (const PCGEx::FAttributeIdentity& Identity : UniqueIdentitiesList)
		{
			TArray<FDataBlendingOperationBase*>& Ops = CachedOperations.Emplace_GetRef();
			Ops.SetNumUninitialized(Sources.Num());

			int32 OpsCount = 0;
			PCGMetadataAttribute::CallbackWithRightType(
				static_cast<uint16>(Identity.UnderlyingType), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					PCGEx::TFAttributeWriter<T>* Writer = new PCGEx::TFAttributeWriter<T>(Identity.Name, T{}, *AllowsInterpolation.Find(Identity.Name));
					Writer->BindAndGet(*TargetData);

					Writers.Add(Writer);

					for (int i = 0; i < Sources.Num(); i++)
					{
						PCGExData::FPointIO* Source = Sources[i];
						TMap<FName, FDataBlendingOperationBase*>* OpsMap = PerSourceOpsMap[i];

						// Prepare each operation with this writer/source pair
						if (FDataBlendingOperationBase** OperationPtr = OpsMap->Find(Identity.Name))
						{
							(*OperationPtr)->PrepareForData(Writer, *Source);
							Ops[i] = *OperationPtr;
							OpsCount++;
						}
						else
						{
							Ops[i] = nullptr;
						}
					}

					if (OpsCount == 0)
					{
						Writers.Pop();
						PCGEX_DELETE(Writer)
					}
				});

			if (OpsCount == 0)
			{
				// No operation added for attribute, remove it from the cache
				CachedOperations.Pop().Empty();
			}
		}

		FCompoundBlender* Merger = this;
		AsyncManager->Start<FPCGExCompoundBlendTask>(-1, TargetData, Merger, CompoundList, DistSettings);
	}

	void FCompoundBlender::Write()
	{
		for (int i = 0; i < UniqueIdentitiesList.Num(); i++)
		{
			const PCGEx::FAttributeIdentity& Identity = UniqueIdentitiesList[i];

			PCGMetadataAttribute::CallbackWithRightType(
				static_cast<uint16>(Identity.UnderlyingType), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					static_cast<PCGEx::TFAttributeWriter<T>*>(Writers[i])->Write();
					delete Writers[i];
				});
		}
		Writers.Empty();
	}

	bool FPCGExCompoundBlendTask::ExecuteTask()
	{
		for (int i = 0; i < CompoundList->Compounds.Num(); i++)
		{
			Manager->Start<FPCGExCompoundedPointBlendTask>(i, PointIO, Merger, CompoundList, DistSettings);
		}
		return true;
	}

	bool FPCGExCompoundedPointBlendTask::ExecuteTask()
	{
		PCGExData::FIdxCompound& Compound = *(*CompoundList)[TaskIndex];
		Compound.ComputeWeights(Merger->Sources, PointIO->GetOutPoint(TaskIndex), DistSettings);

		// Merge
		const int32 NumPoints = Compound.Num();

		//For each attribute, for each source, for each compounded point
		for (int i = 0; i < Merger->UniqueIdentitiesList.Num(); i++)
		{
			for (int j = 0; j < Merger->PerSourceOpsMap.Num(); j++)
			{
				const FDataBlendingOperationBase* Operation = Merger->CachedOperations[i][j];
				if (!Operation) { continue; } //Unsupported blend

				Operation->PrepareOperation(TaskIndex);

				for (int k = 0; k < NumPoints; k++)
				{
					uint32 IOIndex;
					uint32 PtIndex;
					PCGEx::H64(Compound[k], IOIndex, PtIndex);
					Operation->DoOperation(
						TaskIndex, Merger->Sources[IOIndex]->GetInPoint(PtIndex),
						TaskIndex, Compound.Weights[k]);
				}

				Operation->FinalizeOperation(TaskIndex, NumPoints);
			}
		}

		return true;
	}
}
