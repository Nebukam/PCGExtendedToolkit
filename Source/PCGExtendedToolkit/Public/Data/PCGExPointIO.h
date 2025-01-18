// Copyright 2024 Timothé Lapetite and contributors
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
#include "PCGParamData.h"

#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

namespace PCGExData
{
	enum class EIOInit : uint8
	{
		None UMETA(DisplayName = "No Output"),
		New UMETA(DisplayName = "Create Empty Output Object"),
		Duplicate UMETA(DisplayName = "Duplicate Input Object"),
		Forward UMETA(DisplayName = "Forward Input Object")
	};

	enum class ESource : uint8
	{
		In,
		Out
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FPointRef
	{
		friend class FPointIO;

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

		FPointRef()
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
	class /*PCGEXTENDEDTOOLKIT_API*/ FPointIO : public TSharedFromThis<FPointIO>
	{
		friend class FPointIOCollection;

	protected:
		bool bTransactional = false;
		bool bMutable = false;
		FPCGExContext* Context = nullptr;
		TWeakPtr<PCGEx::FWorkPermit> WorkPermit;

		mutable FRWLock PointsLock;
		mutable FRWLock InKeysLock;
		mutable FRWLock OutKeysLock;
		mutable FRWLock AttributesLock;

		bool bWritten = false;
		int32 NumInPoints = -1;

		TSharedPtr<FPCGAttributeAccessorKeysPoints> InKeys; // Shared because reused by duplicates
		TSharedPtr<FPCGAttributeAccessorKeysPoints> OutKeys;

		const UPCGPointData* In = nullptr; // Input PointData	
		UPCGPointData* Out = nullptr;      // Output PointData

		TWeakPtr<FPointIO> RootIO;
		std::atomic<bool> bIsEnabled{true};

	public:
		TSharedPtr<FTags> Tags;
		int32 IOIndex = 0;

		bool bAllowEmptyOutput = false;

		explicit FPointIO(FPCGExContext* InContext):
			Context(InContext), WorkPermit(Context->GetWorkPermit()), In(nullptr)
		{
			PCGEX_LOG_CTR(FPointIO)
		}

		explicit FPointIO(FPCGExContext* InContext, const UPCGPointData* InData):
			Context(InContext), WorkPermit(Context->GetWorkPermit()), In(InData)
		{
			PCGEX_LOG_CTR(FPointIO)
		}

		explicit FPointIO(const TSharedRef<FPointIO>& InPointIO):
			Context(InPointIO->GetContext()), WorkPermit(InPointIO->WorkPermit), In(InPointIO->GetIn())
		{
			PCGEX_LOG_CTR(FPointIO)
			RootIO = InPointIO;

			TSet<FString> TagDump;
			InPointIO->Tags->DumpTo(TagDump); // Not ideal.

			Tags = MakeShared<FTags>(TagDump);
		}

		FPCGExContext* GetContext() const { return Context; }

		void SetInfos(const int32 InIndex,
		              const FName InOutputPin,
		              const TSet<FString>* InTags = nullptr);

		bool InitializeOutput(EIOInit InitOut = EIOInit::None);

		template <typename T>
		bool InitializeOutput(const EIOInit InitOut = EIOInit::None)
		{
			if (!WorkPermit.IsValid()) { return false; }
			if (IsValid(Out) && Out != In) { Context->ManagedObjects->Destroy(Out); }

			bMutable = true;

			if (InitOut == EIOInit::New)
			{
				T* TypedOut = Context->ManagedObjects->New<T>();

				Out = Cast<UPCGPointData>(TypedOut);
				check(Out)

				if (IsValid(In)) { Out->InitializeFromData(In); }
				const UPCGExPointData* TypedPointData = Cast<UPCGExPointData>(In);
				UPCGExPointData* TypedOutPointData = Cast<UPCGExPointData>(TypedOut);
				if (TypedPointData && TypedOutPointData) { TypedOutPointData->InitializeFromPCGExData(TypedPointData, EIOInit::New); }

				return true;
			}

			if (InitOut == EIOInit::Duplicate)
			{
				check(In)
				const T* TypedIn = Cast<T>(In);

				if (!TypedIn)
				{
					T* TypedOut = Context->ManagedObjects->New<T>();

					if (UPCGExPointData* TypedPointData = Cast<UPCGExPointData>(TypedOut)) { TypedPointData->CopyFrom(In); }
					else { TypedOut->InitializeFromData(In); } // This is a potentially failed duplicate

					Out = Cast<UPCGPointData>(TypedOut);
				}
				else
				{
					Out = Context->ManagedObjects->Duplicate<UPCGPointData>(In);
				}

				return true;
			}

			return InitializeOutput(InitOut);
		}

		~FPointIO();

		FORCEINLINE bool IsDataValid(const ESource InSource) const { return InSource == ESource::In ? IsValid(In) : IsValid(Out); }

		FORCEINLINE const UPCGPointData* GetData(const ESource InSource) const { return InSource == ESource::In ? In : Out; }
		FORCEINLINE UPCGPointData* GetMutableData(const ESource InSource) const { return const_cast<UPCGPointData*>(InSource == ESource::In ? In : Out); }
		FORCEINLINE const UPCGPointData* GetIn() const { return In; }
		FORCEINLINE UPCGPointData* GetOut() const { return Out; }
		FORCEINLINE const UPCGPointData* GetOutIn() const { return Out ? Out : In; }
		FORCEINLINE const UPCGPointData* GetInOut() const { return In ? In : Out; }

		FORCEINLINE int32 GetNum() const { return In ? In->GetPoints().Num() : Out ? Out->GetPoints().Num() : -1; }
		FORCEINLINE int32 GetNum(const ESource Source) const { return Source == ESource::In ? In->GetPoints().Num() : Out->GetPoints().Num(); }
		FORCEINLINE int32 GetOutInNum() const { return Out && !Out->GetPoints().IsEmpty() ? Out->GetPoints().Num() : In ? In->GetPoints().Num() : -1; }

		TSharedPtr<FPCGAttributeAccessorKeysPoints> GetInKeys();
		TSharedPtr<FPCGAttributeAccessorKeysPoints> GetOutKeys(const bool bEnsureValidKeys = false);
		void PrintOutKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap) const;

		FName OutputPin = PCGEx::OutputPointsLabel;

		FORCEINLINE const PCGMetadataEntryKey& GetInItemKey(const int32 Index) const { return (In->GetPoints().GetData() + Index)->MetadataEntry; }
		FORCEINLINE const PCGMetadataEntryKey& GetOutItemKey(const int32 Index) const { return (Out->GetPoints().GetData() + Index)->MetadataEntry; }

		FORCEINLINE TArray<FPCGPoint>& GetMutablePoints() const { return Out->GetMutablePoints(); }

		FORCEINLINE const FPCGPoint& GetInPoint(const int32 Index) const { return *(In->GetPoints().GetData() + Index); }
		FORCEINLINE const FPCGPoint& GetOutPoint(const int32 Index) const { return *(Out->GetPoints().GetData() + Index); }
		FORCEINLINE FPCGPoint& GetMutablePoint(const int32 Index) const { return *(Out->GetMutablePoints().GetData() + Index); }
		FORCEINLINE const TArray<FPCGPoint>& GetPoints(const ESource Source) const { return Source == ESource::In ? In->GetPoints() : Out->GetPoints(); }

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

		FORCEINLINE void AddPoint(FPCGPoint& Point, int32& OutIndex, const bool bInit) const
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

		void CleanupKeys();

		void Disable() { bIsEnabled.store(false, std::memory_order_release); }
		void Enable() { bIsEnabled.store(true, std::memory_order_release); }
		bool IsEnabled() const { return bIsEnabled.load(std::memory_order_acquire); }

		bool StageOutput() const;
		bool StageOutput(const int32 MinPointCount, const int32 MaxPointCount) const;

		void DeleteAttribute(FName AttributeName) const;

		template <typename T>
		FPCGMetadataAttribute<T>* CreateAttribute(FName AttributeName, const T& DefaultValue, bool bAllowsInterpolation, bool bOverrideParent)
		{
			FPCGMetadataAttribute<T>* OutAttribute = nullptr;
			if (!Out) { return OutAttribute; }

			{
				FWriteScopeLock WriteScopeLock(AttributesLock);
				OutAttribute = Out->Metadata->CreateAttribute(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent);
			}

			return OutAttribute;
		}

		template <typename T>
		FPCGMetadataAttribute<T>* FindOrCreateAttribute(FName AttributeName, const T& DefaultValue = T{}, bool bAllowsInterpolation = true, bool bOverrideParent = true, bool bOverwriteIfTypeMismatch = true)
		{
			FPCGMetadataAttribute<T>* OutAttribute = nullptr;
			if (!Out) { return OutAttribute; }

			{
				FWriteScopeLock WriteScopeLock(AttributesLock);
				OutAttribute = Out->Metadata->FindOrCreateAttribute(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent, bOverwriteIfTypeMismatch);
			}

			return OutAttribute;
		}
	};

