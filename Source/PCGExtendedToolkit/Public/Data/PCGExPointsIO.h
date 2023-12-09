// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"

#include "PCGEx.h"
#include "PCGExData.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

namespace PCGExPointIO
{
	enum class EInit : uint8
	{
		NoOutput UMETA(DisplayName = "No Output"),
		NewOutput UMETA(DisplayName = "Create Empty Output Object"),
		DuplicateInput UMETA(DisplayName = "Duplicate Input Object"),
		Forward UMETA(DisplayName = "Forward Input Object")
	};
}

/**
 * 
 */
struct PCGEXTENDEDTOOLKIT_API FPCGExPointIO
{
protected:
	mutable FRWLock MapLock;
	mutable FRWLock PointsLock;

	int32 NumInPoints = -1;

	bool bMetadataEntryDirty = true;
	bool bIndicesDirty = true;

	FPCGAttributeAccessorKeysPoints* InKeys = nullptr;
	FPCGAttributeAccessorKeysPoints* OutKeys = nullptr;

	const UPCGPointData* In;      // Input PointData	
	UPCGPointData* Out = nullptr; // Output PointData

public:
	FPCGTaggedData Source; // Source struct
	FPCGTaggedData Output; // Source struct

	FPCGExPointIO(
		FName InDefaultOutputLabel,
		PCGExPointIO::EInit InInit = PCGExPointIO::EInit::NoOutput)
		: In(nullptr)
	{
		DefaultOutputLabel = InDefaultOutputLabel;
		NumInPoints = 0;
		InitializeOutput(InInit);
	}

	FPCGExPointIO(
		const FPCGTaggedData& InSource,
		const UPCGPointData* InData,
		FName InDefaultOutputLabel,
		PCGExPointIO::EInit InInit = PCGExPointIO::EInit::NoOutput)
		: In(InData)
	{
		DefaultOutputLabel = InDefaultOutputLabel;
		Source = InSource;
		NumInPoints = InData->GetPoints().Num();
		InitializeOutput(InInit);
	}

	void InitializeOutput(PCGExPointIO::EInit InitOut = PCGExPointIO::EInit::NoOutput);

	~FPCGExPointIO();
	
	const UPCGPointData* GetIn() const;
	int32 GetNum() const;
	FPCGAttributeAccessorKeysPoints* GetInKeys();
	UPCGPointData* GetOut() const;
	FPCGAttributeAccessorKeysPoints* GetOutKeys();

	FName DefaultOutputLabel = PCGEx::OutputPointsLabel;

	const FPCGPoint& GetInPoint(const int32 Index) const { return In->GetPoints()[Index]; }
	const FPCGPoint& GetOutPoint(const int32 Index) const { return Out->GetPoints()[Index]; }
	FPCGPoint& GetMutablePoint(const int32 Index) const { return Out->GetMutablePoints()[Index]; }

	const FPCGPoint* TryGetInPoint(const int32 Index) const { return In->GetPoints().IsValidIndex(Index) ? &In->GetPoints()[Index] : nullptr; }
	const FPCGPoint* TryGetOutPoint(const int32 Index) const { return Out->GetPoints().IsValidIndex(Index) ? &Out->GetPoints()[Index] : nullptr; }

	void InitPoint(FPCGPoint& Point, PCGMetadataEntryKey FromKey) const;
	void InitPoint(FPCGPoint& Point, const FPCGPoint& FromPoint) const;
	void InitPoint(FPCGPoint& Point) const;
	FPCGPoint& CopyPoint(const FPCGPoint& FromPoint, int32& OutIndex) const;
	FPCGPoint& NewPoint(int32& OutIndex) const;
	void AddPoint(FPCGPoint& Point, int32& OutIndex, bool bInit) const;
	void AddPoint(FPCGPoint& Point, int32& OutIndex, const FPCGPoint& FromPoint) const;

	UPCGPointData* NewEmptyOutput() const;
	UPCGPointData* NewEmptyOutput(FPCGContext* Context, FName PinLabel = NAME_None) const;

	void Cleanup() const;

	void BuildMetadataEntries();

