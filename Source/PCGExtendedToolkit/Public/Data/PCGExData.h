// Copyright Timothé Lapetite 2024
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
	
	static UPCGPointData* GetMutablePointData(FPCGContext* Context, const FPCGTaggedData& Source)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Source.Data);
		if (!SpatialData) { return nullptr; }

		const UPCGPointData* PointData = SpatialData->ToPointData(Context);
		if (!PointData) { return nullptr; }

		return const_cast<UPCGPointData*>(PointData);
	}
}
