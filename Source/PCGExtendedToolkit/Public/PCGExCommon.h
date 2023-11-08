// Fill out your copyright notice in the Description page of Project Rules.

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "PCGExCommon.generated.h"

#define PCGEX_FOREACH_POINTPROPERTY(MACRO)\
MACRO(EPCGPointProperties::Density, Density) \
MACRO(EPCGPointProperties::BoundsMin, BoundsMin) \
MACRO(EPCGPointProperties::BoundsMax, BoundsMax) \
MACRO(EPCGPointProperties::Extents, GetExtents()) \
MACRO(EPCGPointProperties::Color, Color) \
MACRO(EPCGPointProperties::Position, Transform.GetLocation()) \
MACRO(EPCGPointProperties::Rotation, Transform.Rotator()) \
MACRO(EPCGPointProperties::Scale, Transform.GetScale3D()) \
MACRO(EPCGPointProperties::Transform, Transform) \
MACRO(EPCGPointProperties::Steepness, Steepness) \
MACRO(EPCGPointProperties::LocalCenter, GetLocalCenter()) \
MACRO(EPCGPointProperties::Seed, Seed)
#define PCGEX_FOREACH_POINTEXTRAPROPERTY(MACRO)\
MACRO(EPCGExtraProperties::Index, MetadataEntry)