	static TSharedPtr<FPointIO> NewPointIO(FPCGExContext* InContext, FName InOutputPin = NAME_None, int32 Index = -1)
	{
		PCGEX_MAKE_SHARED(NewIO, FPointIO, InContext)
		NewIO->SetInfos(Index, InOutputPin);
		return NewIO;
	}

	static TSharedPtr<FPointIO> NewPointIO(FPCGExContext* InContext, const UPCGPointData* InData, FName InOutputPin = NAME_None, int32 Index = -1)
	{
		PCGEX_MAKE_SHARED(NewIO, FPointIO, InContext, InData)
		NewIO->SetInfos(Index, InOutputPin);
		return NewIO;
	}

	static TSharedPtr<FPointIO> NewPointIO(const TSharedRef<FPointIO>& InPointIO, FName InOutputPin = NAME_None, int32 Index = -1)
	{
		PCGEX_MAKE_SHARED(NewIO, FPointIO, InPointIO)
		NewIO->SetInfos(Index, InOutputPin);
		return NewIO;
	}

	/**
	 * 
	 */
	class /*PCGEXTENDEDTOOLKIT_API*/ FPointIOCollection : public TSharedFromThis<FPointIOCollection>
	{
	protected:
		mutable FRWLock PairsLock;
		FPCGExContext* Context = nullptr;
		bool bTransactional = false;