	/**
	 * Write valid outputs to Context' tagged data
	 * @param Context 
	 * @param bEmplace if false (default), will try to use the source first
	 */
	bool OutputTo(FPCGContext* Context, bool bEmplace = false);
	bool OutputTo(FPCGContext* Context, bool bEmplace, int64 MinPointCount, int64 MaxPointCount);
};

/**
 * 
 */
class PCGEXTENDEDTOOLKIT_API FPCGExPointIOGroup
{
protected:
	mutable FRWLock PairsLock;

public:
	FPCGExPointIOGroup();
	FPCGExPointIOGroup(FPCGContext* Context, FName InputLabel, PCGExPointIO::EInit InitOut = PCGExPointIO::EInit::NoOutput);
	FPCGExPointIOGroup(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, PCGExPointIO::EInit InitOut = PCGExPointIO::EInit::NoOutput);

	~FPCGExPointIOGroup();

	FName DefaultOutputLabel = PCGEx::OutputPointsLabel;
	TArray<FPCGExPointIO> Pairs;

	/**
	 * Initialize from Sources
	 * @param Context 
	 * @param Sources 
	 * @param InitOut 
	 */
	void Initialize(
		FPCGContext* Context, TArray<FPCGTaggedData>& Sources,
		PCGExPointIO::EInit InitOut = PCGExPointIO::EInit::NoOutput);

	void Initialize(
		FPCGContext* Context, TArray<FPCGTaggedData>& Sources,
		const PCGExPointIO::EInit InitOut,
		const TFunction<bool(UPCGPointData*)>& ValidateFunc,
		const TFunction<void(FPCGExPointIO&)>& PostInitFunc);

	FPCGExPointIO& Emplace_GetRef(
		const FPCGExPointIO& PointIO,
		const PCGExPointIO::EInit InitOut = PCGExPointIO::EInit::NoOutput);

	FPCGExPointIO& Emplace_GetRef(
		const FPCGTaggedData& Source,
		const UPCGPointData* In,
		const PCGExPointIO::EInit InitOut = PCGExPointIO::EInit::NoOutput);
	FPCGExPointIO& Emplace_GetRef(
		const UPCGPointData* In,
		PCGExPointIO::EInit InitOut = PCGExPointIO::EInit::NoOutput);

	FPCGExPointIO& Emplace_GetRef(PCGExPointIO::EInit InitOut = PCGExPointIO::EInit::NewOutput);

	bool IsEmpty() const { return Pairs.IsEmpty(); }
	int32 Num() const { return Pairs.Num(); }

	void OutputTo(FPCGContext* Context, bool bEmplace = false);
	void OutputTo(FPCGContext* Context, bool bEmplace, const int64 MinPointCount, const int64 MaxPointCount);

	void ForEach(const TFunction<void(FPCGExPointIO&, const int32)>& BodyLoop);

	void Flush();
};

namespace PCGExPointIO
{
	static FPCGExPointIO* GetPointIO(
		FPCGContext* Context,
		const FPCGTaggedData& Source,
		const FName OutputLabel = NAME_None,
		const PCGExPointIO::EInit InitOut = PCGExPointIO::EInit::NoOutput)
	{
		if (const UPCGPointData* InData = PCGExData::GetMutablePointData(Context, Source))
		{
			FPCGExPointIO* PointIO = new FPCGExPointIO(Source, InData, OutputLabel, InitOut);
			PointIO->InitializeOutput(InitOut);
			return PointIO;
		}
		return nullptr;
	}

	static UPCGPointData* NewEmptyPointData(const UPCGPointData* InData = nullptr)
	{
		UPCGPointData* OutData = NewObject<UPCGPointData>();
		if (InData) { OutData->InitializeFromData(InData); }
		return OutData;
	}

	static UPCGPointData* NewEmptyPointData(FPCGContext* Context, const FName PinLabel, const UPCGPointData* InData = nullptr)
	{
		UPCGPointData* OutData = NewObject<UPCGPointData>();
		if (InData) { OutData->InitializeFromData(InData); }
		FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Emplace_GetRef();
		OutputRef.Data = OutData;
		OutputRef.Pin = PinLabel;

		return OutData;
	}
}
