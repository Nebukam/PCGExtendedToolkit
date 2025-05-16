// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Metadata/Accessors//PCGCustomAccessor.h"
#include "IndexTypes.h"
#include "PCGElement.h"
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

	struct PCGEXTENDEDTOOLKIT_API FPoint
	{
		int32 Index = -1;
		int32 IO = -1;

		constexpr FPoint() = default;

		constexpr explicit FPoint(const uint64 Hash)
			: Index(PCGEx::H64A(Hash)), IO(PCGEx::H64A(Hash))
		{
		}

		constexpr FPoint(const uint32 InNode, const uint32 InEdge)
			: Index(InNode), IO(InEdge)
		{
		}

		FORCEINLINE uint64 H64() const { return PCGEx::H64U(Index, IO); }

		bool operator==(const FPoint& Other) const { return Index == Other.Index && IO == Other.IO; }
		FORCEINLINE friend uint32 GetTypeHash(const FPoint& Key) { return HashCombineFast(Key.Index, Key.IO); }
	};

	struct PCGEXTENDEDTOOLKIT_API FPointRef
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

		FPCGPoint& MutablePoint() const { return const_cast<FPCGPoint&>(*Point); }
	};

	/**
	 * 
	 */
	class PCGEXTENDEDTOOLKIT_API FPointIO : public TSharedFromThis<FPointIO>
	{
		friend class FPointIOCollection;

	protected:
		bool bTransactional = false;
		bool bMutable = false;
		TWeakPtr<FPCGContextHandle> ContextHandle;

		mutable FRWLock PointsLock;
		mutable FRWLock InKeysLock;
		mutable FRWLock OutKeysLock;
		mutable FRWLock AttributesLock;

		bool bWritten = false;
		int32 NumInPoints = -1;

		TSharedPtr<FPCGAttributeAccessorKeysPointIndices> InKeys; // Shared because reused by duplicates
		TSharedPtr<FPCGAttributeAccessorKeysPointIndices> OutKeys;

		const UPCGBasePointData* In = nullptr; // Input PointData	
		UPCGBasePointData* Out = nullptr;      // Output PointData

		TWeakPtr<FPointIO> RootIO;
		std::atomic<bool> bIsEnabled{true};

	public:
		TSharedPtr<FTags> Tags;
		int32 IOIndex = 0;
		int32 InitializationIndex = -1;
		TObjectPtr<const UPCGData> InitializationData = nullptr;

		bool bAllowEmptyOutput = false;

		explicit FPointIO(const TWeakPtr<FPCGContextHandle>& InContextHandle):
			ContextHandle(InContextHandle), In(nullptr)
		{
			PCGEX_LOG_CTR(FPointIO)
		}

		explicit FPointIO(const TWeakPtr<FPCGContextHandle>& InContextHandle, const UPCGBasePointData* InData):
			ContextHandle(InContextHandle), In(InData)
		{
			PCGEX_LOG_CTR(FPointIO)
		}

		explicit FPointIO(const TSharedRef<FPointIO>& InPointIO):
			ContextHandle(InPointIO->GetContext()), In(InPointIO->GetIn())
		{
			PCGEX_LOG_CTR(FPointIO)
			RootIO = InPointIO;

			TSet<FString> TagDump;
			InPointIO->Tags->DumpTo(TagDump); // Not ideal.

			Tags = MakeShared<FTags>(TagDump);
		}

		TWeakPtr<FPCGContextHandle> GetContext() const { return ContextHandle; }

		void SetInfos(const int32 InIndex,
		              const FName InOutputPin,
		              const TSet<FString>* InTags = nullptr);

		bool InitializeOutput(EIOInit InitOut = EIOInit::None);

		template <typename T>
		bool InitializeOutput(const EIOInit InitOut = EIOInit::None)
		{
			FPCGContext::FSharedContext<FPCGExContext> SharedContext(ContextHandle);

			if (!SharedContext.Get()) { return false; }
			if (IsValid(Out) && Out != In)
			{
				SharedContext.Get()->ManagedObjects->Destroy(Out);
				Out = nullptr;
			}

			bMutable = true;


			if (InitOut == EIOInit::New)
			{
				T* TypedOut = SharedContext.Get()->ManagedObjects->New<T>();
				Out = Cast<UPCGBasePointData>(TypedOut);

				check(Out)

				if (IsValid(In)) { Out->InitializeFromData(In); }

				return true;
			}

			if (InitOut == EIOInit::Duplicate)
			{
				check(In)
				const T* TypedIn = Cast<T>(In);

				if (!TypedIn)
				{
					T* TypedOut = SharedContext.Get()->ManagedObjects->DuplicateData<T>(In);
					Out = Cast<UPCGBasePointData>(TypedOut);
				}
				else
				{
					SharedContext.Get()->ManagedObjects->New<T>();
					Out->InitializeFromData(In);
				}

				return true;
			}

			return InitializeOutput(InitOut);
		}

		~FPointIO();

		bool IsDataValid(const ESource InSource) const { return InSource == ESource::In ? IsValid(In) : IsValid(Out); }

		FORCEINLINE const UPCGBasePointData* GetData(const ESource InSource) const { return InSource == ESource::In ? In : Out; }
		FORCEINLINE UPCGBasePointData* GetMutableData(const ESource InSource) const { return const_cast<UPCGBasePointData*>(InSource == ESource::In ? In : Out); }
		FORCEINLINE const UPCGBasePointData* GetIn() const { return In; }
		FORCEINLINE UPCGBasePointData* GetOut() const { return Out; }
		FORCEINLINE const UPCGBasePointData* GetOutIn() const { return Out ? Out : In; }
		FORCEINLINE const UPCGBasePointData* GetInOut() const { return In ? In : Out; }
		const UPCGBasePointData* GetOutIn(ESource& OutSource) const;
		const UPCGBasePointData* GetInOut(ESource& OutSource) const;
		bool GetSource(const UPCGData* InData, ESource& OutSource) const;

		int32 GetNum() const { return In ? In->GetNumPoints : Out ? Out->GetNumPoints() : -1; }
		int32 GetNum(const ESource Source) const { return Source == ESource::In ? In->GetNumPoints() : Out->GetNumPoints(); }
		int32 GetOutInNum() const { return Out && !Out->GetNumPoints() ? Out->GetNumPoints() : In ? In->GetNumPoints() : -1; }

		TSharedPtr<FPCGAttributeAccessorKeysPointIndices> GetInKeys();
		TSharedPtr<FPCGAttributeAccessorKeysPointIndices> GetOutKeys(const bool bEnsureValidKeys = false);
		void PrintOutKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap) const;
		void PrintInKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap) const;
		void PrintOutInKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap) const;

		FName OutputPin = PCGEx::OutputPointsLabel;

		void InitPoint(FPCGPoint& Point, const PCGMetadataEntryKey ParentKey) const
		{
			Out->Metadata->InitializeOnSet(Point.MetadataEntry, ParentKey, In->Metadata);
		}

		void InitPoint(FPCGPoint& Point) const
		{
			Out->Metadata->InitializeOnSet(Point.MetadataEntry);
		}

		FPCGPoint& CopyPoint(const FPCGPoint& FromPoint, int32& OutIndex) const
		{
			FWriteScopeLock WriteLock(PointsLock);
			TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
			OutIndex = MutablePoints.Num();
			FPCGPoint& Pt = MutablePoints.Add_GetRef(FromPoint);
			InitPoint(Pt, FromPoint.MetadataEntry);
			return Pt;
		}

		FPCGPoint& NewPoint(int32& OutIndex) const
		{
			FWriteScopeLock WriteLock(PointsLock);
			TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
			FPCGPoint& Pt = MutablePoints.Emplace_GetRef();
			OutIndex = MutablePoints.Num() - 1;
			InitPoint(Pt);
			return Pt;
		}

		void AddPoint(FPCGPoint& Point, int32& OutIndex, const bool bInit) const
		{
			FWriteScopeLock WriteLock(PointsLock);
			TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
			MutablePoints.Add(Point);
			OutIndex = MutablePoints.Num() - 1;
			if (bInit) { Out->Metadata->InitializeOnSet(Point.MetadataEntry); }
		}

		void AddPoint(FPCGPoint& Point, int32& OutIndex, const FPCGPoint& FromPoint) const
		{
			FWriteScopeLock WriteLock(PointsLock);
			TArray<FPCGPoint>& MutablePoints = Out->GetMutablePoints();
			MutablePoints.Add(Point);
			OutIndex = MutablePoints.Num() - 1;
			InitPoint(Point, FromPoint.MetadataEntry);
		}

		void CleanupKeys();

		void Disable() { bIsEnabled.store(false, std::memory_order_release); }
		void Enable() { bIsEnabled.store(true, std::memory_order_release); }
		bool IsEnabled() const { return bIsEnabled.load(std::memory_order_acquire); }

		bool StageOutput(FPCGExContext* TargetContext) const;
		bool StageOutput(FPCGExContext* TargetContext, const int32 MinPointCount, const int32 MaxPointCount) const;
		bool StageAnyOutput(FPCGExContext* TargetContext) const;

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

	static TSharedPtr<FPointIO> NewPointIO(FPCGExContext* InContext, const UPCGBasePointData* InData, FName InOutputPin = NAME_None, int32 Index = -1)
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
	class PCGEXTENDEDTOOLKIT_API FPointIOCollection : public TSharedFromThis<FPointIOCollection>
	{
	protected:
		mutable FRWLock PairsLock;
		TWeakPtr<FPCGContextHandle> ContextHandle;
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

		TSharedPtr<FPointIO> Emplace_GetRef(const UPCGBasePointData* In, const EIOInit InitOut = EIOInit::None, const TSet<FString>* Tags = nullptr);
		TSharedPtr<FPointIO> Emplace_GetRef(EIOInit InitOut = EIOInit::New);
		TSharedPtr<FPointIO> Emplace_GetRef(const TSharedPtr<FPointIO>& PointIO, const EIOInit InitOut = EIOInit::None);
		TSharedPtr<FPointIO> Insert_Unsafe(const int32 Index, const TSharedPtr<FPointIO>& PointIO);
		TSharedPtr<FPointIO> Add_Unsafe(const TSharedPtr<FPointIO>& PointIO);
		void Add_Unsafe(const TArray<TSharedPtr<FPointIO>>& IOs);


		template <typename T>
		TSharedPtr<FPointIO> Emplace_GetRef(const UPCGBasePointData* In, const EIOInit InitOut = EIOInit::None, const TSet<FString>* Tags = nullptr)
		{
			FWriteScopeLock WriteLock(PairsLock);
			TSharedPtr<FPointIO> NewIO = Pairs.Add_GetRef(MakeShared<FPointIO>(ContextHandle, In));
			NewIO->SetInfos(Pairs.Num() - 1, OutputPin, Tags);
			if (!NewIO->InitializeOutput<T>(InitOut)) { return nullptr; }
			return NewIO;
		}

		template <typename T>
		TSharedPtr<FPointIO> Emplace_GetRef(const EIOInit InitOut = EIOInit::New)
		{
			FWriteScopeLock WriteLock(PairsLock);
			TSharedPtr<FPointIO> NewIO = Pairs.Add_GetRef(MakeShared<FPointIO>(ContextHandle));
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
		void StageAnyOutputs();

		void Sort();

		FBox GetInBounds() const;
		FBox GetOutBounds() const;

		void PruneNullEntries(const bool bUpdateIndices);

		void Flush();
	};

	class PCGEXTENDEDTOOLKIT_API FPointIOTaggedEntries : public TSharedFromThis<FPointIOTaggedEntries>
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

	class PCGEXTENDEDTOOLKIT_API FPointIOTaggedDictionary
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
		int32 GetTotalPointsNum(const TArray<TSharedPtr<FPointIO>>& InIOs, const ESource InSource = ESource::In);

		static const UPCGBasePointData* GetPointData(const FPCGContext* Context, const FPCGTaggedData& Source)
		{
			const UPCGBasePointData* PointData = Cast<const UPCGBasePointData>(Source.Data);
			if (!PointData) { return nullptr; }

			return PointData;
		}

		static UPCGBasePointData* GetMutablePointData(const FPCGContext* Context, const FPCGTaggedData& Source)
		{
			const UPCGBasePointData* PointData = GetPointData(Context, Source);
			if (!PointData) { return nullptr; }

			return const_cast<UPCGBasePointData*>(PointData);
		}

		static const UPCGBasePointData* ToPointData(FPCGExContext* Context, const FPCGTaggedData& Source)
		{
			// NOTE : This has a high probability of creating new data on the fly
			// so it should absolutely not be used to be inherited or duplicated
			// since it would mean point data that inherit potentially destroyed parents
			if (const UPCGBasePointData* RealPointData = Cast<const UPCGBasePointData>(Source.Data))
			{
				return RealPointData;
			}
			if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Source.Data))
			{
				// Currently we support collapsing to point data only, but at some point in the future that might be different
				const UPCGBasePointData* PointData = Cast<const UPCGSpatialData>(SpatialData)->ToPointData(Context);

				//Keep track of newly created data internally
				if (PointData != SpatialData) { Context->ManagedObjects->Add(const_cast<UPCGBasePointData*>(PointData)); }
				return PointData;
			}

			if (const UPCGParamData* ParamData = Cast<UPCGParamData>(Source.Data))
			{
				const UPCGMetadata* ParamMetadata = ParamData->Metadata;

				const int64 ParamItemCount = ParamMetadata->GetLocalItemCount();
				if (ParamItemCount == 0) { return nullptr; }

				UPCGBasePointData* PointData = Context->ManagedObjects->New<PCGEX_NEW_POINT_DATA_TYPE>();
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

			return nullptr;
		}
	}

	static TSharedPtr<FPointIO> TryGetSingleInput(FPCGExContext* InContext, const FName InputPinLabel, const bool bTransactional, const bool bThrowError)
	{
		TSharedPtr<FPointIO> SingleIO;
		const TSharedPtr<FPointIOCollection> Collection = MakeShared<FPointIOCollection>(InContext, InputPinLabel, EIOInit::None, bTransactional);

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
