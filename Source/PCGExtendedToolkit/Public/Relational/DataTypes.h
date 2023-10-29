#pragma once

#include "CoreMinimal.h"
#include "Relational\DataTypes.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "..\PCGExCommon.h"
#include "DataTypes.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FDirectionalRelationSlotSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName AttributeName = "DRS_";

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector Direction = FVector::UpVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float DotTolerance = 0.707f; // 45deg
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FDirectionalRelationData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int64 Index = -1;

#pragma region Operators

	friend FArchive& operator<<(FArchive& Ar, FDirectionalRelationData& SlotAttData)
	{
		// Implement serialization and deserialization here
		//Ar << SlotAttData.Index;		
		return Ar;
	}

	bool operator==(const FDirectionalRelationData& Other) const
	{
		return Other.Index == Index;
	}

	FDirectionalRelationData operator*(float Weight) const
	{
		FDirectionalRelationData result;
		result.Index = this->Index;
		return result;
	}

	FDirectionalRelationData operator*(const FDirectionalRelationData Other) const
	{
		FDirectionalRelationData result;
		result.Index = Other.Index * this->Index;
		return result;
	}

	FDirectionalRelationData operator+(const FDirectionalRelationData Other) const
	{
		FDirectionalRelationData result;
		result.Index = Other.Index + this->Index;
		return result;
	}

	FDirectionalRelationData operator-(const FDirectionalRelationData Other) const
	{
		FDirectionalRelationData result;
		result.Index = Other.Index - this->Index;
		return result;
	}

	FDirectionalRelationData operator/(const FDirectionalRelationData Other) const
	{
		FDirectionalRelationData result;
		result.Index = Other.Index / this->Index;
		return result;
	}

	bool operator<(const FDirectionalRelationData& Other) const
	{
		return Index < Other.Index;
	}

#pragma endregion
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FDirectionalRelationSlotListSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TArray<FDirectionalRelationSlotSettings> Slots = {
		FDirectionalRelationSlotSettings{"DRS_Forward", FVector::ForwardVector},
		FDirectionalRelationSlotSettings{"DRS_Backward", FVector::BackwardVector},
		FDirectionalRelationSlotSettings{"DRS_Right", FVector::RightVector},
		FDirectionalRelationSlotSettings{"DRS_Left", FVector::LeftVector},
		FDirectionalRelationSlotSettings{"DRS_Up", FVector::UpVector},
		FDirectionalRelationSlotSettings{"DRS_Down", FVector::DownVector}
	};

	int Num() const
	{
		return  Slots.Num();
	}
};

USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FSlotCandidateData
{
	GENERATED_BODY()

public:
	int64 Index = -1;
	float MinDistance = TNumericLimits<float>::Max();

	void Reset()
	{
		Index = -1;
		MinDistance = TNumericLimits<float>::Max();
	}
	
};

class PCGEXTENDEDTOOLKIT_API DataTypeHelpers
{
public:
	static TArray<const FPCGMetadataAttribute<FDirectionalRelationData>*> FindOrCreateAttributes(
		const FDirectionalRelationSlotListSettings& Data, const UPCGPointData* OutputData)
	{
		int32 Num = Data.Slots.Num();
		TArray<const FPCGMetadataAttribute<FDirectionalRelationData>*> SlotAttributes;
		SlotAttributes.Reserve(Num);

		// Get or create matching attributes
		for (int i = 0; i < Num; i++)
		{
			const FDirectionalRelationSlotSettings SlotSettings = Data.Slots[i];
			const FPCGMetadataAttribute<FDirectionalRelationData>* SlotAttribute = OutputData->Metadata->
				FindOrCreateAttribute<FDirectionalRelationData>(
					SlotSettings.AttributeName, FDirectionalRelationData{-1}, false);
			SlotAttributes.Add(SlotAttribute);
		}

		return SlotAttributes;
	}
};
