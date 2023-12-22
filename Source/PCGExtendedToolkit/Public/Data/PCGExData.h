// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointIO.h"
#include "Data/PCGPointData.h"
#include "UObject/Object.h"
//#include "PCGExData.generated.h"

namespace PCGExData
{
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

	template <typename T>
	struct FPointIOMarkedPair
	{
		FName MarkIdentifier = NAME_None;
		T Mark;
		FPointIO* A = nullptr;
		FPointIO* B = nullptr;

		FPointIOMarkedPair()
		{
		}

		FPointIOMarkedPair(FPointIO* InA, FName InMarkIdentifier)
			: MarkIdentifier(InMarkIdentifier),
			  A(InA)
		{
			if (!TryReadMark<T>(*A, MarkIdentifier, Mark)) { Mark = T{}; }
		}

		FPointIOMarkedPair(const FPointIOMarkedPair& Other)
			: MarkIdentifier(Other.MarkIdentifier), Mark(Other.Mark),
			  A(Other.A), B(Other.B)
		{
		}

		~FPointIOMarkedPair()
		{
			MarkIdentifier = NAME_None;
			A = nullptr;
			B = nullptr;
		}

		bool IsMatching(const FPointIO& Other)
		{
			T OtherMark;
			if (!TryReadMark<T>(Other, MarkIdentifier, OtherMark)) { return false; }
			return Mark == OtherMark;
		}

		bool IsValid() { return A && B; }

		void UpdateMark()
		{
			if (!IsValid()) { return; }
			WriteMark<T>(*A, MarkIdentifier, Mark);
			WriteMark<T>(*B, MarkIdentifier, Mark);
		}
	};

	template <typename T>
	struct FKPointIOMarkedBindings
	{
		FName MarkIdentifier = NAME_None;
		T Mark;
		FPointIO* Key = nullptr;
		TArray<FPointIO*> Values;

		FKPointIOMarkedBindings()
		{
		}

		FKPointIOMarkedBindings(FPointIO* InKey, FName InMarkIdentifier)
			: MarkIdentifier(InMarkIdentifier),
			  Key(InKey)
		{
			if (!TryReadMark<T>(*Key, MarkIdentifier, Mark)) { Mark = T{}; }
		}

		FKPointIOMarkedBindings(const FKPointIOMarkedBindings& Other)
			: MarkIdentifier(Other.MarkIdentifier),
			  Mark(Other.Mark),
			  Key(Other.Key)
		{
			Values.Append(Other.Values);
		}

		~FKPointIOMarkedBindings()
		{
			MarkIdentifier = NAME_None;
			Key = nullptr;
			Values.Empty();
		}

		bool IsMatching(const FPointIO& Other)
		{
			T OtherMark;
			if (!TryReadMark<T>(Other, MarkIdentifier, OtherMark)) { return false; }
			return Mark == OtherMark;
		}

		void Add(FPointIO& Other) { Values.Add(&Other); }

		bool IsValid() { return Key && !Values.IsEmpty(); }

		void UpdateMark()
		{
			if (!IsValid()) { return; }
			WriteMark<T>(*Key, MarkIdentifier, Mark);
			for (const FPointIO* Value : Values) { WriteMark<T>(*Value, MarkIdentifier, Mark); }
		}
	};

	static UPCGPointData* GetMutablePointData(FPCGContext* Context, const FPCGTaggedData& Source)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Source.Data);
		if (!SpatialData) { return nullptr; }

		const UPCGPointData* PointData = SpatialData->ToPointData(Context);
		if (!PointData) { return nullptr; }

		return const_cast<UPCGPointData*>(PointData);
	}
}
