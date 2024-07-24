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
#include "PCGExPointData.h"
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

	struct PCGEXTENDEDTOOLKIT_API FPointRef
	{
		friend struct FPointIO;

		FPointRef(const FPCGPoint& InPoint, const int32 InIndex):
			Point(&InPoint), Index(InIndex)
		{
		}

		FPointRef(const FPCGPoint* InPoint, const int32 InIndex):
			Point(InPoint), Index(InIndex)
		{
		}

		FPointRef(const FPointRef& Other):
			Point(Other.Point), Index(Other.Index)
		{
		}

		bool IsValid() const { return Point && Index != -1; }
		const FPCGPoint* Point = nullptr;
		const int32 Index = -1;

		FORCEINLINE FPCGPoint& MutablePoint() const { return const_cast<FPCGPoint&>(*Point); }
	};

	/**
	 * 
	 */
	struct PCGEXTENDEDTOOLKIT_API FPointIO
	{
		friend class FPointIOCollection;

	protected:
		FPCGExContext* Context = nullptr;

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

		FPointIO(FPCGExContext* InContext):
			Context(InContext), In(nullptr)
		{
			PCGEX_LOG_CTR(FPointIO)
		}

		explicit FPointIO(FPCGExContext* InContext, const UPCGPointData* InData):
			Context(InContext), In(InData)
		{
			PCGEX_LOG_CTR(FPointIO)
		}

		explicit FPointIO(FPCGExContext* InContext, const FPointIO* InPointIO):
			Context(InContext), In(InPointIO->GetIn())
		{
			PCGEX_LOG_CTR(FPointIO)
			Tags = new FTags(*InPointIO->Tags);
		}

		FPCGExContext* GetContext() const { return Context; }

		void SetInfos(const int32 InIndex,
		              const FName InDefaultOutputLabel,
		              const TSet<FString>* InTags = nullptr);

		void InitializeOutput(EInit InitOut = EInit::NoOutput);

		template <typename T>
		void InitializeOutput(const EInit InitOut = EInit::NoOutput)
		{
			if (Out != In) { PCGEX_DELETE_UOBJECT(Out) }

			if (InitOut == EInit::NewOutput)
			{
				PCGEX_NEW_TRANSIENT(T, TypedOut)

				Out = Cast<UPCGPointData>(TypedOut);
				check(Out)

				if (In) { Out->InitializeFromData(In); }
				const UPCGExPointData* TypedPointData = Cast<UPCGExPointData>(In);
				UPCGExPointData* TypedOutPointData = Cast<UPCGExPointData>(TypedOut);
				if (TypedPointData && TypedOutPointData) { TypedOutPointData->InitializeFromPCGExData(TypedPointData, EInit::NewOutput); }

				return;
			}

			if (InitOut == EInit::DuplicateInput)
			{
				check(In)
				const T* TypedIn = Cast<T>(In);

				if (!TypedIn)
				{
					PCGEX_NEW_TRANSIENT(T, TypedOut)

					if (UPCGExPointData* TypedPointData = Cast<UPCGExPointData>(TypedOut)) { TypedPointData->CopyFrom(In); }
					else { TypedOut->InitializeFromData(In); } // This is a potentially failed duplicate

					Out = Cast<UPCGPointData>(TypedOut);
				}
				else
				{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
					Out = Cast<UPCGPointData>(In->DuplicateData(true));
#else
					Out = Cast<UPCGPointData>(In->DuplicateData(Context, true));
#endif
				}

				return;
			}

			InitializeOutput(InitOut);
		}

		~FPointIO();

		FORCEINLINE const UPCGPointData* GetData(const ESource InSource) const { return InSource == ESource::In ? In : Out; }
		FORCEINLINE UPCGPointData* GetMutableData(ESource InSource) const { return const_cast<UPCGPointData*>(InSource == ESource::In ? In : Out); }
		FORCEINLINE const UPCGPointData* GetIn() const { return In; }
		FORCEINLINE UPCGPointData* GetOut() const { return Out; }
		FORCEINLINE const UPCGPointData* GetOutIn() const { return Out ? Out : In; }
		FORCEINLINE const UPCGPointData* GetInOut() const { return In ? In : Out; }

		FORCEINLINE int32 GetNum() const { return In ? In->GetPoints().Num() : Out ? Out->GetPoints().Num() : -1; }
		FORCEINLINE int32 GetNum(const ESource Source) const { return Source == ESource::In ? In->GetPoints().Num() : Out->GetPoints().Num(); }
		FORCEINLINE int32 GetOutInNum() const { return Out && !Out->GetPoints().IsEmpty() ? Out->GetPoints().Num() : In ? In->GetPoints().Num() : -1; }

		FPCGAttributeAccessorKeysPoints* CreateInKeys();
		FORCEINLINE FPCGAttributeAccessorKeysPoints* GetInKeys() const { return InKeys; }
		void PrintInKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap);

		FPCGAttributeAccessorKeysPoints* CreateOutKeys();
		FORCEINLINE FPCGAttributeAccessorKeysPoints* GetOutKeys() const { return OutKeys; }
		void PrintOutKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap, bool bInitializeOnSet);

		FPCGAttributeAccessorKeysPoints* CreateKeys(ESource InSource);
		FORCEINLINE FPCGAttributeAccessorKeysPoints* GetKeys(const ESource InSource) const { return InSource == ESource::In ? GetInKeys() : GetOutKeys(); }

		FName DefaultOutputLabel = PCGEx::OutputPointsLabel;

		FORCEINLINE const FPCGPoint& GetInPoint(const int32 Index) const { return *(In->GetPoints().GetData() + Index); }
		FORCEINLINE const FPCGPoint& GetOutPoint(const int32 Index) const { return *(Out->GetPoints().GetData() + Index); }
		FORCEINLINE FPCGPoint& GetMutablePoint(const int32 Index) const { return *(Out->GetMutablePoints().GetData() + Index); }

		FORCEINLINE FPointRef GetInPointRef(const int32 Index) const { return FPointRef(In->GetPoints().GetData() + Index, Index); }
		FORCEINLINE FPointRef GetOutPointRef(const int32 Index) const { return FPointRef(Out->GetPoints().GetData() + Index, Index); }

		FORCEINLINE FPointRef* GetInPointRefPtr(const int32 Index) const { return new FPointRef(In->GetPoints().GetData() + Index, Index); }
		FORCEINLINE FPointRef* GetOutPointRefPtr(const int32 Index) const { return new FPointRef(Out->GetPoints().GetData() + Index, Index); }

		FORCEINLINE const FPCGPoint* TryGetInPoint(const int32 Index) const { return In && In->GetPoints().IsValidIndex(Index) ? (In->GetPoints().GetData() + Index) : nullptr; }
		FORCEINLINE const FPCGPoint* TryGetOutPoint(const int32 Index) const { return Out && Out->GetPoints().IsValidIndex(Index) ? (Out->GetPoints().GetData() + Index) : nullptr; }

		FORCEINLINE void InitPoint(FPCGPoint& Point, const PCGMetadataEntryKey FromKey) const { Out->Metadata->InitializeOnSet(Point.MetadataEntry, FromKey, In->Metadata); }
		FORCEINLINE void InitPoint(FPCGPoint& Point, const FPCGPoint& FromPoint) const { Out->Metadata->InitializeOnSet(Point.MetadataEntry, FromPoint.MetadataEntry, In->Metadata); }
		FORCEINLINE void InitPoint(FPCGPoint& Point) const { Out->Metadata->InitializeOnSet(Point.MetadataEntry); }
		FORCEINLINE FPCGPoint& CopyPoint(const FPCGPoint& FromPoint, int32& OutIndex) const
		{
			FWriteScopeLock WriteLock(PointsLock);
			TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
			OutIndex = MutablePoints.Num();
			FPCGPoint& Pt = MutablePoints.Add_GetRef(FromPoint);
			InitPoint(Pt, FromPoint);
			return Pt;
		}

		FORCEINLINE FPCGPoint& NewPoint(int32& OutIndex) const
		{
			FWriteScopeLock WriteLock(PointsLock);
			TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
			FPCGPoint& Pt = MutablePoints.Emplace_GetRef();
			OutIndex = MutablePoints.Num() - 1;
			InitPoint(Pt);
			return Pt;
		}

		FORCEINLINE void AddPoint(FPCGPoint& Point, int32& OutIndex, bool bInit) const
		{
			FWriteScopeLock WriteLock(PointsLock);
			TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
			MutablePoints.Add(Point);
			OutIndex = MutablePoints.Num() - 1;
			if (bInit) { Out->Metadata->InitializeOnSet(Point.MetadataEntry); }
		}

		FORCEINLINE void AddPoint(FPCGPoint& Point, int32& OutIndex, const FPCGPoint& FromPoint) const
		{
			FWriteScopeLock WriteLock(PointsLock);
			TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
			MutablePoints.Add(Point);
			OutIndex = MutablePoints.Num() - 1;
			InitPoint(Point, FromPoint);
		}

		void InitializeNum(const int32 NumPoints, const bool bForceInit = false) const;

		void CleanupKeys();

		void Disable() { bEnabled = false; }
		void Enable() { bEnabled = true; }
		bool IsEnabled() const { return bEnabled; }

		bool OutputToContext();
		bool OutputToContext(const int32 MinPointCount, const int32 MaxPointCount);
	};

	/**
	 * 
	 */
	class PCGEXTENDEDTOOLKIT_API FPointIOCollection
	{
	protected:
		mutable FRWLock PairsLock;
		FPCGExContext* Context = nullptr;

	public:
		FPointIOCollection(FPCGExContext* InContext);
		FPointIOCollection(FPCGExContext* InContext, FName InputLabel, EInit InitOut = EInit::NoOutput);
		FPointIOCollection(FPCGExContext* InContext, TArray<FPCGTaggedData>& Sources, EInit InitOut = EInit::NoOutput);

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
			TArray<FPCGTaggedData>& Sources,
			EInit InitOut = EInit::NoOutput);

		FPointIO* Emplace_GetRef(const UPCGPointData* In, const EInit InitOut = EInit::NoOutput, const TSet<FString>* Tags = nullptr);
		FPointIO* Emplace_GetRef(EInit InitOut = EInit::NewOutput);
		FPointIO* Emplace_GetRef(const FPointIO* PointIO, const EInit InitOut = EInit::NoOutput);
		FPointIO* AddUnsafe(FPointIO* PointIO);
		void AddUnsafe(const TArray<FPointIO*>& IOs);


		template <typename T>
		FPointIO* Emplace_GetRef(const UPCGPointData* In, const EInit InitOut = EInit::NoOutput, const TSet<FString>* Tags = nullptr)
		{
			FWriteScopeLock WriteLock(PairsLock);
			FPointIO* NewIO = new FPointIO(Context, In);
			NewIO->SetInfos(Pairs.Add(NewIO), DefaultOutputLabel, Tags);
			NewIO->InitializeOutput<T>(InitOut);
			return NewIO;
		}

		template <typename T>
		FPointIO* Emplace_GetRef(EInit InitOut = EInit::NewOutput)
		{
			FWriteScopeLock WriteLock(PairsLock);
			FPointIO* NewIO = new FPointIO(Context);
			NewIO->SetInfos(Pairs.Add(NewIO), DefaultOutputLabel);
			NewIO->InitializeOutput<T>(InitOut);
			return NewIO;
		}

		template <typename T>
		FPointIO* Emplace_GetRef(const FPointIO* PointIO, const EInit InitOut = EInit::NoOutput)
		{
			FPointIO* Branch = Emplace_GetRef<T>(PointIO->GetIn(), InitOut);
			Branch->Tags->Reset(*PointIO->Tags);
			Branch->RootIO = const_cast<FPointIO*>(PointIO);
			return Branch;
		}

		bool IsEmpty() const { return Pairs.IsEmpty(); }
		int32 Num() const { return Pairs.Num(); }

		FPointIO& operator[](const int32 Index) const { return *Pairs[Index]; }

		void OutputToContext();
		void OutputToContext(const int32 MinPointCount, const int32 MaxPointCount);

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
	}

	static FPointIO* TryGetSingleInput(FPCGExContext* InContext, const FName InputPinLabel, const bool bThrowError)
	{
		FPointIO* SingleIO = nullptr;
		const FPointIOCollection* Collection = new FPointIOCollection(InContext, InputPinLabel);
		if (!Collection->Pairs.IsEmpty())
		{
			const FPointIO* Data = Collection->Pairs[0];
			SingleIO = new FPointIO(InContext, Data->GetIn());

			TSet<FString> TagDump;
			Data->Tags->Dump(TagDump);
			SingleIO->SetInfos(-1, InputPinLabel, &TagDump);
		}
		else if (bThrowError)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FText::FromString(TEXT("Missing {0} inputs")), FText::FromName(InputPinLabel)));
		}

		PCGEX_DELETE(Collection)
		return SingleIO;
	}
}
