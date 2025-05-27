// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPointIO.h"

#include "PCGExContext.h"
#include "PCGExMT.h"

namespace PCGExData
{
	FScope::FScope(UPCGBasePointData* InData, const int32 InStart, const int32 InCount)
		: Data(InData), Start(InStart), Count(InCount), End(InStart + InCount)
	{
	}

	FScope::FScope(const UPCGBasePointData* InData, const int32 InStart, const int32 InCount)
		: Data(const_cast<UPCGBasePointData*>(InData)), Start(InStart), Count(InCount), End(InStart + InCount)
	{
	}

	void FScope::GetIndices(TArray<int32>& OutIndices) const
	{
		OutIndices.SetNumUninitialized(Count);
		for (int i = 0; i < Count; i++) { OutIndices[i] = Start + i; }
	}

#pragma region FPoint

	FElement::FElement(const uint64 Hash)
		: Index(PCGEx::H64A(Hash)), IO(PCGEx::H64B(Hash))
	{
	}

	FElement::FElement(const int32 InIndex, const int32 InIO)
		: Index(InIndex), IO(InIO)
	{
	}

	FElement::FElement(const TSharedPtr<FPointIO>& InIO, const uint32 InIndex)
		: Index(InIndex), IO(InIO->IOIndex)
	{
	}

	FPoint::FPoint(const uint64 Hash)
		: FElement(Hash)
	{
	}

	FPoint::FPoint(const int32 InIndex, const int32 InIO)
		: FElement(InIndex, InIO)
	{
	}

	FPoint::FPoint(const TSharedPtr<FPointIO>& InIO, const uint32 InIndex)
		: FElement(InIO, InIndex)
	{
	}

	FWeightedPoint::FWeightedPoint(const uint64 Hash, const double InWeight)
		: FPoint(Hash)
	{
	}

	FWeightedPoint::FWeightedPoint(const int32 InIndex, const double InWeight, const int32 InIO)
		: FPoint(InIndex, InIO), Weight(InWeight)
	{
	}

	FWeightedPoint::FWeightedPoint(const TSharedPtr<FPointIO>& InIO, const uint32 InIndex, const double InWeight)
		: FPoint(InIO, InIndex), Weight(InWeight)
	{
	}

	FMutablePoint::FMutablePoint(UPCGBasePointData* InData, const int32 InIndex, const int32 InIO)
		: FPoint(InIndex, InIO), Data(InData)
	{
	}

	FMutablePoint::FMutablePoint(const TSharedPtr<FPointIO>& InFacade, const int32 InIndex)
		: FPoint(InFacade, InIndex), Data(InFacade->GetOut())
	{
	}

	FTransform& FMutablePoint::GetMutableTransform()
	{
		TPCGValueRange<FTransform> Transforms = Data->GetTransformValueRange(false);
		return Transforms[Index];
	}

	void FMutablePoint::SetTransform(const FTransform& InValue)
	{
		const TPCGValueRange<FTransform> Transforms = Data->GetTransformValueRange(false);
		Transforms[Index] = InValue;
	}

	void FMutablePoint::SetLocation(const FVector& InValue)
	{
		const TPCGValueRange<FTransform> Transforms = Data->GetTransformValueRange(false);
		Transforms[Index].SetLocation(InValue);
	}

	void FMutablePoint::SetScale3D(const FVector& InValue)
	{
		const TPCGValueRange<FTransform> Transforms = Data->GetTransformValueRange(false);
		Transforms[Index].SetScale3D(InValue);
	}

	void FMutablePoint::SetRotation(const FQuat& InValue)
	{
		const TPCGValueRange<FTransform> Transforms = Data->GetTransformValueRange(false);
		Transforms[Index].SetRotation(InValue);
	}

	void FMutablePoint::SetBoundsMin(const FVector& InValue)
	{
		const TPCGValueRange<FVector> BoundsMin = Data->GetBoundsMinValueRange(false);
		BoundsMin[Index] = InValue;
	}

	void FMutablePoint::SetBoundsMax(const FVector& InValue)
	{
		const TPCGValueRange<FVector> BoundsMax = Data->GetBoundsMaxValueRange(false);
		BoundsMax[Index] = InValue;
	}

	void FMutablePoint::SetExtents(const FVector& InValue, const bool bKeepLocalCenter)
	{
		const TPCGValueRange<FVector> BoundsMin = Data->GetBoundsMinValueRange(false);
		const TPCGValueRange<FVector> BoundsMax = Data->GetBoundsMaxValueRange(false);

		if (bKeepLocalCenter)
		{
			const FVector LocalCenter = Data->GetLocalCenter(Index);
			BoundsMin[Index] = LocalCenter - InValue;
			BoundsMax[Index] = LocalCenter + InValue;
		}
		else
		{
			BoundsMin[Index] = -InValue;
			BoundsMax[Index] = InValue;
		}
	}

