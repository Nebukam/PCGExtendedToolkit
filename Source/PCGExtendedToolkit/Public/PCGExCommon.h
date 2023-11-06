// Fill out your copyright notice in the Description page of Project Rules.

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGElement.h"
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
		OutValue = static_cast<FPCGMetadataAttribute<T>*>(Attribute)->GetValue(Key);
		return true;
	}

	bool CopyAndFixLast(const UPCGPointData* InData)
	{
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
};

USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExPointDataPair
{
	GENERATED_BODY()

public:
	UPCGPointData* SourcePointData;
	UPCGPointData* OutputPointData;
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

	static bool ForEachPointData(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, const TFunction<void(const FPCGTaggedData& Source, const UPCGPointData* PointData)>& PerPointDataFunc)
	{
		bool bSkippedInvalidData = false; 
		for (const FPCGTaggedData& Source : Sources)
		{

			const UPCGSpatialData* AsSpatialData = Cast<UPCGSpatialData>(Source.Data);
			if (!AsSpatialData) { bSkippedInvalidData = true; continue; }
			
			const UPCGPointData* AsPointData = AsSpatialData->ToPointData(Context);
			if (!AsPointData) { bSkippedInvalidData = true; continue; }

			PerPointDataFunc(Source, AsPointData);
			
		}
		return  bSkippedInvalidData;
	}

	/**
	 * 
	 * @tparam ProcessElementFunc 
	 * @param Context The context containing the AsyncState
	 * @param NumIterations The number of calls that will be done to the provided function, also an upper bound on the number of data generated.
	 * @param LoopBody Signature: bool(int32 ReadIndex, int32 WriteIndex).
	 * @param ChunkSize Size of the chunks to cut the input data with
	 * @return 
	 */
	template <typename ProcessElementFunc>
	bool AsyncForLoop(FPCGContext* Context, const int32 NumIterations, ProcessElementFunc&& LoopBody, const int32 ChunkSize = 32)
	{
		auto Initialize = [](){};		
		return FPCGAsync::AsyncProcessingOneToOneEx(Context->AsyncState, NumIterations, Initialize, LoopBody, true, ChunkSize);
	}

	/**
	 * For each UPCGPointData found in sources, creates a copy.
	 * @param Context 
	 * @param Sources 
	 * @param OutPairs 
	 * @param OnDataCopyEnd 
	 */
	static void ForwardSourcePoints(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, TArray<FPCGExPointDataPair>& OutPairs, const TFunction<bool(const int32, const int32, FPCGExPointDataPair&)>& OnDataCopyBegin, const TFunction<void(const int32, FPCGExPointDataPair&)>& OnDataCopyEnd)
	{

		OutPairs.Reset();

		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		int32 Index = 0;
		
		for (FPCGTaggedData& Source : Sources)
		{
			
			const UPCGSpatialData* SourceData = Cast<UPCGSpatialData>(Source.Data);
			if (!SourceData) { continue; }
			const UPCGPointData* InPointData = SourceData->ToPointData(Context);
			if (!InPointData) { continue; }

			FPCGExPointDataPair& OutPair = OutPairs.Emplace_GetRef();
			OutPair.SourcePointData = const_cast<UPCGPointData*>(InPointData);
			OutPair.OutputPointData = NewObject<UPCGPointData>();
			OutPair.OutputPointData->InitializeFromData(OutPair.SourcePointData);
			Outputs.Add_GetRef(Source).Data = OutPair.OutputPointData;

			const bool bContinue = OnDataCopyBegin(Index, OutPair.SourcePointData->GetPoints().Num(), OutPair);

			if(!bContinue)
			{
				OutPairs.Pop();
				Outputs.Pop();
				continue;
			}
			
			TArray<FPCGPoint>& OutPoints = OutPair.OutputPointData->GetMutablePoints();
			auto CopyPoint = [](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
			{
				OutPoint = InPoint;
				return true;
			};
		
			FPCGAsync::AsyncPointProcessing(Context, InPointData->GetPoints(), OutPoints, CopyPoint);

			OnDataCopyEnd(Index, OutPair);
			Index++;
			
		}
		
	}

	/**
	 * For each UPCGPointData found in sources, creates a copy.
	 * @param Context 
	 * @param Sources 
	 * @param OutPairs 
	 * @param OnPointCopied 
	 * @param OnDataCopyEnd 
	 */
	static void ForwardSourcePoints(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, TArray<FPCGExPointDataPair>& OutPairs, const TFunction<bool(const int32, const int32, FPCGExPointDataPair&)>& OnDataCopyBegin, const TFunction<void(const int32, const FPCGPoint&, FPCGPoint&, FPCGExPointDataPair&)>& OnPointCopied, const TFunction<void(const int32, FPCGExPointDataPair&)>& OnDataCopyEnd)
	{

		OutPairs.Reset();

		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		int32 Index = 0;
		
		for (FPCGTaggedData& Source : Sources)
		{
			
			const UPCGSpatialData* SourceData = Cast<UPCGSpatialData>(Source.Data);
			if (!SourceData) { continue; }
			const UPCGPointData* InPointData = SourceData->ToPointData(Context);
			if (!InPointData) { continue; }

			FPCGExPointDataPair& OutPair = OutPairs.Emplace_GetRef();
			OutPair.SourcePointData = const_cast<UPCGPointData*>(InPointData);
			OutPair.OutputPointData = NewObject<UPCGPointData>();
			OutPair.OutputPointData->InitializeFromData(OutPair.SourcePointData);
			Outputs.Add_GetRef(Source).Data = OutPair.OutputPointData;

			const bool bContinue = OnDataCopyBegin(Index, OutPair.SourcePointData->GetPoints().Num(), OutPair);

			if(!bContinue)
			{
				OutPairs.Pop();
				Outputs.Pop();
				continue;
			}
			
			int PointIndex = 0;
			TArray<FPCGPoint>& OutPoints = OutPair.OutputPointData->GetMutablePoints();
			auto CopyPoint = [&PointIndex, OutPair, &OnPointCopied](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
			{
				OutPoint = InPoint;
				OnPointCopied(PointIndex, InPoint, OutPoint, OutPair);
				PointIndex++;
				return true;
			};
		
			FPCGAsync::AsyncPointProcessing(Context, InPointData->GetPoints(), OutPoints, CopyPoint);

			OnDataCopyEnd(Index, OutPair);
			Index++;
			
		}
		
	}
	
};
