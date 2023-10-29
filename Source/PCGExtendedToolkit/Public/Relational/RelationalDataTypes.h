#pragma once

#include "CoreMinimal.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "PCGExAttributesUtils.h"
#include "RelationalDataTypes.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FDRSlotSettings
{
	GENERATED_BODY()

public:
	/** Name of the attribute to write neighbor index to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName AttributeName = "DRS_";

	/** Slot 'look-at' direction. Used along with DotTolerance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector Direction = FVector::UpVector;

	/** Tolerance threshold. Used along with the direction of the slot when looking for the closest candidate. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float DotTolerance = 0.707f; // 45deg
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FDRSlotListSettings
{
	GENERATED_BODY()

public:
	/** List of slot settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TArray<FDRSlotSettings> Slots = {
		FDRSlotSettings{"DRS_Forward", FVector::ForwardVector},
		FDRSlotSettings{"DRS_Backward", FVector::BackwardVector},
		FDRSlotSettings{"DRS_Right", FVector::RightVector},
		FDRSlotSettings{"DRS_Left", FVector::LeftVector},
		FDRSlotSettings{"DRS_Up", FVector::UpVector},
		FDRSlotSettings{"DRS_Down", FVector::DownVector}
	};
};

USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FSlotCandidateData
{
	GENERATED_BODY()

public:
	FPCGMetadataAttribute<int64>* Attribute = nullptr;
	int64 Index = -1;
	float MinDistance = TNumericLimits<float>::Max();

	void Reset()
	{
		Index = -1;
		MinDistance = TNumericLimits<float>::Max();
	}
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FDRData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int64 Index = -1;

#pragma region Operators

	friend FArchive& operator<<(FArchive& Ar, FDRData& SlotAttData)
	{
		// Implement serialization and deserialization here
		//Ar << SlotAttData.Index;		
		return Ar;
	}

	bool operator==(const FDRData& Other) const
	{
		return Other.Index == Index;
	}

	FDRData operator*(float Weight) const
	{
		FDRData result;
		result.Index = this->Index;
		return result;
	}

	FDRData operator*(const FDRData Other) const
	{
		FDRData result;
		result.Index = Other.Index * this->Index;
		return result;
	}

	FDRData operator+(const FDRData Other) const
	{
		FDRData result;
		result.Index = Other.Index + this->Index;
		return result;
	}

	FDRData operator-(const FDRData Other) const
	{
		FDRData result;
		result.Index = Other.Index - this->Index;
		return result;
	}

	FDRData operator/(const FDRData Other) const
	{
		FDRData result;
		result.Index = Other.Index / this->Index;
		return result;
	}

	bool operator<(const FDRData& Other) const
	{
		return Index < Other.Index;
	}

#pragma endregion
};

class PCGEXTENDEDTOOLKIT_API RelationalDataTypeHelpers
{
public:
	static TArray<FPCGMetadataAttribute<FDRData>*> FindOrCreateAttributes(
		FDRSlotListSettings Data, const UPCGPointData* OutputData)
	{
		int32 Num = Data.Slots.Num();
		TArray<FPCGMetadataAttribute<FDRData>*> SlotAttributes;
		SlotAttributes.Reserve(Num);

		for (const FDRSlotSettings& S : Data.Slots)
		{
			SlotAttributes.Add(
				OutputData->Metadata->FindOrCreateAttribute<FDRData>(S.AttributeName, FDRData{-1}, false));
		}

		return SlotAttributes;
	}

};