	void FMutablePoint::SetLocalBounds(const FBox& InValue)
	{
		const TPCGValueRange<FVector> BoundsMin = Data->GetBoundsMinValueRange(false);
		BoundsMin[Index] = InValue.Min;

		const TPCGValueRange<FVector> BoundsMax = Data->GetBoundsMaxValueRange(false);
		BoundsMax[Index] = InValue.Max;
	}

	void FMutablePoint::SetMetadataEntry(const int64 InValue)
	{
		const TPCGValueRange<int64> MetadataEntries = Data->GetMetadataEntryValueRange(false);
		MetadataEntries[Index] = InValue;
	}

	FConstPoint::FConstPoint(const FMutablePoint& InPoint)
		: FConstPoint(InPoint.Data, InPoint.Index)
	{
	}

	FConstPoint::FConstPoint(const UPCGBasePointData* InData, const uint64 Hash)
		: FPoint(Hash), Data(InData)
	{
	}

	FConstPoint::FConstPoint(const UPCGBasePointData* InData, const int32 InIndex, const int32 InIO)
		: FPoint(InIndex, InIO), Data(InData)
	{
	}

	FConstPoint::FConstPoint(const UPCGBasePointData* InData, const FPoint& InPoint)
		: FPoint(InPoint.Index, InPoint.IO), Data(InData)
	{
	}

	FConstPoint::FConstPoint(const TSharedPtr<FPointIO>& InFacade, const int32 InIndex)
		: FPoint(InFacade, InIndex), Data(InFacade->GetIn())
	{
	}


	FMutablePoint::FMutablePoint(UPCGBasePointData* InData, const uint64 Hash)
		: FPoint(Hash), Data(InData)
	{
	}

	FProxyPoint::FProxyPoint(const FMutablePoint& InPoint):
		Transform(InPoint.GetTransform()),
		BoundsMin(InPoint.GetBoundsMin()),
		BoundsMax(InPoint.GetBoundsMax()),
		Steepness(InPoint.GetSteepness())
	{
	}

	FProxyPoint::FProxyPoint(const FConstPoint& InPoint)
		: Transform(InPoint.GetTransform()),
		  BoundsMin(InPoint.GetBoundsMin()),
		  BoundsMax(InPoint.GetBoundsMax()),
		  Steepness(InPoint.GetSteepness())
	{
	}

	FProxyPoint::FProxyPoint(const UPCGBasePointData* InData, const uint64 Hash)
		: FProxyPoint(FConstPoint(InData, Hash))
	{
	}

	FProxyPoint::FProxyPoint(const UPCGBasePointData* InData, const int32 InIndex, const int32 InIO)
		: FProxyPoint(FConstPoint(InData, InIndex, InIO))
	{
	}

	FProxyPoint::FProxyPoint(const TSharedPtr<FPointIO>& InFacade, const int32 InIndex)
		: FProxyPoint(FConstPoint(InFacade, InIndex))
	{
	}

	void FProxyPoint::CopyTo(UPCGBasePointData* InData) const
	{
		TPCGValueRange<FTransform> OutTransform = InData->GetTransformValueRange(false);

		TPCGValueRange<FVector> OutBoundsMin = InData->GetBoundsMinValueRange(false);
		TPCGValueRange<FVector> OutBoundsMax = InData->GetBoundsMinValueRange(false);

		OutTransform[Index] = Transform;
		OutBoundsMin[Index] = BoundsMin;
		OutBoundsMax[Index] = BoundsMax;
	}

	void FProxyPoint::CopyTo(FMutablePoint& InPoint) const
	{
		InPoint.SetTransform(Transform);
		InPoint.SetBoundsMin(BoundsMin);
		InPoint.SetBoundsMax(BoundsMax);
	}


	void SetPointProperty(FMutablePoint& InPoint, const double InValue, const EPCGExPointPropertyOutput InProperty)
	{
		if (InProperty == EPCGExPointPropertyOutput::Density)
		{
			TPCGValueRange<float> Density = InPoint.Data->GetDensityValueRange(false);
			Density[InPoint.Index] = InValue;
		}
		else if (InProperty == EPCGExPointPropertyOutput::Steepness)
		{
			TPCGValueRange<float> Steepness = InPoint.Data->GetSteepnessValueRange(false);
			Steepness[InPoint.Index] = InValue;
		}
		else if (InProperty == EPCGExPointPropertyOutput::ColorR)
		{
			TPCGValueRange<FVector4> Color = InPoint.Data->GetColorValueRange(false);
			Color[InPoint.Index].Component(0) = InValue;
		}
		else if (InProperty == EPCGExPointPropertyOutput::ColorG)
		{
			TPCGValueRange<FVector4> Color = InPoint.Data->GetColorValueRange(false);
			Color[InPoint.Index].Component(1) = InValue;
		}
		else if (InProperty == EPCGExPointPropertyOutput::ColorB)
		{
			TPCGValueRange<FVector4> Color = InPoint.Data->GetColorValueRange(false);
			Color[InPoint.Index].Component(2) = InValue;
		}
		else if (InProperty == EPCGExPointPropertyOutput::ColorA)
		{
			TPCGValueRange<FVector4> Color = InPoint.Data->GetColorValueRange(false);
			Color[InPoint.Index].Component(3) = InValue;
		}
	}

#pragma endregion

#pragma region FPointIO

