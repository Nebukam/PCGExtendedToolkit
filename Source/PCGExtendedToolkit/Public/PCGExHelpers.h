// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

// Most helpers here are Copyright Epic Games, Inc. All Rights Reserved, cherry picked for 5.3

#pragma once
#include "PCGContext.h"
#include "PCGElement.h"
#include "PCGExMacros.h"
#include "PCGModule.h"
#include "Metadata/PCGMetadataAttributeTraits.h"

#include "PCGExHelpers.generated.h"

namespace PCGExHelpers
{
	static TArray<UFunction*> FindUserFunctions(const TSubclassOf<AActor>& ActorClass, const TArray<FName>& FunctionNames, const TArray<const UFunction*>& FunctionPrototypes, const FPCGContext* InContext)
	{
		TArray<UFunction*> Functions;

		if (!ActorClass)
		{
			return Functions;
		}

		for (FName FunctionName : FunctionNames)
		{
			if (FunctionName == NAME_None)
			{
				continue;
			}

			if (UFunction* Function = ActorClass->FindFunctionByName(FunctionName))
			{
#if WITH_EDITOR
				if (!Function->GetBoolMetaData(TEXT("CallInEditor")))
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT( "Function '{0}' in class '{1}' requires CallInEditor to be true while in-editor."), FText::FromName(FunctionName), FText::FromName(ActorClass->GetFName())));
					continue;
				}
#endif
				for (const UFunction* Prototype : FunctionPrototypes)
				{
					if (Function->IsSignatureCompatibleWith(Prototype))
					{
						Functions.Add(Function);
						break;
					}
				}

				if (Functions.IsEmpty() || Functions.Last() != Function)
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Function '{0}' in class '{1}' has incorrect parameters."), FText::FromName(FunctionName), FText::FromName(ActorClass->GetFName())));
				}
			}
			else
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Function '{0}' was not found in class '{1}'."), FText::FromName(FunctionName), FText::FromName(ActorClass->GetFName())));
			}
		}

		return Functions;
	}
}

/** Holds function prototypes used to match against actor function signatures. */
UCLASS(MinimalAPI)
class UPCGExFunctionPrototypes : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static UFunction* GetPrototypeWithNoParams() { return FindObject<UFunction>(StaticClass(), TEXT("PrototypeWithNoParams")); }
	static UFunction* GetPrototypeWithPointAndMetadata() { return FindObject<UFunction>(StaticClass(), TEXT("PrototypeWithPointAndMetadata")); }

private:
	UFUNCTION()
	void PrototypeWithNoParams()
	{
	}

	UFUNCTION()
	void PrototypeWithPointAndMetadata(FPCGPoint Point, const UPCGMetadata* Metadata)
	{
	}
};

namespace PCGEx
{
#pragma region Metadata Type

	template <typename T>
	static EPCGMetadataTypes GetMetadataType()
	{
		if constexpr (std::is_same_v<T, bool>) { return EPCGMetadataTypes::Boolean; }
		else if constexpr (std::is_same_v<T, int32>) { return EPCGMetadataTypes::Integer32; }
		else if constexpr (std::is_same_v<T, int64>) { return EPCGMetadataTypes::Integer64; }
		else if constexpr (std::is_same_v<T, float>) { return EPCGMetadataTypes::Float; }
		else if constexpr (std::is_same_v<T, double>) { return EPCGMetadataTypes::Double; }
		else if constexpr (std::is_same_v<T, FVector2D>) { return EPCGMetadataTypes::Vector2; }
		else if constexpr (std::is_same_v<T, FVector>) { return EPCGMetadataTypes::Vector; }
		else if constexpr (std::is_same_v<T, FVector4>) { return EPCGMetadataTypes::Vector4; }
		else if constexpr (std::is_same_v<T, FQuat>) { return EPCGMetadataTypes::Quaternion; }
		else if constexpr (std::is_same_v<T, FRotator>) { return EPCGMetadataTypes::Rotator; }
		else if constexpr (std::is_same_v<T, FTransform>) { return EPCGMetadataTypes::Transform; }
		else if constexpr (std::is_same_v<T, FString>) { return EPCGMetadataTypes::String; }
		else if constexpr (std::is_same_v<T, FName>) { return EPCGMetadataTypes::Name; }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
		else if constexpr (std::is_same_v<T, FSoftClassPath>) { return EPCGMetadataTypes::SoftClassPath; }
		else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return EPCGMetadataTypes::SoftObjectPath; }
#endif
		else { return EPCGMetadataTypes::Unknown; }
	}

	static EPCGMetadataTypes GetPropertyType(const EPCGPointProperties Property)
	{
		switch (Property)
		{
		case EPCGPointProperties::Density:
			return EPCGMetadataTypes::Float;
		case EPCGPointProperties::BoundsMin:
			return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::BoundsMax:
			return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::Extents:
			return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::Color:
			return EPCGMetadataTypes::Vector4;
		case EPCGPointProperties::Position:
			return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::Rotation:
			return EPCGMetadataTypes::Rotator;
		case EPCGPointProperties::Scale:
			return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::Transform:
			return EPCGMetadataTypes::Transform;
		case EPCGPointProperties::Steepness:
			return EPCGMetadataTypes::Float;
		case EPCGPointProperties::LocalCenter:
			return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::Seed:
			return EPCGMetadataTypes::Integer32;
		default:
			return EPCGMetadataTypes::Unknown;
		}
	}

	template <typename T>
	FORCEINLINE static void InitArray(TArray<T>& InArray, const int32 Num)
	{
		if constexpr (std::is_trivially_copyable_v<T>) { InArray.SetNumUninitialized(Num); }
		else { InArray.SetNum(Num); }
	}

	template <typename T>
	FORCEINLINE static void InitArray(TSharedPtr<TArray<T>> InArray, const int32 Num)
	{
		if constexpr (std::is_trivially_copyable_v<T>) { InArray->SetNumUninitialized(Num); }
		else { InArray->SetNum(Num); }
	}

	template <typename T>
	FORCEINLINE static void InitArray(TSharedRef<TArray<T>> InArray, const int32 Num)
	{
		if constexpr (std::is_trivially_copyable_v<T>) { InArray.SetNumUninitialized(Num); }
		else { InArray.SetNum(Num); }
	}

	template <typename T>
	FORCEINLINE static void InitArray(TArray<T>* InArray, const int32 Num)
	{
		if constexpr (std::is_trivially_copyable_v<T>) { InArray->SetNumUninitialized(Num); }
		else { InArray->SetNum(Num); }
	}

#pragma endregion
}
