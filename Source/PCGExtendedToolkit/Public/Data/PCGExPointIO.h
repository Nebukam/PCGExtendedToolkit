// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"

#include "PCGEx.h"
#include "PCGExContext.h"
#include "PCGExDataTag.h"
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

	enum class ESource : uint8
	{
		In,
		Out
	};

	/**
	 * 
	 */
	struct PCGEXTENDEDTOOLKIT_API FPointIO
	{
		friend class FPointIOCollection;

	protected:
		bool bWritten = false;
		mutable FRWLock PointsLock;
		int32 NumInPoints = -1;

		FPCGAttributeAccessorKeysPoints* InKeys = nullptr;
		FPCGAttributeAccessorKeysPoints* OutKeys = nullptr;

		const UPCGPointData* In;      // Input PointData	
		UPCGPointData* Out = nullptr; // Output PointData

		FPointIO* RootIO = nullptr;
		bool bEnabled = true;

	public:
		FTags* Tags = nullptr;
		int32 IOIndex = 0;

		explicit FPointIO(
			const FName InDefaultOutputLabel,
			const EInit InInit = EInit::NoOutput,
			const int32 InIndex = -1)
			: In(nullptr), IOIndex(InIndex)
		{
			DefaultOutputLabel = InDefaultOutputLabel;
			NumInPoints = 0;
			Tags = new FTags();
			InitializeOutput(InInit);
		}

		FPointIO(
			const UPCGPointData* InData,
			const FName InDefaultOutputLabel,
			const EInit InInit = EInit::NoOutput,
			const int32 InIndex = -1,
			const TSet<FString>* InTags = nullptr)
			: In(InData), IOIndex(InIndex)
		{
			DefaultOutputLabel = InDefaultOutputLabel;
			Tags = InTags ? new FTags(*InTags) : new FTags();
			NumInPoints = InData->GetPoints().Num();
			InitializeOutput(InInit);
		}

		void InitializeOutput(EInit InitOut = EInit::NoOutput);

		~FPointIO();

		const UPCGPointData* GetData(const ESource InSource) const;
		UPCGPointData* GetMutableData(ESource InSource) const;
		const UPCGPointData* GetIn() const;
		UPCGPointData* GetOut() const;
		const UPCGPointData* GetOutIn() const;
		const UPCGPointData* GetInOut() const;

		int32 GetNum() const;
		int32 GetOutNum() const;

		FPCGAttributeAccessorKeysPoints* CreateInKeys();
		FPCGAttributeAccessorKeysPoints* GetInKeys() const;
		void PrintInKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap);

		FPCGAttributeAccessorKeysPoints* CreateOutKeys();
		FPCGAttributeAccessorKeysPoints* GetOutKeys() const;
		void PrintOutKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap, bool bInitializeOnSet);

		FPCGAttributeAccessorKeysPoints* CreateKeys(ESource InSource);
		FPCGAttributeAccessorKeysPoints* GetKeys(ESource InSource) const;

		FName DefaultOutputLabel = PCGEx::OutputPointsLabel;

		FORCEINLINE const FPCGPoint& GetInPoint(const int32 Index) const { return In->GetPoints()[Index]; }
		FORCEINLINE const FPCGPoint& GetOutPoint(const int32 Index) const { return Out->GetPoints()[Index]; }
		FORCEINLINE FPCGPoint& GetMutablePoint(const int32 Index) const { return Out->GetMutablePoints()[Index]; }

		FORCEINLINE PCGEx::FPointRef GetInPointRef(const int32 Index) const { return PCGEx::FPointRef(In->GetPoints()[Index], Index); }
		FORCEINLINE PCGEx::FPointRef GetOutPointRef(const int32 Index) const { return PCGEx::FPointRef(Out->GetPoints()[Index], Index); }

		FORCEINLINE const FPCGPoint* TryGetInPoint(const int32 Index) const { return In && In->GetPoints().IsValidIndex(Index) ? &In->GetPoints()[Index] : nullptr; }
		FORCEINLINE const FPCGPoint* TryGetOutPoint(const int32 Index) const { return Out && Out->GetPoints().IsValidIndex(Index) ? &Out->GetPoints()[Index] : nullptr; }

		FORCEINLINE void InitPoint(FPCGPoint& Point, PCGMetadataEntryKey FromKey) const;
		FORCEINLINE void InitPoint(FPCGPoint& Point, const FPCGPoint& FromPoint) const;
		FORCEINLINE void InitPoint(FPCGPoint& Point) const;
		FORCEINLINE FPCGPoint& CopyPoint(const FPCGPoint& FromPoint, int32& OutIndex) const;
		FORCEINLINE FPCGPoint& NewPoint(int32& OutIndex) const;
		FORCEINLINE void AddPoint(FPCGPoint& Point, int32& OutIndex, bool bInit) const;
		FORCEINLINE void AddPoint(FPCGPoint& Point, int32& OutIndex, const FPCGPoint& FromPoint) const;

		void SetNumInitialized(const int32 NumPoints, const bool bForceInit = false) const;

		void CleanupKeys();

		FPointIO& Branch();

		void Disable() { bEnabled = false; }
		void Enable() { bEnabled = true; }
		bool IsEnabled() const { return bEnabled; }
		/**
		 * Write valid outputs to Context' tagged data
		 * @param Context 
		 */
		bool OutputTo(FPCGContext* Context);
		bool OutputTo(FPCGContext* Context, const int32 MinPointCount, const int32 MaxPointCount);
	};

	/**
	 * 
	 */
	class PCGEXTENDEDTOOLKIT_API FPointIOCollection
	{
	protected:
		mutable FRWLock PairsLock;

	public:
		FPointIOCollection();
		FPointIOCollection(const FPCGContext* Context, FName InputLabel, EInit InitOut = EInit::NoOutput);
		FPointIOCollection(const FPCGContext* Context, TArray<FPCGTaggedData>& Sources, EInit InitOut = EInit::NoOutput);

		~FPointIOCollection();

		FName DefaultOutputLabel = PCGEx::OutputPointsLabel;
		TArray<FPointIO*> Pairs;

		/**
		 * Initialize from Sources
		 * @param Context 
		 * @param Sources 
		 * @param InitOut 
		 */
		void Initialize(
			const FPCGContext* Context, TArray<FPCGTaggedData>& Sources,
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

		FPointIO& operator[](const int32 Index) const { return *Pairs[Index]; }

		void OutputTo(FPCGContext* Context);
		void OutputTo(FPCGContext* Context, const int32 MinPointCount, const int32 MaxPointCount);

		void ForEach(const TFunction<void(FPointIO&, const int32)>& BodyLoop);

		void Sort();

		FBox GetInBounds() const;
		FBox GetOutBounds() const;

		void Flush();
	};

	class PCGEXTENDEDTOOLKIT_API FPointIOTaggedEntries
	{
	public:
		FString TagId;
		FString TagValue;
		TArray<FPointIO*> Entries;

		FPointIOTaggedEntries(const FString& InTagId, const FString& InTagValue)
			: TagId(InTagId), TagValue(InTagValue)
		{
		}

		~FPointIOTaggedEntries()
		{
			Entries.Empty();
		}

		void Add(FPointIO* Value);
	};

	class PCGEXTENDEDTOOLKIT_API FPointIOTaggedDictionary
	{
	public:
		FString TagId;
		TMap<FString, int32> TagMap;
		TArray<FPointIOTaggedEntries*> Entries;

		explicit FPointIOTaggedDictionary(const FString& InTagId)
			: TagId(InTagId)
		{
		}

		~FPointIOTaggedDictionary()
		{
			TagMap.Empty();
			Entries.Empty();
		}

		bool CreateKey(const FPointIO& PointIOKey);
		bool TryAddEntry(FPointIO& PointIOEntry);
		FPointIOTaggedEntries* GetEntries(const FString& Key);
	};

	namespace PCGExPointIO
	{
		static UPCGPointData* GetMutablePointData(const FPCGContext* Context, const FPCGTaggedData& Source)
		{
			const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Source.Data);
			if (!SpatialData) { return nullptr; }

			const UPCGPointData* PointData = SpatialData->ToPointData(const_cast<FPCGContext*>(Context));
			if (!PointData) { return nullptr; }

			return const_cast<UPCGPointData*>(PointData);
		}

		static FPointIO* GetPointIO(
			const FPCGContext* Context,
			const FPCGTaggedData& Source,
			const FName OutputLabel = NAME_None,
			const EInit InitOut = EInit::NoOutput)
		{
			if (const UPCGPointData* InData = GetMutablePointData(Context, Source))
			{
				FPointIO* PointIO = new FPointIO(InData, OutputLabel, InitOut, -1, &Source.Tags);
				PointIO->InitializeOutput(InitOut);
				return PointIO;
			}
			return nullptr;
		}
	}
}