	void FPointIO::SetInfos(
		const int32 InIndex,
		const FName InOutputPin,
		const TSet<FString>* InTags)
	{
		IOIndex = InIndex;
		OutputPin = InOutputPin;
		NumInPoints = In ? In->GetNumPoints() : 0;

		if (InTags) { Tags = MakeShared<FTags>(*InTags); }
		else if (!Tags) { Tags = MakeShared<FTags>(); }
	}

	bool FPointIO::InitializeOutput(const EIOInit InitOut)
	{
		PCGEX_SHARED_CONTEXT(ContextHandle)

		if (IsValid(Out) && Out != In)
		{
			SharedContext.Get()->ManagedObjects->Destroy(Out);
			Out = nullptr;
		}

		OutKeys.Reset();

		bMutable = false;

		if (InitOut == EIOInit::None) { return true; }

		if (InitOut == EIOInit::Forward)
		{
			check(In);
			Out = const_cast<UPCGBasePointData*>(In);
			return true;
		}

		bMutable = true;

		if (InitOut == EIOInit::New)
		{
			if (In)
			{
				UObject* GenericInstance = SharedContext.Get()->ManagedObjects->New<UObject>(GetTransientPackage(), In->GetClass());
				Out = Cast<UPCGBasePointData>(GenericInstance);

				// Input type was not a PointData child, should not happen.
				check(Out)

				FPCGInitializeFromDataParams InitializeFromDataParams(In);
				InitializeFromDataParams.bInheritSpatialData = false;
				Out->InitializeFromDataWithParams(InitializeFromDataParams);
			}
			else
			{
				Out = SharedContext.Get()->ManagedObjects->New<UPCGPointArrayData>();
			}

			return true;
		}

		if (InitOut == EIOInit::Duplicate)
		{
			check(In)
			Out = SharedContext.Get()->ManagedObjects->DuplicateData<UPCGBasePointData>(In);
		}

		return true;
	}

	const UPCGBasePointData* FPointIO::GetOutIn(EIOSide& OutSide) const
	{
		if (Out)
		{
			OutSide = EIOSide::Out;
			return Out;
		}

		if (In)
		{
			OutSide = EIOSide::In;
			return In;
		}

		return nullptr;
	}

	const UPCGBasePointData* FPointIO::GetInOut(EIOSide& OutSide) const
	{
		if (In)
		{
			OutSide = EIOSide::In;
			return In;
		}

		if (Out)
		{
			OutSide = EIOSide::Out;
			return Out;
		}

		return nullptr;
	}

	bool FPointIO::GetSource(const UPCGData* InData, EIOSide& OutSide) const
	{
		if (!InData) { return false; }

		if (InData == In)
		{
			OutSide = EIOSide::In;
			return true;
		}

		if (InData == Out)
		{
			OutSide = EIOSide::Out;
			return true;
		}

		return false;
	}

