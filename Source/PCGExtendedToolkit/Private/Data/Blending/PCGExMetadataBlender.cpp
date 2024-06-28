// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/Blending/PCGExMetadataBlender.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExData.h"
#include "Data/Blending/PCGExDataBlendingOperations.h"
#include "Data/Blending/PCGExDataBlending.h"

namespace PCGExDataBlending
{
	FMetadataBlender::~FMetadataBlender()
	{
		Cleanup();
	}

	FMetadataBlender::FMetadataBlender(const FPCGExBlendingSettings* InBlendingSettings)
	{
		BlendingSettings = InBlendingSettings;
	}

	FMetadataBlender::FMetadataBlender(const FMetadataBlender* ReferenceBlender)
	{
		BlendingSettings = ReferenceBlender->BlendingSettings;
	}

	void FMetadataBlender::PrepareForData(
		PCGExData::FFacade* InPrimaryData,
		const PCGExData::ESource SecondarySource,
		const bool bInitFirstOperation,
		const TSet<FName>* IgnoreAttributeSet)
	{
		InternalPrepareForData(InPrimaryData, InPrimaryData, SecondarySource, bInitFirstOperation, IgnoreAttributeSet);
	}

	void FMetadataBlender::PrepareForData(
		PCGExData::FFacade* InPrimaryData,
		PCGExData::FFacade* InSecondaryData,
		const PCGExData::ESource SecondarySource,
		const bool bInitFirstOperation,
		const TSet<FName>* IgnoreAttributeSet)
	{
		InternalPrepareForData(InPrimaryData, InSecondaryData, SecondarySource, bInitFirstOperation, IgnoreAttributeSet);
	}

	void FMetadataBlender::PrepareForBlending(const PCGEx::FPointRef& Target, const FPCGPoint* Defaults) const
	{
		for (const FDataBlendingOperationBase* Op : OperationsToBePrepared) { Op->PrepareOperation(Target.Index); }

		if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }

