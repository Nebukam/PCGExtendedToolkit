// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "PCGExCommon.generated.h"

#define PCGEX_FOREACH_SUPPORTEDTYPES(MACRO) \
MACRO(bool, Boolean)       \
MACRO(int32, Integer32)      \
MACRO(int64, Integer64)      \
MACRO(float, Float)      \
MACRO(double, Double)     \
MACRO(FVector2D, Vector2D)  \
MACRO(FVector, Vector)    \
MACRO(FVector4, Vector4)   \
MACRO(FQuat, Quaternion)      \
MACRO(FRotator, Rotator)   \
MACRO(FTransform, Transform) \
MACRO(FString, String)    \
MACRO(FName, Name)

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
	X UMETA(DisplayName = "X", ToolTip="X/Roll component if it exist, raw value otherwise."),
	Y UMETA(DisplayName = "Y (→x)", ToolTip="Y/Pitch component if it exist, fallback to previous value otherwise."),
	Z UMETA(DisplayName = "Z (→y)", ToolTip="Z/Yaw component if it exist, fallback to previous value otherwise."),
	W UMETA(DisplayName = "W (→z)", ToolTip="W component if it exist, fallback to previous value otherwise."),
	XYZ UMETA(DisplayName = "X→Y→Z", ToolTip="X, then Y, then Z. Mostly for comparisons, fallback to X/Roll otherwise"),
	XZY UMETA(DisplayName = "X→Z→Y", ToolTip="X, then Z, then Y. Mostly for comparisons, fallback to X/Roll otherwise"),
	YXZ UMETA(DisplayName = "Y→X→Z", ToolTip="Y, then X, then Z. Mostly for comparisons, fallback to Y/Pitch otherwise"),
	YZX UMETA(DisplayName = "Y→Z→X", ToolTip="Y, then Z, then X. Mostly for comparisons, fallback to Y/Pitch otherwise"),
	ZXY UMETA(DisplayName = "Z→X→Y", ToolTip="Z, then X, then Y. Mostly for comparisons, fallback to Z/Yaw otherwise"),
	ZYX UMETA(DisplayName = "Z→Y→X", ToolTip="Z, then Y, then Z. Mostly for comparisons, fallback to Z/Yaw otherwise"),
	Length UMETA(DisplayName = "Length", ToolTip="Length if vector, raw value otherwise."),
};

UENUM(BlueprintType)
enum class EPCGExSingleComponentSelection : uint8
{
	X UMETA(DisplayName = "X/Roll", ToolTip="X/Roll component if it exist, raw value otherwise."),
	Y UMETA(DisplayName = "Y/Pitch", ToolTip="Y/Pitch component if it exist, fallback to previous value otherwise."),
	Z UMETA(DisplayName = "Z/Yaw", ToolTip="Z/Yaw component if it exist, fallback to previous value otherwise."),
	W UMETA(DisplayName = "W", ToolTip="W component if it exist, fallback to previous value otherwise."),
	Length UMETA(DisplayName = "Length", ToolTip="Length if vector, raw value otherwise."),
};