	TSharedPtr<FPCGAttributeAccessorKeysPointIndices> FPointIO::GetInKeys()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPointIO::GetInKeys);

		{
			FReadScopeLock ReadScopeLock(InKeysLock);
			if (InKeys) { return InKeys; }
		}

		{
			FWriteScopeLock WriteScopeLock(InKeysLock);
			if (InKeys) { return InKeys; }
			if (const TSharedPtr<FPointIO> PinnedRoot = RootIO.Pin()) { InKeys = PinnedRoot->GetInKeys(); }
			else { InKeys = MakeShared<FPCGAttributeAccessorKeysPointIndices>(In); }
		}

		return InKeys;
	}

	TSharedPtr<FPCGAttributeAccessorKeysPointIndices> FPointIO::GetOutKeys(const bool bEnsureValidKeys)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPointIO::GetOutKeys);

		check(Out)

		{
			FReadScopeLock ReadScopeLock(OutKeysLock);
			if (OutKeys) { return OutKeys; }
		}

		{
			FWriteScopeLock WriteScopeLock(OutKeysLock);
			if (OutKeys) { return OutKeys; }

			if (bEnsureValidKeys)
			{
				UPCGMetadata* OutMetadata = Out->Metadata;
				TPCGValueRange<int64> MetadataEntries = Out->GetMetadataEntryValueRange(true);
				for (int64& Key : MetadataEntries) { OutMetadata->InitializeOnSet(Key); }

				OutKeys = MakeShared<FPCGAttributeAccessorKeysPointIndices>(Out, false);
			}
			else
			{
				OutKeys = MakeShared<FPCGAttributeAccessorKeysPointIndices>(Out, true);
			}
		}

		return OutKeys;
	}

	void FPointIO::PrintOutKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap) const
	{
		const TPCGValueRange<int64> MetadataEntries = Out->GetMetadataEntryValueRange();
		InMap.Empty(MetadataEntries.Num());
		for (int i = 0; i < MetadataEntries.Num(); i++)
		{
			int64& Key = MetadataEntries[i];
			if (Key == PCGInvalidEntryKey) { Out->Metadata->InitializeOnSet(Key); }
			InMap.Add(Key, i);
		}
	}

	void FPointIO::PrintInKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap) const
	{
		const TConstPCGValueRange<int64> MetadataEntries = In->GetConstMetadataEntryValueRange();
		InMap.Empty(MetadataEntries.Num());
		for (int i = 0; i < MetadataEntries.Num(); i++) { InMap.Add(MetadataEntries[i], i); }
	}

	void FPointIO::PrintOutInKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap) const
	{
		if (Out) { PrintOutKeysMap(InMap); }
		else { PrintInKeysMap(InMap); }
	}

	FScope FPointIO::GetInScope(const int32 Start, const int32 Count, const bool bInclusive) const
	{
		FScope Scope = bInclusive ? FScope(In, Start, Count) : FScope(In, Start + 1, Count - 1);
		return Scope;
	}

	FScope FPointIO::GetOutScope(const int32 Start, const int32 Count, const bool bInclusive) const
	{
		FScope Scope = bInclusive ? FScope(Out, Start, Count) : FScope(Out, Start + 1, Count - 1);
		return Scope;
	}

	FScope FPointIO::GetInRange(const int32 Start, const int32 End, const bool bInclusive) const
	{
		if (Start < End)
		{
			FScope Scope = bInclusive ? FScope(In, Start, End - Start) : FScope(In, Start + 1, (End - Start) - 2);
			return Scope;
		}
		else
		{
			FScope Scope = bInclusive ? FScope(In, End, Start - End) : FScope(In, End + 1, (Start - End) - 2);
			return Scope;
		}
	}

	FScope FPointIO::GetOutRange(const int32 Start, const int32 End, const bool bInclusive) const
	{
		if (Start < End)
		{
			FScope Scope = bInclusive ? FScope(Out, Start, End - Start) : FScope(Out, Start + 1, (End - Start) - 2);
			return Scope;
		}
		else
		{
			FScope Scope = bInclusive ? FScope(Out, End, Start - End) : FScope(Out, End + 1, (Start - End) - 2);
			return Scope;
		}
	}

	void FPointIO::SetPoints(const TArray<FPCGPoint>& InPCGPoints)
	{
		check(Out)
		Out->SetNumPoints(InPCGPoints.Num());
		SetPoints(0, InPCGPoints);
	}

	void FPointIO::SetPoints(const int32 StartIndex, const TArray<FPCGPoint>& InPCGPoints, const EPCGPointNativeProperties Properties)
	{
		check(Out)

#define PCGEX_COPYRANGEIF(_NAME, _TYPE, ...)\
		if (EnumHasAllFlags(Properties, EPCGPointNativeProperties::_NAME)){\
			const TPCGValueRange<_TYPE> Range = Out->Get##_NAME##ValueRange(false);\
			for(int i = 0; i < InPCGPoints.Num(); i++){ Range[StartIndex + i] = InPCGPoints[i]._NAME;}\
		}

		PCGEX_FOREACH_POINT_NATIVE_PROPERTY(PCGEX_COPYRANGEIF)
	}

	TArray<int32>& FPointIO::GetIdxMapping(const int32 NumElements)
	{
		check(Out)
		const int32 ExpectedNumElement = NumElements < 0 ? Out->GetNumPoints() : NumElements;

		{
			FReadScopeLock ReadScopeLock(IdxMappingLock);
			if (IdxMapping.IsValid())
			{
				check(IdxMapping->Num() == ExpectedNumElement)
				return *IdxMapping.Get();
			}
		}

		{
			FWriteScopeLock WriteScopeLock(IdxMappingLock);
			if (IdxMapping.IsValid()) { return *IdxMapping.Get(); }

			IdxMapping = MakeShared<TArray<int32>>();
			IdxMapping->SetNumUninitialized(ExpectedNumElement);

			return *IdxMapping.Get();
		}
	}

	void FPointIO::ClearIdxMapping()
	{
		IdxMapping.Reset();
	}

	void FPointIO::ConsumeIdxMapping(const EPCGPointNativeProperties Properties, const bool bClear)
	{
		check(IdxMapping.IsValid());
		check(IdxMapping->Num() == Out->GetNumPoints());

		InheritProperties(*IdxMapping.Get(), Properties);
		if (bClear) { ClearIdxMapping(); }
	}

	void FPointIO::InheritProperties(const int32 ReadStartIndex, const int32 WriteStartIndex, const int32 Count, const EPCGPointNativeProperties Properties) const
	{
		In->CopyPropertiesTo(Out, ReadStartIndex, WriteStartIndex, Count, Properties);
	}

	void FPointIO::InheritProperties(const TArrayView<const int32>& ReadIndices, const TArrayView<const int32>& WriteIndices, const EPCGPointNativeProperties Properties) const
	{
		In->CopyPropertiesTo(Out, ReadIndices, WriteIndices, Properties);
	}

	void FPointIO::InheritProperties(const TArrayView<const int32>& ReadIndices, const EPCGPointNativeProperties Properties) const
	{
		check(Out->GetNumPoints() >= ReadIndices.Num())

		TArray<int32> WriteIndices;
		PCGEx::ArrayOfIndices(WriteIndices, ReadIndices.Num());

		In->CopyPropertiesTo(Out, ReadIndices, WriteIndices, Properties);
	}

	void FPointIO::InheritPoints(const int32 ReadStartIndex, const int32 WriteStartIndex, const int32 Count) const
	{
		In->CopyPointsTo(Out, ReadStartIndex, WriteStartIndex, Count);
	}

	void FPointIO::InheritPoints(const TArrayView<const int32>& ReadIndices, const TArrayView<const int32>& WriteIndices) const
	{
		In->CopyPointsTo(Out, ReadIndices, WriteIndices);
	}

	int32 FPointIO::InheritPoints(const TArrayView<const int8>& Mask, const bool bInvert) const
	{
		check(In)
		check(Out)
		TArray<int32> WriteIndices;
		const int32 NumElements = PCGEx::ArrayOfIndices(WriteIndices, Mask, 0, bInvert);
		Out->SetNumPoints(WriteIndices.Num());
		InheritPoints(WriteIndices, 0);
		return NumElements;
	}

	int32 FPointIO::InheritPoints(const TBitArray<>& Mask, const bool bInvert) const
	{
		check(In)
		check(Out)
		TArray<int32> WriteIndices;
		const int32 NumElements = PCGEx::ArrayOfIndices(WriteIndices, Mask, 0, bInvert);
		Out->SetNumPoints(WriteIndices.Num());
		InheritPoints(WriteIndices, 0);
		return NumElements;
	}

	void FPointIO::InheritPoints(const TArrayView<const int32>& WriteIndices) const
	{
		check(In)
		check(Out)

		const int32 NumReads = In->GetNumPoints();
		check(NumReads >= WriteIndices.Num())

		TArray<int32> ReadIndices;
		PCGEx::ArrayOfIndices(ReadIndices, NumReads);
		In->CopyPointsTo(Out, ReadIndices, WriteIndices);
	}

	void FPointIO::InheritPoints(const TArrayView<const int32>& SelectedIndices, const int32 StartIndex) const
	{
		check(In)
		check(Out)

		const int32 NewSize = StartIndex + SelectedIndices.Num();
		if (Out->GetNumPoints() < NewSize) { Out->SetNumPoints(NewSize); }

		TArray<int32> WriteIndices;
		PCGEx::ArrayOfIndices(WriteIndices, SelectedIndices.Num(), StartIndex);
		In->CopyPointsTo(Out, SelectedIndices, WriteIndices);
	}

	void FPointIO::RepeatPoint(const int32 ReadIndex, const TArrayView<const int32>& WriteIndices, const EIOSide ReadSide) const
	{
		TArray<int32> SelectedIndices;
		SelectedIndices.Init(ReadIndex, WriteIndices.Num());

		(ReadSide == EIOSide::In ? In : Out)->CopyPointsTo(Out, SelectedIndices, WriteIndices);
	}

	void FPointIO::RepeatPoint(const int32 ReadIndex, const int32 WriteIndex, const int32 Count, const EIOSide ReadSide) const
	{
		TArray<int32> SelectedIndices;
		SelectedIndices.Init(ReadIndex, Count);

		TArray<int32> WriteIndices;
		PCGEx::ArrayOfIndices(WriteIndices, Count, WriteIndex);

		(ReadSide == EIOSide::In ? In : Out)->CopyPointsTo(Out, SelectedIndices, WriteIndices);
	}

	void FPointIO::ClearCachedKeys()
	{
		InKeys.Reset();
		OutKeys.Reset();
	}

	FPointIO::~FPointIO() = default;

	bool FPointIO::StageOutput(FPCGExContext* TargetContext) const
	{
		// If this hits, it needs to be reported. It means a node is trying to output data that is meant to be transactional only
		check(!bTransactional)

		if (!IsEnabled() || !Out || (!bAllowEmptyOutput && Out->IsEmpty())) { return false; }

		TargetContext->StageOutput(OutputPin, Out, Tags->Flatten(), Out != In, bMutable);

		return true;
	}

	bool FPointIO::StageOutput(FPCGExContext* TargetContext, const int32 MinPointCount, const int32 MaxPointCount) const
	{
		if (Out)
		{
			const int64 OutNumPoints = Out->GetNumPoints();
			if ((MinPointCount >= 0 && OutNumPoints < MinPointCount) ||
				(MaxPointCount >= 0 && OutNumPoints > MaxPointCount))
			{
				return StageOutput(TargetContext);
			}
		}
		return false;
	}

	bool FPointIO::StageAnyOutput(FPCGExContext* TargetContext) const
	{
		if (!IsEnabled()) { return false; }

		if (bTransactional)
		{
			if (InitializationData)
			{
				UPCGData* MutableData = const_cast<UPCGData*>(InitializationData.Get());
				if (!MutableData) { return false; }

				TargetContext->StageOutput(OutputPin, MutableData, Tags->Flatten(), false, false);
				return true;
			}

			return false;
		}

		if (!Out || (!bAllowEmptyOutput && Out->IsEmpty())) { return false; }

		return true;
	}

	int32 FPointIO::Gather(const TArrayView<int32> InIndices) const
	{
		if (!Out) { return 0; }

		const int32 ReducedNum = InIndices.Num();

		if (ReducedNum == Out->GetNumPoints()) { return ReducedNum; }

		PCGEX_FOREACH_POINT_NATIVE_PROPERTY_GET(Out)

		Out->AllocateProperties(EPCGPointNativeProperties::All);

		for (int i = 0; i < ReducedNum; i++)
		{
#define PCGEX_VALUERANGE_GATHER(_NAME, _TYPE, ...) _NAME##ValueRange[i] = _NAME##ValueRange[InIndices[i]];
			PCGEX_FOREACH_POINT_NATIVE_PROPERTY(PCGEX_VALUERANGE_GATHER)
#undef PCGEX_VALUERANGE_GATHER
		}

		Out->SetNumPoints(ReducedNum);
		return ReducedNum;
	}

	int32 FPointIO::Gather(const TArrayView<int8> InMask, const bool bInvert) const
	{
		TArray<int32> Indices;
		Indices.Reserve(InMask.Num());

		if (bInvert) { for (int i = 0; i < InMask.Num(); i++) { if (!InMask[i]) { Indices.Add(i); } } }
		else { for (int i = 0; i < InMask.Num(); i++) { if (InMask[i]) { Indices.Add(i); } } }

		return Gather(Indices);
	}

	int32 FPointIO::Gather(const TBitArray<>& InMask, const bool bInvert) const
	{
		TArray<int32> Indices;
		Indices.Reserve(InMask.Num());

		if (bInvert) { for (int i = 0; i < InMask.Num(); i++) { if (!InMask[i]) { Indices.Add(i); } } }
		else { for (int i = 0; i < InMask.Num(); i++) { if (InMask[i]) { Indices.Add(i); } } }

		return Gather(Indices);
	}

	void FPointIO::DeleteAttribute(FName AttributeName) const
	{
		if (!Out) { return; }

		{
			FWriteScopeLock WriteScopeLock(AttributesLock);
			if (Out->Metadata->HasAttribute(AttributeName)) { Out->Metadata->DeleteAttribute(AttributeName); }
		}
	}