		PropertiesBlender->PrepareBlending(Target.MutablePoint(), Defaults ? *Defaults : *Target.Point);
	}

	void FMetadataBlender::PrepareForBlending(const int32 PrimaryIndex, const FPCGPoint* Defaults) const
	{
		for (const FDataBlendingOperationBase* Op : OperationsToBePrepared) { Op->PrepareOperation(PrimaryIndex); }

		if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }

		PropertiesBlender->PrepareBlending((*PrimaryPoints)[PrimaryIndex], Defaults ? *Defaults : (*PrimaryPoints)[PrimaryIndex]);
	}

	void FMetadataBlender::Blend(
		const PCGEx::FPointRef& A,
		const PCGEx::FPointRef& B,
		const PCGEx::FPointRef& Target,
		const double Weight)
	{
		const bool IsFirstOperation = FirstPointOperation[A.Index];
		for (const FDataBlendingOperationBase* Op : Operations) { Op->DoOperation(A.Index, B.Index, Target.Index, Weight, IsFirstOperation); }
		FirstPointOperation[A.Index] = false;

		if (bSkipProperties) { return; }

		PropertiesBlender->Blend(*A.Point, *B.Point, Target.MutablePoint(), Weight);
	}

	void FMetadataBlender::Blend(
		const int32 PrimaryIndex,
		const int32 SecondaryIndex,
		const int32 TargetIndex,
		const double Weight)
	{
		const bool IsFirstOperation = FirstPointOperation[PrimaryIndex];
		for (const FDataBlendingOperationBase* Op : Operations) { Op->DoOperation(PrimaryIndex, SecondaryIndex, TargetIndex, Weight, IsFirstOperation); }
		FirstPointOperation[PrimaryIndex] = false;

		if (bSkipProperties) { return; }

		PropertiesBlender->Blend((*PrimaryPoints)[PrimaryIndex], (*SecondaryPoints)[SecondaryIndex], (*PrimaryPoints)[TargetIndex], Weight);
	}

	void FMetadataBlender::CompleteBlending(
		const PCGEx::FPointRef& Target,
		const int32 Count,
		const double TotalWeight) const
	{
		for (const FDataBlendingOperationBase* Op : OperationsToBeCompleted) { Op->FinalizeOperation(Target.Index, Count, TotalWeight); }

		if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }

		PropertiesBlender->CompleteBlending(Target.MutablePoint(), Count, TotalWeight);
	}

	void FMetadataBlender::CompleteBlending(const int32 PrimaryIndex, const int32 Count, const double TotalWeight) const
	{
		for (const FDataBlendingOperationBase* Op : OperationsToBeCompleted) { Op->FinalizeOperation(PrimaryIndex, Count, TotalWeight); }

		if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }

		PropertiesBlender->CompleteBlending((*PrimaryPoints)[PrimaryIndex], Count, TotalWeight);
	}

	void FMetadataBlender::PrepareRangeForBlending(
		const int32 StartIndex,
		const int32 Range) const
	{
		for (const FDataBlendingOperationBase* Op : OperationsToBePrepared) { Op->PrepareRangeOperation(StartIndex, Range); }

		if (bSkipProperties) { return; }

		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Range);
		PropertiesBlender->PrepareRangeBlending(View, (*PrimaryPoints)[StartIndex]);
	}

	void FMetadataBlender::BlendRange(
		const PCGEx::FPointRef& A,
		const PCGEx::FPointRef& B,
		const int32 StartIndex,
		const int32 Range,
		const TArrayView<double>& Weights)
	{
		const int32 PrimaryIndex = A.Index;
		const int32 SecondaryIndex = B.Index;

		const bool IsFirstOperation = FirstPointOperation[PrimaryIndex];
		for (const FDataBlendingOperationBase* Op : Operations) { Op->DoRangeOperation(PrimaryIndex, SecondaryIndex, StartIndex, Weights, IsFirstOperation); }
		FirstPointOperation[PrimaryIndex] = false;

		if (bSkipProperties) { return; }

		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Range);
		PropertiesBlender->BlendRange(*A.Point, *B.Point, View, Weights);
	}

	void FMetadataBlender::CompleteRangeBlending(
		const int32 StartIndex,
		const int32 Range,
		const TArrayView<const int32>& Counts,
		const TArrayView<double>& TotalWeights) const
	{
		for (const FDataBlendingOperationBase* Op : OperationsToBePrepared) { Op->FinalizeRangeOperation(StartIndex, Counts, TotalWeights); }

		if (bSkipProperties) { return; }

		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Range);
		PropertiesBlender->CompleteRangeBlending(View, Counts, TotalWeights);
	}

	void FMetadataBlender::BlendRangeFromTo(
		const PCGEx::FPointRef& From,
		const PCGEx::FPointRef& To,
		const int32 StartIndex,
		const TArrayView<double>& Weights)
	{
		TArray<int32> Counts;
		Counts.SetNumUninitialized(Weights.Num());
		for (int32& C : Counts) { C = 2; }

		const int32 Range = Weights.Num();
		const int32 PrimaryIndex = From.Index;
		const int32 SecondaryIndex = To.Index;

		const bool IsFirstOperation = FirstPointOperation[PrimaryIndex];

		for (const FDataBlendingOperationBase* Op : OperationsToBePrepared)
		{
			Op->PrepareRangeOperation(StartIndex, Range);
			Op->DoRangeOperation(PrimaryIndex, SecondaryIndex, StartIndex, Weights, IsFirstOperation);
			Op->FinalizeRangeOperation(StartIndex, Counts, Weights);
		}

		FirstPointOperation[PrimaryIndex] = false;

		if (bSkipProperties) { return; }

		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Range);
		PropertiesBlender->BlendRangeFromTo(*From.Point, *To.Point, View, Weights);
	}

	void FMetadataBlender::Cleanup()
	{
		FirstPointOperation.Empty();
		OperationIdMap.Empty();

		PCGEX_DELETE(PropertiesBlender)

		PCGEX_DELETE_TARRAY(Operations)

		OperationsToBePrepared.Empty();
		OperationsToBeCompleted.Empty();

		PrimaryPoints = nullptr;
		SecondaryPoints = nullptr;
	}

	void FMetadataBlender::InternalPrepareForData(
		PCGExData::FFacade* InPrimaryData,
		PCGExData::FFacade* InSecondaryData,
		const PCGExData::ESource SecondarySource,
		const bool bInitFirstOperation,
		const TSet<FName>* IgnoreAttributeSet)
	{
		Cleanup();

		bSkipProperties = !bBlendProperties;
		if (!bSkipProperties)
		{
			PropertiesBlender = new FPropertiesBlender(BlendingSettings->GetPropertiesBlendingSettings());
			if (PropertiesBlender->bHasNoBlending)
			{
				bSkipProperties = true;
				PCGEX_DELETE(PropertiesBlender)
			}
		}

		InPrimaryData->Source->CreateOutKeys();
		InSecondaryData->Source->CreateKeys(SecondarySource);

		PrimaryPoints = &InPrimaryData->Source->GetOut()->GetMutablePoints();
		SecondaryPoints = const_cast<TArray<FPCGPoint>*>(&InSecondaryData->Source->GetData(SecondarySource)->GetPoints());

		TArray<PCGEx::FAttributeIdentity> Identities;
		PCGEx::FAttributeIdentity::Get(InPrimaryData->Source->GetOut()->Metadata, Identities);
		BlendingSettings->Filter(Identities);

		if constexpr (&InSecondaryData != &InPrimaryData)
		{
			TArray<FName> PrimaryNames;
			TArray<FName> SecondaryNames;
			TMap<FName, PCGEx::FAttributeIdentity> PrimaryIdentityMap;
			TMap<FName, PCGEx::FAttributeIdentity> SecondaryIdentityMap;
			PCGEx::FAttributeIdentity::Get(InPrimaryData->Source->GetOut()->Metadata, PrimaryNames, PrimaryIdentityMap);
			PCGEx::FAttributeIdentity::Get(InSecondaryData->Source->GetData(SecondarySource)->Metadata, SecondaryNames, SecondaryIdentityMap);

			for (FName PrimaryName : PrimaryNames)
			{
				const PCGEx::FAttributeIdentity& PrimaryIdentity = *PrimaryIdentityMap.Find(PrimaryName);
				if (!SecondaryIdentityMap.Find(PrimaryName))
				{
					//An attribute exists on Primary but not on Secondary -- Simply ignore it.
					Identities.Remove(PrimaryIdentity);
				}
			}

			for (FName SecondaryName : SecondaryNames)
			{
				const PCGEx::FAttributeIdentity& SecondaryIdentity = *SecondaryIdentityMap.Find(SecondaryName);
				if (const PCGEx::FAttributeIdentity* PrimaryIdentityPtr = PrimaryIdentityMap.Find(SecondaryName))
				{
					if (PrimaryIdentityPtr->UnderlyingType != SecondaryIdentity.UnderlyingType)
					{
						// Type mismatch -- Simply ignore it
						Identities.Remove(*PrimaryIdentityPtr);
					}
				}
				else if (BlendingSettings->CanBlend(SecondaryIdentity.Name))
				{
					//Operation will handle missing attribute creation.
					Identities.Add(SecondaryIdentity);
				}
			}
		}

		Operations.Empty(Identities.Num());
		OperationsToBeCompleted.Empty(Identities.Num());
		OperationsToBePrepared.Empty(Identities.Num());

		for (const PCGEx::FAttributeIdentity& Identity : Identities)
		{
			if (IgnoreAttributeSet && IgnoreAttributeSet->Contains(Identity.Name)) { continue; }

			const EPCGExDataBlendingType* TypePtr = BlendingSettings->AttributesOverrides.Find(Identity.Name);
			FDataBlendingOperationBase* Op = CreateOperation(TypePtr ? *TypePtr : BlendingSettings->DefaultBlending, Identity);

			if (!Op) { continue; }

			OperationIdMap.Add(Identity.Name, Op);

			Operations.Add(Op);
			if (Op->GetRequiresPreparation()) { OperationsToBePrepared.Add(Op); }
			if (Op->GetRequiresFinalization()) { OperationsToBeCompleted.Add(Op); }

			Op->PrepareForData(InPrimaryData, InSecondaryData, SecondarySource);
		}

		FirstPointOperation.SetNum(PrimaryPoints->Num());

		if (bInitFirstOperation) { for (bool& bFirstOp : FirstPointOperation) { bFirstOp = true; } }
		else { for (bool& bFirstOp : FirstPointOperation) { bFirstOp = false; } }
	}
}