	public:
		explicit FPointIOCollection(FPCGExContext* InContext, bool bIsTransactional = false);
		FPointIOCollection(FPCGExContext* InContext, FName InputLabel, EIOInit InitOut = EIOInit::None, bool bIsTransactional = false);
		FPointIOCollection(FPCGExContext* InContext, TArray<FPCGTaggedData>& Sources, EIOInit InitOut = EIOInit::None, bool bIsTransactional = false);

		~FPointIOCollection();

		FName OutputPin = PCGEx::OutputPointsLabel;
		TArray<TSharedPtr<FPointIO>> Pairs;

		/**
		 * Initialize from Sources
		 * @param Sources 
		 * @param InitOut 
		 */
		void Initialize(
			TArray<FPCGTaggedData>& Sources,
			EIOInit InitOut = EIOInit::None);

		TSharedPtr<FPointIO> Emplace_GetRef(const UPCGPointData* In, const EIOInit InitOut = EIOInit::None, const TSet<FString>* Tags = nullptr);
		TSharedPtr<FPointIO> Emplace_GetRef(EIOInit InitOut = EIOInit::New);
		TSharedPtr<FPointIO> Emplace_GetRef(const TSharedPtr<FPointIO>& PointIO, const EIOInit InitOut = EIOInit::None);
		TSharedPtr<FPointIO> Insert_Unsafe(const int32 Index, const TSharedPtr<FPointIO>& PointIO);
		TSharedPtr<FPointIO> Add_Unsafe(const TSharedPtr<FPointIO>& PointIO);
		void Add_Unsafe(const TArray<TSharedPtr<FPointIO>>& IOs);


		template <typename T>
		TSharedPtr<FPointIO> Emplace_GetRef(const UPCGPointData* In, const EIOInit InitOut = EIOInit::None, const TSet<FString>* Tags = nullptr)
		{
			FWriteScopeLock WriteLock(PairsLock);
			TSharedPtr<FPointIO> NewIO = Pairs.Add_GetRef(MakeShared<FPointIO>(Context, In));
			NewIO->SetInfos(Pairs.Num() - 1, OutputPin, Tags);
			if (!NewIO->InitializeOutput<T>(InitOut)) { return nullptr; }
			return NewIO;
		}

		template <typename T>
		TSharedPtr<FPointIO> Emplace_GetRef(const EIOInit InitOut = EIOInit::New)
		{
			FWriteScopeLock WriteLock(PairsLock);
			TSharedPtr<FPointIO> NewIO = Pairs.Add_GetRef(MakeShared<FPointIO>(Context));
			NewIO->SetInfos(Pairs.Num() - 1, OutputPin);
			if (!NewIO->InitializeOutput<T>(InitOut)) { return nullptr; }
			return NewIO;
		}

		template <typename T>
		TSharedPtr<FPointIO> Emplace_GetRef(const TSharedPtr<FPointIO>& PointIO, const EIOInit InitOut = EIOInit::None)
		{
			TSharedPtr<FPointIO> Branch = Emplace_GetRef<T>(PointIO->GetIn(), InitOut);
			if (!Branch) { return nullptr; }

			Branch->Tags->Reset(PointIO->Tags);
			Branch->RootIO = PointIO;
			return Branch;
		}

		bool IsEmpty() const { return Pairs.IsEmpty(); }
		int32 Num() const { return Pairs.Num(); }

		TSharedPtr<FPointIO> operator[](const int32 Index) const { return Pairs[Index]; }

		void IncreaseReserve(int32 InIncreaseNum);

		void StageOutputs();
		void StageOutputs(const int32 MinPointCount, const int32 MaxPointCount);

		void Sort();