#pragma endregion

#pragma region FPointIOCollection

	FPointIOCollection::FPointIOCollection(FPCGExContext* InContext, const bool bIsTransactional)
		: ContextHandle(InContext->GetOrCreateHandle()), bTransactional(bIsTransactional)
	{
		PCGEX_LOG_CTR(FPointIOCollection)
	}

	FPointIOCollection::FPointIOCollection(FPCGExContext* InContext, const FName InputLabel, const EIOInit InitOut, const bool bIsTransactional)
		: FPointIOCollection(InContext, bIsTransactional)
	{
		TArray<FPCGTaggedData> Sources = InContext->InputData.GetInputsByPin(InputLabel);
		Initialize(Sources, InitOut);
	}

	FPointIOCollection::FPointIOCollection(FPCGExContext* InContext, TArray<FPCGTaggedData>& Sources, const EIOInit InitOut, const bool bIsTransactional)
		: FPointIOCollection(InContext, bIsTransactional)
	{
		Initialize(Sources, InitOut);
	}

	FPointIOCollection::~FPointIOCollection() = default;

	void FPointIOCollection::Initialize(
		TArray<FPCGTaggedData>& Sources,
		const EIOInit InitOut)
	{
		PCGEX_SHARED_CONTEXT_VOID(ContextHandle)

		Pairs.Empty(Sources.Num());
		TSet<uint64> UniqueData;
		UniqueData.Reserve(Sources.Num());

		for (int i = 0; i < Sources.Num(); i++)
		{
			FPCGTaggedData& Source = Sources[i];
			bool bIsAlreadyInSet;
			UniqueData.Add(Source.Data->UID, &bIsAlreadyInSet);
			if (bIsAlreadyInSet) { continue; } // Dedupe

			const UPCGBasePointData* SourcePointData = PCGExPointIO::GetPointData(SharedContext.Get(), Source);
			if (!SourcePointData && bTransactional)
			{
				// Only allowed for execution-time only
				// Otherwise find a way to ensure conversion is plugged to the outputs, pin-less
				check(InitOut == EIOInit::None)
				SourcePointData = PCGExPointIO::ToPointData(SharedContext.Get(), Source);
			}

			if (!SourcePointData || SourcePointData->IsEmpty()) { continue; }
			const TSharedPtr<FPointIO> NewIO = Emplace_GetRef(SourcePointData, InitOut, &Source.Tags);
			NewIO->bTransactional = bTransactional;
			NewIO->InitializationIndex = i;
			NewIO->InitializationData = Source.Data;
		}
		UniqueData.Empty();
	}

	TSharedPtr<FPointIO> FPointIOCollection::Emplace_GetRef(
		const UPCGBasePointData* In,
		const EIOInit InitOut,
		const TSet<FString>* Tags)
	{
		FWriteScopeLock WriteLock(PairsLock);
		TSharedPtr<FPointIO> NewIO = Pairs.Add_GetRef(MakeShared<FPointIO>(ContextHandle, In));
		NewIO->SetInfos(Pairs.Num() - 1, OutputPin, Tags);
		if (!NewIO->InitializeOutput(InitOut)) { return nullptr; }
		return NewIO;
	}

	TSharedPtr<FPointIO> FPointIOCollection::Emplace_GetRef(const EIOInit InitOut)
	{
		FWriteScopeLock WriteLock(PairsLock);
		TSharedPtr<FPointIO> NewIO = Pairs.Add_GetRef(MakeShared<FPointIO>(ContextHandle));
		NewIO->SetInfos(Pairs.Num() - 1, OutputPin);
		if (!NewIO->InitializeOutput(InitOut)) { return nullptr; }
		return NewIO;
	}

	TSharedPtr<FPointIO> FPointIOCollection::Emplace_GetRef(const TSharedPtr<FPointIO>& PointIO, const EIOInit InitOut)
	{
		TSharedPtr<FPointIO> Branch = Emplace_GetRef(PointIO->GetIn(), InitOut);
		if (!Branch) { return nullptr; }

		Branch->Tags->Reset(PointIO->Tags);
		Branch->RootIO = PointIO;
		return Branch;
	}

	TSharedPtr<FPointIO> FPointIOCollection::Insert_Unsafe(const int32 Index, const TSharedPtr<FPointIO>& PointIO)
	{
		check(!Pairs[Index]) // should be an empty spot
		Pairs[Index] = PointIO;
		PointIO->SetInfos(Index, OutputPin);
		return PointIO;
	}

	TSharedPtr<FPointIO> FPointIOCollection::Add_Unsafe(const TSharedPtr<FPointIO>& PointIO)
	{
		PointIO->SetInfos(Pairs.Add(PointIO), OutputPin);
		return PointIO;
	}

	void FPointIOCollection::Add_Unsafe(const TArray<TSharedPtr<FPointIO>>& IOs)
	{
		if (IOs.IsEmpty()) { return; }
		Pairs.Reserve(Pairs.Num() + IOs.Num());
		for (const TSharedPtr<FPointIO>& IO : IOs)
		{
			if (!IO) { continue; }
			Add_Unsafe(IO);
		}
	}

	void FPointIOCollection::IncreaseReserve(const int32 InIncreaseNum)
	{
		FWriteScopeLock WriteLock(PairsLock);
		Pairs.Reserve(Pairs.Max() + InIncreaseNum);
	}

	void FPointIOCollection::StageOutputs()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPointIOCollection::StageOutputs);

		PCGEX_SHARED_CONTEXT_VOID(ContextHandle)
		FPCGExContext* Context = SharedContext.Get();

		Sort();

		Context->IncreaseStagedOutputReserve(Pairs.Num());

		for (int i = 0; i < Pairs.Num(); i++) { (void)Pairs[i]->StageOutput(Context); }
	}

	void FPointIOCollection::StageOutputs(const int32 MinPointCount, const int32 MaxPointCount)
	{
		PCGEX_SHARED_CONTEXT_VOID(ContextHandle)

		FPCGExContext* Context = SharedContext.Get();

		if (!Context) { return; }

		Sort();

		Context->IncreaseStagedOutputReserve(Pairs.Num());
		for (int i = 0; i < Pairs.Num(); i++) { Pairs[i]->StageOutput(Context, MinPointCount, MaxPointCount); }
	}

	void FPointIOCollection::StageAnyOutputs()
	{
		PCGEX_SHARED_CONTEXT_VOID(ContextHandle)

		FPCGExContext* Context = SharedContext.Get();

		Sort();

		Context->IncreaseStagedOutputReserve(Pairs.Num());
		for (int i = 0; i < Pairs.Num(); i++) { Pairs[i]->StageAnyOutput(Context); }
	}

	void FPointIOCollection::Sort()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPointIOCollection::Sort);
		Pairs.Sort([](const TSharedPtr<FPointIO>& A, const TSharedPtr<FPointIO>& B) { return A->IOIndex < B->IOIndex; });
	}

	FBox FPointIOCollection::GetInBounds() const
	{
		FBox Bounds = FBox(ForceInit);
		for (const TSharedPtr<FPointIO>& IO : Pairs) { Bounds += IO->GetIn()->GetBounds(); }
		return Bounds;
	}

	FBox FPointIOCollection::GetOutBounds() const
	{
		FBox Bounds = FBox(ForceInit);
		for (const TSharedPtr<FPointIO>& IO : Pairs) { Bounds += IO->GetOut()->GetBounds(); }
		return Bounds;
	}

	void FPointIOCollection::PruneNullEntries(const bool bUpdateIndices)
	{
		const int32 MaxPairs = Pairs.Num();
		int32 WriteIndex = 0;
		if (!bUpdateIndices)
		{
			for (int32 i = 0; i < MaxPairs; i++) { if (Pairs[i]) { Pairs[WriteIndex++] = Pairs[i]; } }
		}
		else
		{
			for (int32 i = 0; i < MaxPairs; i++)
			{
				if (Pairs[i])
				{
					Pairs[i]->IOIndex = WriteIndex;
					Pairs[WriteIndex++] = Pairs[i];
				}
			}
		}
		Pairs.SetNum(WriteIndex);
	}

	void FPointIOCollection::Flush()
	{
		Pairs.Empty();
	}

