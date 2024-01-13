// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"

#include "PCGEx.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

namespace PCGExData
{
	enum class EInit : uint8
	{
		NoOutput UMETA(DisplayName = "No Output"),
		NewOutput UMETA(DisplayName = "Create Empty Output Object"),
		DuplicateInput UMETA(DisplayName = "Duplicate Input Object"),
		Forward UMETA(DisplayName = "Forward Input Object")
	};

	/**
	 * 
	 */
	struct PCGEXTENDEDTOOLKIT_API FPointIO
	{
		friend class FPointIOGroup;

	protected:
		mutable FRWLock PointsLock;
		int32 NumInPoints = -1;

		FPCGAttributeAccessorKeysPoints* InKeys = nullptr;
		FPCGAttributeAccessorKeysPoints* OutKeys = nullptr;

		const UPCGPointData* In;      // Input PointData	
		UPCGPointData* Out = nullptr; // Output PointData

		FPointIO* RootIO = nullptr;

	public:
		FPCGTaggedData Source; // Source struct
		FPCGTaggedData Output; // Source struct

		explicit FPointIO(
			const FName InDefaultOutputLabel,
			const EInit InInit = EInit::NoOutput)
			: In(nullptr)
		{
			DefaultOutputLabel = InDefaultOutputLabel;
			NumInPoints = 0;
			InitializeOutput(InInit);
		}

		FPointIO(
			const FPCGTaggedData& InSource,
			const UPCGPointData* InData,
			const FName InDefaultOutputLabel,
			const EInit InInit = EInit::NoOutput)
			: In(InData)
		{
			DefaultOutputLabel = InDefaultOutputLabel;
			Source = InSource;
			NumInPoints = InData->GetPoints().Num();
			InitializeOutput(InInit);
		}

		void InitializeOutput(EInit InitOut = EInit::NoOutput);

		~FPointIO();

		const UPCGPointData* GetIn() const;
		UPCGPointData* GetOut() const;
		const UPCGPointData* GetOutIn() const;
		const UPCGPointData* GetInOut() const;

		int32 GetNum() const;
		int32 GetOutNum() const;
		FPCGAttributeAccessorKeysPoints* CreateInKeys();
		FPCGAttributeAccessorKeysPoints* GetInKeys() const;

		FPCGAttributeAccessorKeysPoints* CreateOutKeys();
		FPCGAttributeAccessorKeysPoints* GetOutKeys() const;

		FName DefaultOutputLabel = PCGEx::OutputPointsLabel;

		const FPCGPoint& GetInPoint(const int32 Index) const { return In->GetPoints()[Index]; }
		const FPCGPoint& GetOutPoint(const int32 Index) const { return Out->GetPoints()[Index]; }
		FPCGPoint& GetMutablePoint(const int32 Index) const { return Out->GetMutablePoints()[Index]; }

		PCGEx::FPointRef GetInPointRef(const int32 Index) const { return PCGEx::FPointRef(In->GetPoints()[Index], Index); }
		PCGEx::FPointRef GetOutPointRef(const int32 Index) const { return PCGEx::FPointRef(Out->GetPoints()[Index], Index); }

		const FPCGPoint* TryGetInPoint(const int32 Index) const { return In && In->GetPoints().IsValidIndex(Index) ? &In->GetPoints()[Index] : nullptr; }
		const FPCGPoint* TryGetOutPoint(const int32 Index) const { return Out && Out->GetPoints().IsValidIndex(Index) ? &Out->GetPoints()[Index] : nullptr; }

		void InitPoint(FPCGPoint& Point, PCGMetadataEntryKey FromKey) const;
		void InitPoint(FPCGPoint& Point, const FPCGPoint& FromPoint) const;
		void InitPoint(FPCGPoint& Point) const;
		FPCGPoint& CopyPoint(const FPCGPoint& FromPoint, int32& OutIndex) const;
		FPCGPoint& NewPoint(int32& OutIndex) const;
		void AddPoint(FPCGPoint& Point, int32& OutIndex, bool bInit) const;
		void AddPoint(FPCGPoint& Point, int32& OutIndex, const FPCGPoint& FromPoint) const;

		UPCGPointData* NewEmptyOutput() const;
		UPCGPointData* NewEmptyOutput(FPCGContext* Context, FName PinLabel = NAME_None) const;

		void Cleanup();

		FPointIO& Branch();

		/**
		 * Write valid outputs to Context' tagged data
		 * @param Context 
		 * @param bEmplace if false (default), will try to use the source first
		 */
		bool OutputTo(FPCGContext* Context);
		bool OutputTo(FPCGContext* Context, bool bEmplace, const int32 MinPointCount, const int32 MaxPointCount);
	};

	/**
	 * 
	 */
	class PCGEXTENDEDTOOLKIT_API FPointIOGroup
	{
	protected:
		mutable FRWLock PairsLock;

	public:
		FPointIOGroup();
		FPointIOGroup(FPCGContext* Context, FName InputLabel, EInit InitOut = EInit::NoOutput);
		FPointIOGroup(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, EInit InitOut = EInit::NoOutput);

		~FPointIOGroup();

		FName DefaultOutputLabel = PCGEx::OutputPointsLabel;
		TArray<FPointIO*> Pairs;

		/**
		 * Initialize from Sources
		 * @param Context 
		 * @param Sources 
		 * @param InitOut 
		 */
		void Initialize(
			FPCGContext* Context, TArray<FPCGTaggedData>& Sources,
			EInit InitOut = EInit::NoOutput);

		FPointIO& Emplace_GetRef(
			const FPointIO& PointIO,
			const EInit InitOut = EInit::NoOutput);

		FPointIO& Emplace_GetRef(
			const FPCGTaggedData& Source,
			const UPCGPointData* In,
			const EInit InitOut = EInit::NoOutput);
		FPointIO& Emplace_GetRef(
			const UPCGPointData* In,
			EInit InitOut = EInit::NoOutput);

		FPointIO& Emplace_GetRef(EInit InitOut = EInit::NewOutput);

		bool IsEmpty() const { return Pairs.IsEmpty(); }
		int32 Num() const { return Pairs.Num(); }

		FPointIO& operator[](int32 Index) const { return *Pairs[Index]; }

		void OutputTo(FPCGContext* Context, bool bEmplace = false);
		void OutputTo(FPCGContext* Context, bool bEmplace, const int32 MinPointCount, const int32 MaxPointCount);

		void ForEach(const TFunction<void(FPointIO&, const int32)>& BodyLoop);

		void Flush();
	};

	namespace PCGExPointIO
	{
		static UPCGPointData* GetMutablePointData(FPCGContext* Context, const FPCGTaggedData& Source)
		{
			const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Source.Data);
			if (!SpatialData) { return nullptr; }

			const UPCGPointData* PointData = SpatialData->ToPointData(Context);
			if (!PointData) { return nullptr; }

			return const_cast<UPCGPointData*>(PointData);
		}

		static FPointIO* GetPointIO(
			FPCGContext* Context,
			const FPCGTaggedData& Source,
			const FName OutputLabel = NAME_None,
			const EInit InitOut = EInit::NoOutput)
		{
			if (const UPCGPointData* InData = GetMutablePointData(Context, Source))
			{
				FPointIO* PointIO = new FPointIO(Source, InData, OutputLabel, InitOut);
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
}