		FBox GetInBounds() const;
		FBox GetOutBounds() const;

		void PruneNullEntries(const bool bUpdateIndices);

		void Flush();
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FPointIOTaggedEntries : public TSharedFromThis<FPointIOTaggedEntries>
	{
	public:
		FString TagId;
		IDType TagValue;
		TArray<TSharedRef<FPointIO>> Entries;

		FPointIOTaggedEntries(const FString& InTagId, const IDType& InTagValue)
			: TagId(InTagId), TagValue(InTagValue)
		{
		}

		~FPointIOTaggedEntries() = default;

		void Add(const TSharedRef<FPointIO>& Value);
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FPointIOTaggedDictionary
	{
	public:
		FString TagId;             // LeftSide
		TMap<int32, int32> TagMap; // :RightSize @ Entry Index
		TArray<TSharedPtr<FPointIOTaggedEntries>> Entries;

		explicit FPointIOTaggedDictionary(const FString& InTagId)
			: TagId(InTagId)
		{
		}

		~FPointIOTaggedDictionary() = default;

		bool CreateKey(const TSharedRef<FPointIO>& PointIOKey);
		bool TryAddEntry(const TSharedRef<FPointIO>& PointIOEntry);
		TSharedPtr<FPointIOTaggedEntries> GetEntries(int32 Key);
	};

	namespace PCGExPointIO
	{
		static const UPCGPointData* GetPointData(const FPCGContext* Context, const FPCGTaggedData& Source)
		{
			// This is actually creating a transient In that is potentially a new data, this is super bad
			//const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Source.Data);
			//if (!SpatialData) { return nullptr; }

			//const UPCGPointData* PointData = SpatialData->ToPointData(const_cast<FPCGContext*>(Context));
			const UPCGPointData* PointData = Cast<const UPCGPointData>(Source.Data);
			if (!PointData) { return nullptr; }

			return PointData;
		}

		static UPCGPointData* GetMutablePointData(const FPCGContext* Context, const FPCGTaggedData& Source)
		{
			const UPCGPointData* PointData = GetPointData(Context, Source);
			if (!PointData) { return nullptr; }

			return const_cast<UPCGPointData*>(PointData);
		}

		static const UPCGPointData* ToPointData(FPCGExContext* Context, const FPCGTaggedData& Source)
		{
			// NOTE : This has a high probability of creating new data on the fly
			// so it should absolutely not be used to be inherited or duplicated
			// since it would mean point data that inherit potentially destroyed parents
			if (const UPCGPointData* RealPointData = Cast<const UPCGPointData>(Source.Data))
			{
				return RealPointData;
			}
			if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Source.Data))
			{
				// Currently we support collapsing to point data only, but at some point in the future that might be different
#if PCGEX_ENGINE_VERSION < 505
				const UPCGPointData* PointData = Cast<const UPCGSpatialData>(SpatialData)->ToPointData();
#else
				const UPCGPointData* PointData = Cast<const UPCGSpatialData>(SpatialData)->ToPointData(Context);
#endif

				//Keep track of newly created data internally
				if (PointData != SpatialData) { Context->ManagedObjects->Add(const_cast<UPCGPointData*>(PointData)); }
				return PointData;
			}
#if PCGEX_ENGINE_VERSION > 503
			if (const UPCGParamData* ParamData = Cast<UPCGParamData>(Source.Data))
			{
				const UPCGMetadata* ParamMetadata = ParamData->Metadata;

				const int64 ParamItemCount = ParamMetadata->GetLocalItemCount();
				if (ParamItemCount == 0) { return nullptr; }

				UPCGPointData* PointData = Context->ManagedObjects->New<UPCGPointData>();
				check(PointData->Metadata);
				PointData->Metadata->Initialize(ParamMetadata);

				TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
				Points.SetNum(ParamItemCount);

				for (int PointIndex = 0; PointIndex < ParamItemCount; ++PointIndex)
				{
					Points[PointIndex].MetadataEntry = PointIndex;
				}

				return PointData;
			}
#endif

			return nullptr;
		}
	}

	static TSharedPtr<FPointIO> TryGetSingleInput(FPCGExContext* InContext, const FName InputPinLabel, const bool bThrowError)
	{
		TSharedPtr<FPointIO> SingleIO;
		const TSharedPtr<FPointIOCollection> Collection = MakeShared<FPointIOCollection>(InContext, InputPinLabel);

		if (!Collection->Pairs.IsEmpty() && Collection->Pairs[0]->GetNum() > 0)
		{
			SingleIO = Collection->Pairs[0];
		}
		else if (bThrowError)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FText::FromString(TEXT("Missing or zero-points '{0}' inputs")), FText::FromName(InputPinLabel)));
		}

		return SingleIO;
	}
}
