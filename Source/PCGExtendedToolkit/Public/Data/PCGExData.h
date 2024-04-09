// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointIO.h"
#include "Blending/PCGExDataBlending.h"
#include "Blending/PCGExDataBlendingOperations.h"
#include "Data/PCGPointData.h"
#include "UObject/Object.h"

#include "PCGExData.generated.h"


/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExParamDataBase : public UPCGPointData
{
	GENERATED_BODY()

public:
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Param; }
};

namespace PCGExData
{
	constexpr PCGExMT::AsyncState State_MergingData = __COUNTER__;

#pragma region Compound

	struct PCGEXTENDEDTOOLKIT_API FIdxCompound
	{
		TArray<uint64> CompoundedPoints;
		TArray<double> Weights;

		FIdxCompound() { CompoundedPoints.Empty(); }

		~FIdxCompound()
		{
			CompoundedPoints.Empty();
			Weights.Empty();
		}

		bool ContainsIOIndex(int32 InIOIndex);
		void ComputeWeights(const TArray<FPointIO*>& Sources, const FPCGPoint& Target, const FPCGExDistanceSettings& DistSettings);
		void ComputeWeights(const TArray<FPCGPoint>& SourcePoints, const FPCGPoint& Target, const FPCGExDistanceSettings& DistSettings);

		uint64 Add(const int32 IOIndex, const int32 PointIndex);

		int32 Num() const { return CompoundedPoints.Num(); }
		uint64 operator[](const int32 Index) const { return this->CompoundedPoints[Index]; }
	};

	struct PCGEXTENDEDTOOLKIT_API FIdxCompoundList
	{
		TArray<FIdxCompound*> Compounds;

		FIdxCompoundList() { Compounds.Empty(); }
		~FIdxCompoundList() { PCGEX_DELETE_TARRAY(Compounds) }

		int32 Num() const { return Compounds.Num(); }

		FIdxCompound* New();

		uint64 Add(const int32 Index, const int32 IOIndex, const int32 PointIndex);
		void GetIOIndices(const int32 Index, TArray<int32>& OutIOIndices);
		bool HasIOIndexOverlap(int32 InIdx, const TArray<int32>& InIndices);

		FIdxCompound* operator[](const int32 Index) const { return this->Compounds[Index]; }
	};

#pragma endregion

#pragma region Data Marking

	template <typename T>
	static FPCGMetadataAttribute<T>* WriteMark(UPCGMetadata* Metadata, const FName MarkID, T MarkValue)
	{
		FPCGMetadataAttribute<T>* Mark = Metadata->FindOrCreateAttribute<T>(MarkID, MarkValue, false, true, true);
		Mark->ClearEntries();
		Mark->SetDefaultValue(MarkValue);
		return Mark;
	}

	template <typename T>
	static FPCGMetadataAttribute<T>* WriteMark(const FPointIO& PointIO, const FName MarkID, T MarkValue)
	{
		return WriteMark(PointIO.GetOut()->Metadata, MarkID, MarkValue);
	}


	template <typename T>
	static bool TryReadMark(UPCGMetadata* Metadata, const FName MarkID, T& OutMark)
	{
		const FPCGMetadataAttribute<T>* Mark = Metadata->GetConstTypedAttribute<T>(MarkID);
		if (!Mark) { return false; }
		OutMark = Mark->GetValue(PCGInvalidEntryKey);
		return true;
	}

	template <typename T>
	static bool TryReadMark(const FPointIO& PointIO, const FName MarkID, T& OutMark)
	{
		return TryReadMark(PointIO.GetIn() ? PointIO.GetIn()->Metadata : PointIO.GetOut()->Metadata, MarkID, OutMark);
	}

	static void WriteId(const FPointIO& PointIO, const FName IdName, const int64 Id)
	{
		FString OutId;
		PointIO.Tags->Set(IdName.ToString(), Id, OutId);
		if (PointIO.GetOut()) { WriteMark(PointIO.GetOut()->Metadata, IdName, Id); }
	}

	static UPCGPointData* GetMutablePointData(FPCGContext* Context, const FPCGTaggedData& Source)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Source.Data);
		if (!SpatialData) { return nullptr; }

		const UPCGPointData* PointData = SpatialData->ToPointData(Context);
		if (!PointData) { return nullptr; }

		return const_cast<UPCGPointData*>(PointData);
	}

#pragma endregion
}

namespace PCGExDataBlending
{
	static FDataBlendingOperationBase* CreateOperation(const EPCGExDataBlendingType Type, const PCGEx::FAttributeIdentity& Identity)
	{
#define PCGEX_SAO_NEW(_TYPE, _NAME, _ID) case EPCGMetadataTypes::_NAME : NewOperation = new FDataBlending##_ID<_TYPE>(); break;
#define PCGEX_BLEND_CASE(_ID) case EPCGExDataBlendingType::_ID: switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_NEW, _ID) } break;
#define PCGEX_FOREACH_BLEND(MACRO)\
PCGEX_BLEND_CASE(None)\
PCGEX_BLEND_CASE(Copy)\
PCGEX_BLEND_CASE(Average)\
PCGEX_BLEND_CASE(Weight)\
PCGEX_BLEND_CASE(Min)\
PCGEX_BLEND_CASE(Max)\
PCGEX_BLEND_CASE(Sum)

		FDataBlendingOperationBase* NewOperation = nullptr;

		switch (Type)
		{
		default:
		PCGEX_FOREACH_BLEND(PCGEX_BLEND_CASE)
		}

		if (NewOperation) { NewOperation->SetAttributeName(Identity.Name); }
		return NewOperation;

#undef PCGEX_SAO_NEW
#undef PCGEX_BLEND_CASE
#undef PCGEX_FOREACH_BLEND
	}
}