UENUM(BlueprintType)
enum class EPCGExComponentSelection : uint8
{
	X UMETA(DisplayName = "X"),
	Y UMETA(DisplayName = "Y (→x)"),
	Z UMETA(DisplayName = "Z (→y)"),
	W UMETA(DisplayName = "W (→z)"),
	XYZ UMETA(DisplayName = "X→Y→Z"),
	XZY UMETA(DisplayName = "X→Z→Y"),
	YXZ UMETA(DisplayName = "Y→X→Z"),
	YZX UMETA(DisplayName = "Y→Z→X"),
	ZXY UMETA(DisplayName = "Z→X→Y"),
	ZYX UMETA(DisplayName = "Z→Y→X"),
	Length UMETA(DisplayName = "Length"),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSelectorSettingsBase
{
	GENERATED_BODY()

	FPCGExSelectorSettingsBase()
	{
		bFixed = false;
	}

	FPCGExSelectorSettingsBase(const FPCGExSelectorSettingsBase& Other)
	{
		Selector = Other.Selector;
		ComponentSelection = Other.ComponentSelection;
		Attribute = Other.Attribute;
		bFixed = false;
	}

public:
	/** Name of the attribute to compare */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector Selector;

	/** Sub-sorting order, used only for multi-field attributes (FVector, FRotator etc). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExComponentSelection ComponentSelection = EPCGExComponentSelection::XYZ;

	/* Can hold a reference to the attribute pointer, if prepared like so */
	FPCGMetadataAttributeBase* Attribute = nullptr;

	/* Whether CopyAndFixLast has been called*/
	bool bFixed = false;

	bool IsValid(const UPCGPointData* PointData) const
	{
		const EPCGAttributePropertySelection Sel = Selector.GetSelection();
		if (Sel == EPCGAttributePropertySelection::Attribute && (Attribute == nullptr || !PointData->Metadata->HasAttribute(Selector.GetName()))) { return false; }
		return Selector.IsValid();
	}

	template <typename T>
	bool TryGetValue(int64 Key, T& OutValue)
	{
		if (Attribute == nullptr) { return false; }
		OutValue = static_cast<FPCGMetadataAttribute<T>*>(Attribute)->GetScale(Key);
		return true;
	}

	bool CopyAndFixLast(const UPCGPointData* InData)
	{
		// Selector pointer is valid but points to shit
		bFixed = true;
		Selector = Selector.CopyAndFixLast(InData);
		if (Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			Attribute = Selector.IsValid() ? InData->Metadata->GetMutableAttribute(Selector.GetName()) : nullptr;
			return Attribute != nullptr;
		}
		else
		{
			return Selector.IsValid();
		}
	}

	FString ToString() const
	{
		return Selector.GetName().ToString();
	}

	~FPCGExSelectorSettingsBase()
	{
		Attribute = nullptr;
	}
};


USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExPointDataIO
{
	GENERATED_BODY()

public:
	FPCGTaggedData* Source = nullptr; // Source struct
	UPCGPointData* In = nullptr; // Input PointData
	FPCGTaggedData* Output = nullptr; // Source struct
	UPCGPointData* Out = nullptr; // Output PointData

	/**
	 * 
	 * @param Context 
	 * @param bForwardOnly Only initialize output if there is an existing input
	 * @return 
	 */
	bool InitializeOut(FPCGContext* Context, bool bForwardOnly = true)
	{
		if (bForwardOnly && (!In || !Source)) { return false; }
		Out = NewObject<UPCGPointData>();
		if (In && Source) { Out->InitializeFromData(In); }
		return true;
	}

	/**
	 * Write valid outputs to Context' tagged data
	 * @param Context 
	 * @param bEmplace if false (default), will try to use the source first
	 */
	void OutputTo(FPCGContext* Context, bool bEmplace = false)
	{
		if (!Out) { return; }
		if (Source && !bEmplace) { Output = &(Context->OutputData.TaggedData.Add_GetRef(*Source)); }
		else if(bEmplace){ Output = &(Context->OutputData.TaggedData.Emplace_GetRef()); }

		if(!Output)
		{
			UE_LOG(LogTemp, Error, TEXT("FPCGExPointDataIO::OutputTo => bEmplace=false but there is no source!"));
			return;
		}
		
		Output->Data = Out;
	}

	/**
	 * Copy In.Points to Out.Points
	 * @param Context 
	 * @return 
	 */
	bool ForwardPoints(FPCGContext* Context)
	{
		if (!Out || !In) { return false; }

		TArray<FPCGPoint>& OutPoints = Out->GetMutablePoints();
		auto CopyPoint = [](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
		{
			OutPoint = InPoint;
			return true;
		};

		FPCGAsync::AsyncPointProcessing(Context, In->GetPoints(), OutPoints, CopyPoint);
		return true;
	}

	/**
	 * Copy In.Points to Out.Points with a callback after each copy
	 * @param Context 
	 * @param PointFunc 
	 * @return 
	 */
	bool ForwardPoints(FPCGContext* Context, const TFunction<void(FPCGPoint&, const int32)>& PointFunc)
	{
		if (!Out || !In) { return false; }

		int PointIndex = 0;
		TArray<FPCGPoint>& OutPoints = Out->GetMutablePoints();
		auto CopyPoint = [&PointIndex, &PointFunc, this](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
		{
			OutPoint = InPoint;
			PointFunc(OutPoint, PointIndex);
			PointIndex++;
			return true;
		};

		FPCGAsync::AsyncPointProcessing(Context, In->GetPoints(), OutPoints, CopyPoint);

		return true;
	}

	~FPCGExPointDataIO()
	{
		Source = nullptr;
		In = nullptr;
		Output = nullptr;
		Out = nullptr;
	}
	
};

struct PCGEXTENDEDTOOLKIT_API FPCGExIndexedPointDataIO : public FPCGExPointDataIO
{
public:
	TMap<uint64, int32> Indices; //MetadaEntry::Index, based on Input points (output MetadataEntry will be offset)

	FPCGExIndexedPointDataIO()
	{
		Indices.Empty();
	}
	
	/**
	 * Copy In.Points to Out.Points and build the Indices map
	 * @param Context 
	 * @return 
	 */
	bool ForwardPointsIndexed(FPCGContext* Context)
	{
		return ForwardPoints(Context, [this](FPCGPoint& Point, const int32 Index)
		{
			Indices.Add(Point.MetadataEntry, Index);
		});
	}

	/**
	 * Copy In.Points to Out.Points and build the Indices map with a callback after each copy
	 * @param Context 
	 * @param PointFunc 
	 * @return 
	 */
	bool ForwardPointsIndexed(FPCGContext* Context, const TFunction<void(FPCGPoint&, const int32)>& PointFunc)
	{
		return ForwardPoints(Context, [&PointFunc, this](FPCGPoint& Point, const int32 Index)
		{
			Indices.Add(Point.MetadataEntry, Index);
			PointFunc(Point, Index);
		});
	}

	int64 GetIndex(int64 Key) { return *(Indices.Find(Key)); }

	~FPCGExIndexedPointDataIO()
	{
		Indices.Empty();
	}
	
};

template <typename T>
struct PCGEXTENDEDTOOLKIT_API FPCGExPointIOMap
{
	FPCGExPointIOMap()
	{
		Pairs.Empty();
	}

	FPCGExPointIOMap(FPCGContext* Context, FName InputLabel, bool bInitializeOutput = false): FPCGExPointIOMap()
	{
		TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(InputLabel);
		Initialize(Context, Sources, bInitializeOutput);
	}

	FPCGExPointIOMap(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, bool bInitializeOutput = false): FPCGExPointIOMap()
	{
		Initialize(Context, Sources, bInitializeOutput);
	}

public:
	TArray<T> Pairs;
	
	/**
	 * Initialize from Sources
	 * @param Context 
	 * @param Sources 
	 * @param bInitializeOutput 
	 */
	void Initialize(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, bool bInitializeOutput = false)
	{
		Pairs.Empty(Sources.Num());
		for (FPCGTaggedData& Source : Sources)
		{
			const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Source.Data);
			if (!SpatialData) { continue; }
			const UPCGPointData* PointData = SpatialData->ToPointData(Context);
			if (!PointData) { continue; }
			T& PIOPair = Pairs.Emplace_GetRef();

			PIOPair.Source = &Source;
			PIOPair.In = const_cast<UPCGPointData*>(SpatialData->ToPointData(Context));

			if (bInitializeOutput) { PIOPair.InitializeOut(Context, true); }
		}
	}

	/**
	 * Write valid outputs to Context' tagged data
	 * @param Context 
	 */
	void OutputTo(FPCGContext* Context)
	{
		for (T& PIOPair : Pairs) { PIOPair.OutputTo(Context); }
	}

	void ForEach(
		FPCGContext* Context,
		const TFunction<void(T*, const int32)>& BodyLoop)
	{
		for (int i = 0; i < Pairs.Num(); i++)
		{
			T* PIOPair = &Pairs[i];
			BodyLoop(PIOPair, i);
		}
	}
	
	~FPCGExPointIOMap()
	{
		Pairs.Empty();
	}
};

class FPCGExCommon
{
public:
	template <typename T, typename K>
	static FPCGMetadataAttribute<T>* GetTypedAttribute(const K& Settings)
	{
		if (Settings.Attribute == nullptr) { return nullptr; }
		return static_cast<FPCGMetadataAttribute<T>*>(Settings.Attribute);
	}

	template <typename T, typename K>
	static FPCGMetadataAttribute<T>* GetTypedAttribute(const K* Settings)
	{
		if (Settings->Attribute == nullptr) { return nullptr; }
		return static_cast<FPCGMetadataAttribute<T>*>(Settings->Attribute);
	}

	static bool ForEachPointData(
		FPCGContext* Context,
		TArray<FPCGTaggedData>& Sources,
		const TFunction<void(const FPCGTaggedData& Source, const UPCGPointData* PointData)>& PerPointDataFunc)
	{
		bool bSkippedInvalidData = false;
		for (const FPCGTaggedData& Source : Sources)
		{
			const UPCGSpatialData* AsSpatialData = Cast<UPCGSpatialData>(Source.Data);
			if (!AsSpatialData)
			{
				bSkippedInvalidData = true;
				continue;
			}

			const UPCGPointData* AsPointData = AsSpatialData->ToPointData(Context);
			if (!AsPointData)
			{
				bSkippedInvalidData = true;
				continue;
			}

			PerPointDataFunc(Source, AsPointData);
		}
		return bSkippedInvalidData;
	}

	/**
	 * 
	 * @tparam LoopBodyFunc 
	 * @param Context The context containing the AsyncState
	 * @param NumIterations The number of calls that will be done to the provided function, also an upper bound on the number of data generated.
	 * @param LoopBody Signature: bool(int32 ReadIndex, int32 WriteIndex).
	 * @param ChunkSize Size of the chunks to cut the input data with
	 * @return 
	 */
	template <typename LoopBodyFunc>
	static bool ParallelForLoop(
		FPCGContext* Context,
		const int32 NumIterations,
		LoopBodyFunc&& LoopBody,
		const int32 ChunkSize = 32)
	{
		auto Initialize = []()
		{
		};
		return FPCGAsync::AsyncProcessingOneToOneEx(&(Context->AsyncState), NumIterations, Initialize, LoopBody, true, ChunkSize);
	}

	/**
	 * 
	 * @tparam LoopBodyFunc 
	 * @param LoopBody Signature: bool(int32 ReadIndex, int32 WriteIndex).
	 * @return 
	 */
	static void AsyncForLoop(
		FPCGContext* Context,
		const int32 NumIterations,
		const TFunction<void(int32, int32)>& LoopBody)
	{
		TArray<FPCGPoint> DummyPointsOut;
		FPCGAsync::AsyncPointProcessing(Context, NumIterations, DummyPointsOut, [&LoopBody](int32 Index, FPCGPoint&)
		{
			LoopBody(Index, Index);
			return false;
		});
	}

	/**
	 * For each UPCGPointData found in sources, creates a copy.
	 * @param Context 
	 * @param Sources 
	 * @param OutIOPairs 
	 * @param OnDataCopyBegin 
	 * @param OnDataCopyEnd 
	 */
	static void ForwardCopySourcePoints(
		FPCGContext* Context,
		TArray<FPCGTaggedData>& Sources,
		TArray<FPCGExPointDataIO>& OutIOPairs,
		const TFunction<bool(FPCGExPointDataIO&, const int32, const int32)>& OnDataCopyBegin,
		const TFunction<void(FPCGExPointDataIO&, const int32)>& OnDataCopyEnd)
	{
		OutIOPairs.Empty();

		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		int32 IOIndex = 0;

		for (FPCGTaggedData& Source : Sources)
		{
			const UPCGSpatialData* SourceData = Cast<UPCGSpatialData>(Source.Data);
			if (!SourceData) { continue; }
			const UPCGPointData* InPointData = SourceData->ToPointData(Context);
			if (!InPointData) { continue; }

			FPCGExPointDataIO& IO = OutIOPairs.Emplace_GetRef();
			IO.In = const_cast<UPCGPointData*>(InPointData);
			IO.Out = NewObject<UPCGPointData>();
			IO.Out->InitializeFromData(IO.In);
			IO.Output = &Outputs.Add_GetRef(Source);
			IO.Output->Data = IO.Out;

			const bool bContinue = OnDataCopyBegin(IO, IO.In->GetPoints().Num(), IOIndex);

			if (!bContinue)
			{
				OutIOPairs.Pop();
				Outputs.Pop();
				continue;
			}

			TArray<FPCGPoint>& OutPoints = IO.Out->GetMutablePoints();
			auto CopyPoint = [](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
			{
				OutPoint = InPoint;
				return true;
			};

			FPCGAsync::AsyncPointProcessing(Context, InPointData->GetPoints(), OutPoints, CopyPoint);

			OnDataCopyEnd(IO, IOIndex);
			IOIndex++;
		}
	}

	/**
	 * For each UPCGPointData found in sources, creates a copy.
	 * @param Context 
	 * @param Sources 
	 * @param OutIOPairs 
	 * @param OnDataCopyBegin 
	 * @param OnPointCopied 
	 * @param OnDataCopyEnd 
	 */
	static void ForwardCopySourcePoints(
		FPCGContext* Context,
		TArray<FPCGTaggedData>& Sources,
		TArray<FPCGExPointDataIO>& OutIOPairs,
		const TFunction<bool(FPCGExPointDataIO&, const int32, const int32)>& OnDataCopyBegin,
		const TFunction<void(FPCGPoint&, FPCGExPointDataIO&, const int32)>& OnPointCopied,
		const TFunction<void(FPCGExPointDataIO&, const int32)>& OnDataCopyEnd)
	{
		OutIOPairs.Empty();

		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		int32 IOIndex = 0;

		for (FPCGTaggedData& Source : Sources)
		{
			const UPCGSpatialData* SourceData = Cast<UPCGSpatialData>(Source.Data);
			if (!SourceData) { continue; }
			const UPCGPointData* InPointData = SourceData->ToPointData(Context);
			if (!InPointData) { continue; }

			FPCGExPointDataIO& IO = OutIOPairs.Emplace_GetRef();
			IO.In = const_cast<UPCGPointData*>(InPointData);
			IO.Out = NewObject<UPCGPointData>();
			IO.Out->InitializeFromData(IO.In);
			IO.Output = &Outputs.Add_GetRef(Source);
			IO.Output->Data = IO.Out;

			const bool bContinue = OnDataCopyBegin(IO, IO.In->GetPoints().Num(), IOIndex);

			if (!bContinue)
			{
				OutIOPairs.Pop();
				Outputs.Pop();
				continue;
			}

			int PointIndex = 0;
			TArray<FPCGPoint>& OutPoints = IO.Out->GetMutablePoints();
			auto CopyPoint = [&PointIndex, &IO, &OnPointCopied](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
			{
				OutPoint = InPoint;
				OnPointCopied(OutPoint, IO, PointIndex);
				PointIndex++;
				return true;
			};

			FPCGAsync::AsyncPointProcessing(Context, InPointData->GetPoints(), OutPoints, CopyPoint);

			OnDataCopyEnd(IO, IOIndex);
			IOIndex++;
		}
	}

	/**
	 * For each UPCGPointData found in sources, creates a copy.
	 * @param Context 
	 * @param Sources 
	 * @param OutIOPairs 
	 * @param OnForwardBegin 
	 * @param OnPointFunc 
	 * @param OnForwardEnd 
	 */
	static void ForwardSourcePoints(
		FPCGContext* Context,
		TArray<FPCGTaggedData>& Sources,
		TArray<FPCGExPointDataIO>& OutIOPairs,
		const TFunction<bool(FPCGExPointDataIO&, const int32, const int32)>& OnForwardBegin,
		const TFunction<void(const FPCGPoint&, FPCGExPointDataIO&, const int32)>& OnPointFunc,
		const TFunction<void(FPCGExPointDataIO&, const int32)>& OnForwardEnd,
		int32 ChunkSize = 32)
	{
		OutIOPairs.Empty();

		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		int32 IOIndex = 0;

		for (FPCGTaggedData& Source : Sources)
		{
			const UPCGSpatialData* SourceData = Cast<UPCGSpatialData>(Source.Data);
			if (!SourceData) { continue; }
			const UPCGPointData* InPointData = SourceData->ToPointData(Context);
			if (!InPointData) { continue; }

			FPCGExPointDataIO& IO = OutIOPairs.Emplace_GetRef();
			IO.In = const_cast<UPCGPointData*>(InPointData);
			IO.Out = IO.In;
			IO.Output = &Outputs.Add_GetRef(Source);
			IO.Output->Data = IO.Out;

			const int32 NumIterations = IO.In->GetPoints().Num();
			const bool bContinue = OnForwardBegin(IO, NumIterations, IOIndex);

			if (!bContinue)
			{
				OutIOPairs.Pop();
				Outputs.Pop();
				continue;
			}

			int PointIndex = 0;
			const TArray<FPCGPoint>& OutPoints = IO.Out->GetPoints();
			auto InternalPointFunc = [&PointIndex, &IO, &OnPointFunc, &OutPoints](const int32 ReadIndex, const int32 WriteIndex)
			{
				OnPointFunc(OutPoints[ReadIndex], IO, PointIndex);
				PointIndex++;
				return true;
			};

			ParallelForLoop(Context, NumIterations, InternalPointFunc, ChunkSize);

			OnForwardEnd(IO, IOIndex);
			IOIndex++;
		}
	}
};