#pragma endregion

#pragma region FPointIOTaggedEntries

	void FPointIOTaggedEntries::Add(const TSharedRef<FPointIO>& Value)
	{
		Entries.AddUnique(Value);
		Value->Tags->Set(TagId, TagValue);
	}

	bool FPointIOTaggedDictionary::CreateKey(const TSharedRef<FPointIO>& PointIOKey)
	{
		IDType TagValue = PointIOKey->Tags->GetOrSet<int32>(TagId, PointIOKey->GetInOut()->GetUniqueID());
		for (const TSharedPtr<FPointIOTaggedEntries>& Binding : Entries)
		{
			// TagValue shouldn't exist already
			if (Binding->TagValue->Value == TagValue->Value) { return false; }
		}

		TagMap.Add(TagValue->Value, Entries.Add(MakeShared<FPointIOTaggedEntries>(TagId, TagValue)));
		return true;
	}

	bool FPointIOTaggedDictionary::TryAddEntry(const TSharedRef<FPointIO>& PointIOEntry)
	{
		const IDType TagValue = PointIOEntry->Tags->GetTypedValue<int32>(TagId);
		if (!TagValue) { return false; }

		if (const int32* Index = TagMap.Find(TagValue->Value))
		{
			Entries[*Index]->Add(PointIOEntry);
			return true;
		}

		return false;
	}

	TSharedPtr<FPointIOTaggedEntries> FPointIOTaggedDictionary::GetEntries(const int32 Key)
	{
		if (const int32* Index = TagMap.Find(Key)) { return Entries[*Index]; }
		return nullptr;
	}

	void GetPoints(const FScope& Scope, TArray<FPCGPoint>& OutPCGPoints)
	{
		check(Scope.IsValid())

		OutPCGPoints.SetNum(Scope.Count);

		const TConstPCGValueRange<FTransform> TransformRange = Scope.Data->GetConstTransformValueRange();
		const TConstPCGValueRange<float> SteepnessRange = Scope.Data->GetConstSteepnessValueRange();
		const TConstPCGValueRange<float> DensityRange = Scope.Data->GetConstDensityValueRange();
		const TConstPCGValueRange<FVector> BoundsMinRange = Scope.Data->GetConstBoundsMinValueRange();
		const TConstPCGValueRange<FVector> BoundsMaxRange = Scope.Data->GetConstBoundsMaxValueRange();
		const TConstPCGValueRange<FVector4> ColorRange = Scope.Data->GetConstColorValueRange();
		const TConstPCGValueRange<int64> MetadataEntryRange = Scope.Data->GetConstMetadataEntryValueRange();
		const TConstPCGValueRange<int32> SeedRange = Scope.Data->GetConstSeedValueRange();

		for (int i = 0; i < Scope.Count; i++)
		{
			const int32 Index = Scope.Start + i;
			FPCGPoint& Pt = OutPCGPoints[i];
			Pt.Transform = TransformRange[Index];
			Pt.Steepness = SteepnessRange[Index];
			Pt.Density = DensityRange[Index];
			Pt.BoundsMin = BoundsMinRange[Index];
			Pt.BoundsMax = BoundsMaxRange[Index];
			Pt.Color = ColorRange[Index];
			Pt.MetadataEntry = MetadataEntryRange[Index];
			Pt.Seed = SeedRange[Index];
		}
	}

	int32 PCGExPointIO::GetTotalPointsNum(const TArray<TSharedPtr<FPointIO>>& InIOs, const EIOSide InSide)
	{
		int32 TotalNum = 0;

		if (InSide == EIOSide::In)
		{
			for (const TSharedPtr<FPointIO>& IO : InIOs)
			{
				if (!IO || !IO->GetIn()) { continue; }
				TotalNum += IO->GetIn()->GetNumPoints();
			}
		}
		else
		{
			for (const TSharedPtr<FPointIO>& IO : InIOs)
			{
				if (!IO || !IO->GetOut()) { continue; }
				TotalNum += IO->GetOut()->GetNumPoints();
			}
		}

		return TotalNum;
	}

#pragma endregion
}