UENUM(BlueprintType)
enum class EPCGExDirectionSelection : uint8
{
	Forward UMETA(DisplayName = "Forward", ToolTip="Forward from Transform/FQuat/Rotator, or raw vector."),
	Backward UMETA(DisplayName = "Backward", ToolTip="Backward from Transform/FQuat/Rotator, or raw vector."),
	Right UMETA(DisplayName = "Right", ToolTip="Right from Transform/FQuat/Rotator, or raw vector."),
	Left UMETA(DisplayName = "Left", ToolTip="Left from Transform/FQuat/Rotator, or raw vector."),
	Up UMETA(DisplayName = "Up", ToolTip="Up from Transform/FQuat/Rotator, or raw vector."),
	Down UMETA(DisplayName = "Down", ToolTip="Down from Transform/FQuat/Rotator, or raw vector."),
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputSelector
{
	GENERATED_BODY()

	FPCGExInputSelector()
	{
		bFixed = false;
	}

	FPCGExInputSelector(const FPCGExInputSelector& Other)
	{
		Selector = Other.Selector;
		Attribute = Other.Attribute;
		bFixed = false;
	}

public:
	/** Name of the attribute to compare */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector Selector;

	/* Can hold a reference to the attribute pointer, if prepared like so */
	FPCGMetadataAttributeBase* Attribute = nullptr;

	/* Whether CopyAndFixLast has been called*/
	bool bFixed = false;

	int16 UnderlyingType = 0;

	bool IsValid(const UPCGPointData* PointData) const
	{
		const EPCGAttributePropertySelection Sel = Selector.GetSelection();
		if (Sel == EPCGAttributePropertySelection::Attribute && (Attribute == nullptr || !PointData->Metadata->HasAttribute(Selector.GetName()))) { return false; }
		return Selector.IsValid();
	}

	template <typename T>
	bool TryGetValue(PCGMetadataEntryKey MetadataEntry, T& OutValue)
	{
		if (Attribute == nullptr) { return false; }
		OutValue = static_cast<FPCGMetadataAttribute<T>*>(Attribute)->GetValueFromItemKey(MetadataEntry);
		return true;
	}

	template <typename T>
	FPCGMetadataAttribute<T>* GetTypedAttribute()
	{
		if (Attribute == nullptr) { return nullptr; }
		return static_cast<FPCGMetadataAttribute<T>*>(Attribute);
	}

	bool Validate(const UPCGPointData* InData)
	{
		bFixed = true;
		Selector = Selector.CopyAndFixLast(InData);
		if (Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			Attribute = Selector.IsValid() ? InData->Metadata->GetMutableAttribute(Selector.GetName()) : nullptr;
			if (Attribute)
			{
				const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InData, Selector);
				UnderlyingType = Accessor->GetUnderlyingType();
				//if (!Accessor.IsValid()) { Attribute = nullptr; }
			}
			return Attribute != nullptr;
		}
		else
		{
			const bool bTempIsValid = Selector.IsValid();
			if (bTempIsValid)
			{
				const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InData, Selector);
				UnderlyingType = Accessor->GetUnderlyingType();
			}
			return bTempIsValid;
		}
	}

	FString ToString() const
	{
		return Selector.GetName().ToString();
	}

	~FPCGExInputSelector()
	{
		Attribute = nullptr;
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputSelectorWithComponent : public FPCGExInputSelector
{
	GENERATED_BODY()

	FPCGExInputSelectorWithComponent(): FPCGExInputSelector()
	{
	}

	FPCGExInputSelectorWithComponent(const FPCGExInputSelectorWithComponent& Other): FPCGExInputSelector(Other)
	{
		ComponentSelection = Other.ComponentSelection;
	}

public:
	/** Sub-sorting order, used only for multi-field attributes (FVector, FRotator etc). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExComponentSelection ComponentSelection = EPCGExComponentSelection::XYZ;

	~FPCGExInputSelectorWithComponent()
	{
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputSelectorWithSingleComponent : public FPCGExInputSelector
{
	GENERATED_BODY()

	FPCGExInputSelectorWithSingleComponent(): FPCGExInputSelector()
	{
	}

	FPCGExInputSelectorWithSingleComponent(const FPCGExInputSelectorWithSingleComponent& Other): FPCGExInputSelector(Other)
	{
		ComponentSelection = Other.ComponentSelection;
		Direction = Other.Direction;
	}

public:
	/** Single component selection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExSingleComponentSelection ComponentSelection = EPCGExSingleComponentSelection::X;

	/** Direction to sample on relevant data types (FQuat are transformed to a direction first, from which the single component is selected) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExDirectionSelection Direction = EPCGExDirectionSelection::Forward;

	~FPCGExInputSelectorWithSingleComponent()
	{
	}
};

namespace PCGEx
{
	enum class EIOInit : uint8
	{
		NoOutput UMETA(DisplayName = "No Output"),
		NewOutput UMETA(DisplayName = "Create Empty Output Object"),
		DuplicateInput UMETA(DisplayName = "Duplicate Input Object"),
	};

	struct PCGEXTENDEDTOOLKIT_API FPointIO
	{
		FPointIO()
		{
			IndicesMap.Empty();
		}

	public:
		FPCGTaggedData Source;       // Source struct
		UPCGPointData* In = nullptr; // Input PointData

		FPCGTaggedData Output;        // Source structS
		UPCGPointData* Out = nullptr; // Output PointData

		int32 NumPoints = -1;

		TMap<PCGMetadataEntryKey, int32> IndicesMap; //MetadataEntry::Index, based on Input points (output MetadataEntry will be offset)

		/**
		 * 
		 * @param InitOut Only initialize output if there is an existing input
		 * @return 
		 */
		bool InitializeOut(EIOInit InitOut = EIOInit::NoOutput)
		{
			switch (InitOut)
			{
			case EIOInit::NoOutput:
				return false;
			case EIOInit::NewOutput:
				Out = NewObject<UPCGPointData>();
				if (In) { Out->InitializeFromData(In); }
				return true;
			case EIOInit::DuplicateInput:
				if (In)
				{
					Out = Cast<UPCGPointData>(In->DuplicateData(true));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Initialize::Duplicate, but no Input."));
				}
				return true;
			default: return false;
			}
		}

		int32 GetIndex(const PCGMetadataEntryKey Key) { return *(IndicesMap.Find(Key)); }

		/**
		 * Write valid outputs to Context' tagged data
		 * @param Context 
		 * @param bEmplace if false (default), will try to use the source first
		 */
		bool OutputTo(FPCGContext* Context, bool bEmplace = false)
		{
			if (!Out) { return false; }

			if (!bEmplace)
			{
				if (!In)
				{
					UE_LOG(LogTemp, Error, TEXT("OutputTo, bEmplace==false but no Input."));
					return false;
				}

				FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Add_GetRef(Source);
				OutputRef.Data = Out;
				Output = OutputRef;
			}
			else
			{
				FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Emplace_GetRef();
				OutputRef.Data = Out;
				Output = OutputRef;
			}
			return true;
		}

		/**
		 * Copy In.Points to Out.Points
		 * @param Context
		 * @param bWriteIndices
		 * @param bInitializeOnSet True if you intend on modifying attributes, otherwise false.
		 * @return 
		 */
		bool ForwardPoints(FPCGContext* Context, bool bWriteIndices = false, bool bInitializeOnSet = true)
		{
			if (!Out || !In) { return false; }

			int32 PointIndex = 0;
			TArray<FPCGPoint>& OutPoints = Out->GetMutablePoints();

			auto CopyPoint = [&PointIndex, &bWriteIndices, &bInitializeOnSet, this](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
			{
				OutPoint = InPoint;
				if (bWriteIndices) { IndicesMap.Add(InPoint.MetadataEntry, PointIndex); }
				if (bInitializeOnSet) { Out->Metadata->InitializeOnSet(OutPoint.MetadataEntry, InPoint.MetadataEntry, In->Metadata); }
				PointIndex++;
				return true;
			};

			FPCGAsync::AsyncPointProcessing(Context, In->GetPoints(), OutPoints, CopyPoint);
			return true;
		}

		/**
		 * Copy In.Points to Out.Points with a callback after each copy
		 * @param Context 
		 * @param PointFunc
		 * @param bWriteIndices
		 * @param bInitializeOnSet True if you intend on modifying attributes, otherwise false.
		 * @return 
		 */
		bool ForwardPoints(
			FPCGContext* Context,
			const TFunction<void(FPCGPoint&, const int32, const FPCGPoint&)>& PointFunc,
			bool bWriteIndices = false, bool bInitializeOnSet = true)
		{
			if (!Out || !In) { return false; }

			int32 PointIndex = 0;
			TArray<FPCGPoint>& OutPoints = Out->GetMutablePoints();
			auto CopyPoint = [&PointIndex, &bWriteIndices, &bInitializeOnSet, &PointFunc, this](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
			{
				OutPoint = InPoint;
				if (bWriteIndices) { IndicesMap.Add(InPoint.MetadataEntry, PointIndex); }
				if (bInitializeOnSet) { Out->Metadata->InitializeOnSet(OutPoint.MetadataEntry, InPoint.MetadataEntry, In->Metadata); }
				PointFunc(OutPoint, PointIndex, InPoint);
				PointIndex++;
				return true;
			};

			FPCGAsync::AsyncPointProcessing(Context, In->GetPoints(), OutPoints, CopyPoint);

			return true;
		}

		~FPointIO()
		{
			IndicesMap.Empty();
			In = nullptr;
			Out = nullptr;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FPointIOGroup
	{
		FPointIOGroup()
		{
			Pairs.Empty();
		}

		FPointIOGroup(
			FPCGContext* Context, FName InputLabel,
			EIOInit InitOut = EIOInit::NoOutput): FPointIOGroup()
		{
			TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(InputLabel);
			Initialize(Context, Sources, InitOut);
		}

		FPointIOGroup(
			FPCGContext* Context, TArray<FPCGTaggedData>& Sources,
			EIOInit InitOut = EIOInit::NoOutput): FPointIOGroup()
		{
			Initialize(Context, Sources, InitOut);
		}

	public:
		TArray<FPointIO> Pairs;

		/**
		 * Initialize from Sources
		 * @param Context 
		 * @param Sources 
		 * @param InitOut 
		 */
		void Initialize(
			FPCGContext* Context, TArray<FPCGTaggedData>& Sources,
			EIOInit InitOut = EIOInit::NoOutput)
		{
			Pairs.Empty(Sources.Num());
			for (FPCGTaggedData& Source : Sources)
			{
				const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Source.Data);
				if (!SpatialData) { continue; }
				const UPCGPointData* PointData = SpatialData->ToPointData(Context);
				if (!PointData) { continue; }

				Emplace_GetRef(Source, const_cast<UPCGPointData*>(SpatialData->ToPointData(Context)), InitOut);
			}
		}

		FPointIO& Emplace_GetRef(
			FPointIO& IO,
			const EIOInit InitOut = EIOInit::NoOutput)
		{
			return Emplace_GetRef(IO.Source, IO.In, InitOut);
		}

		FPointIO& Emplace_GetRef(
			const FPCGTaggedData& Source, UPCGPointData* In,
			const EIOInit InitOut = EIOInit::NoOutput)
		{
			FPointIO& Pair = Pairs.Emplace_GetRef();

			Pair.Source = Source;
			Pair.In = In;
			Pair.NumPoints = Pair.In->GetPoints().Num();

			Pair.InitializeOut(InitOut);
			return Pair;
		}

		bool IsEmpty() { return Pairs.IsEmpty(); }

		/**
		 * Write valid outputs to Context' tagged data
		 * @param Context
		 * @param bEmplace Emplace will create a new entry no matter if a Source is set, otherwise will match the In.Source. 
		 */
		void OutputTo(FPCGContext* Context, bool bEmplace = false)
		{
			for (FPointIO& Pair : Pairs) { Pair.OutputTo(Context, bEmplace); }
		}

		void ForEach(FPCGContext* Context, const TFunction<void(FPointIO*, const int32)>& BodyLoop)
		{
			for (int i = 0; i < Pairs.Num(); i++)
			{
				FPointIO* PIOPair = &Pairs[i];
				BodyLoop(PIOPair, i);
			}
		}

		~FPointIOGroup()
		{
			Pairs.Empty();
		}
	};

	class Common
	{
	public:
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
		 * @param Initialize Called once when the processing starts
		 * @param LoopBody Signature: bool(int32 ReadIndex, int32 WriteIndex).
		 * @param ChunkSize Size of the chunks to cut the input data with
		 * @return 
		 */
		static bool ParallelForLoop
			(
			FPCGContext* Context,
			const int32 NumIterations,
			TFunction<void()>&& Initialize,
			TFunction<void(int32)>&& LoopBody,
			const int32 ChunkSize = 32)
		{
			auto InnerBodyLoop = [&LoopBody](int32 ReadIndex, int32 WriteIndex)
			{
				LoopBody(ReadIndex);
				return true;
			};
			return FPCGAsync::AsyncProcessingOneToOneEx(&(Context->AsyncState), NumIterations, Initialize, InnerBodyLoop, true, ChunkSize);
		}

		/**
		 * 
		 * @param Context
		 * @param NumIterations
		 * @param LoopBody Signature: bool(int32 ReadIndex, int32 WriteIndex).
		 * @return 
		 */
		static void AsyncForLoop
			(
			FPCGContext* Context,
			const int32 NumIterations,
			const TFunction<void(int32)>& LoopBody)
		{
			TArray<FPCGPoint> DummyPointsOut;
			FPCGAsync::AsyncPointProcessing(
				Context, NumIterations, DummyPointsOut, [&LoopBody](int32 Index, FPCGPoint&)
				{
					LoopBody(Index);
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
		[[deprecated("Use FPointIO::ForwardPoint instead; which manages MetadaEntryKeys")]]
		static void ForwardCopySourcePoints
			(
			FPCGContext* Context,
			TArray<FPCGTaggedData>& Sources,
			TArray<FPointIO>& OutIOPairs,
			const TFunction<bool(FPointIO&, const int32, const int32)>& OnDataCopyBegin,
			const TFunction<void(FPointIO&, const int32)>& OnDataCopyEnd)
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

				FPointIO& IO = OutIOPairs.Emplace_GetRef();
				IO.In = const_cast<UPCGPointData*>(InPointData);
				IO.Source = Source;

				IO.Out = NewObject<UPCGPointData>();
				IO.Out->InitializeFromData(IO.In);
				IO.OutputTo(Context);

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
		[[deprecated("Use FPointIO::ForwardPoint instead; which manages MetadaEntryKeys")]]
		static void ForwardCopySourcePoints
			(
			FPCGContext* Context,
			TArray<FPCGTaggedData>& Sources,
			TArray<FPointIO>& OutIOPairs,
			const TFunction<bool(FPointIO&, const int32, const int32)>& OnDataCopyBegin,
			const TFunction<void(FPCGPoint&, FPointIO&, const int32)>& OnPointCopied,
			const TFunction<void(FPointIO&, const int32)>& OnDataCopyEnd)
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

				FPointIO& IO = OutIOPairs.Emplace_GetRef();
				IO.In = const_cast<UPCGPointData*>(InPointData);
				IO.Source = Source;
				IO.Out = NewObject<UPCGPointData>();
				IO.Out->InitializeFromData(IO.In);

				IO.OutputTo(Context);

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
		[[deprecated("Use FPointIO::ForwardPoint instead; which manages MetadaEntryKeys")]]
		static void ForwardSourcePoints
			(
			FPCGContext* Context,
			TArray<FPCGTaggedData>& Sources,
			TArray<FPointIO>& OutIOPairs,
			const TFunction<bool(FPointIO&, const int32, const int32)>& OnForwardBegin,
			const TFunction<void(const FPCGPoint&, FPointIO&, const int32)>& OnPointFunc,
			const TFunction<void(FPointIO&, const int32)>& OnForwardEnd,
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

				FPointIO& IO = OutIOPairs.Emplace_GetRef();
				IO.In = const_cast<UPCGPointData*>(InPointData);
				IO.Source = Source;
				IO.Out = IO.In;
				IO.OutputTo(Context);

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
				auto InternalPointFunc = [&PointIndex, &IO, &OnPointFunc, &OutPoints](const int32 ReadIndex)
				{
					OnPointFunc(OutPoints[ReadIndex], IO, PointIndex);
					PointIndex++;
					return true;
				};

				AsyncForLoop(Context, NumIterations, InternalPointFunc);

				OnForwardEnd(IO, IOIndex);
				IOIndex++;
			}
		}

		static bool IsValidName(const FName& Name)
		{
			return IsValidName(Name.ToString());
		}

		/**
		 * 
		 * @param Name 
		 * @return 
		 */
		static bool IsValidName(const FString& Name)
		{
			// A valid name is alphanumeric with some special characters allowed.
			static const FString AllowedSpecialCharacters = TEXT(" _-/");

			for (int32 i = 0; i < Name.Len(); ++i)
			{
				if (FChar::IsAlpha(Name[i]) || FChar::IsDigit(Name[i]))
				{
					continue;
				}

				bool bAllowedSpecialCharacterFound = false;

				for (int32 j = 0; j < AllowedSpecialCharacters.Len(); ++j)
				{
					if (Name[i] == AllowedSpecialCharacters[j])
					{
						bAllowedSpecialCharacterFound = true;
						break;
					}
				}

				if (!bAllowedSpecialCharacterFound)
				{
					return false;
				}
			}

			return true;
		}
	};
}
