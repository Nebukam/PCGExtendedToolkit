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

	template <typename T, typename CompilerSafety = void>
	static EPCGMetadataTypes GetMetadataType(const T Dummy) { return EPCGMetadataTypes::Unknown; }

	template <typename CompilerSafety = void>
	static EPCGMetadataTypes GetMetadataType(const bool Dummy) { return EPCGMetadataTypes::Boolean; }

	template <typename CompilerSafety = void>
	static EPCGMetadataTypes GetMetadataType(const int32 Dummy) { return EPCGMetadataTypes::Integer32; }

	template <typename CompilerSafety = void>
	static EPCGMetadataTypes GetMetadataType(const int64 Dummy) { return EPCGMetadataTypes::Integer64; }

	template <typename CompilerSafety = void>
	static EPCGMetadataTypes GetMetadataType(const float Dummy) { return EPCGMetadataTypes::Float; }

	template <typename CompilerSafety = void>
	static EPCGMetadataTypes GetMetadataType(const double Dummy) { return EPCGMetadataTypes::Double; }

	template <typename CompilerSafety = void>
	static EPCGMetadataTypes GetMetadataType(const FVector2D Dummy) { return EPCGMetadataTypes::Vector2; }

	template <typename CompilerSafety = void>
	static EPCGMetadataTypes GetMetadataType(const FVector Dummy) { return EPCGMetadataTypes::Vector; }

	template <typename CompilerSafety = void>
	static EPCGMetadataTypes GetMetadataType(const FVector4 Dummy) { return EPCGMetadataTypes::Vector4; }

	template <typename CompilerSafety = void>
	static EPCGMetadataTypes GetMetadataType(const FQuat Dummy) { return EPCGMetadataTypes::Quaternion; }

	template <typename CompilerSafety = void>
	static EPCGMetadataTypes GetMetadataType(const FRotator Dummy) { return EPCGMetadataTypes::Rotator; }

	template <typename CompilerSafety = void>
	static EPCGMetadataTypes GetMetadataType(const FTransform Dummy) { return EPCGMetadataTypes::Transform; }

	template <typename CompilerSafety = void>
	static EPCGMetadataTypes GetMetadataType(const FString Dummy) { return EPCGMetadataTypes::String; }

	template <typename CompilerSafety = void>
	static EPCGMetadataTypes GetMetadataType(const FName Dummy) { return EPCGMetadataTypes::Name; }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3

	template <typename CompilerSafety = void>
	static EPCGMetadataTypes GetMetadataType(const FSoftClassPath Dummy) { return EPCGMetadataTypes::Unknown; }

	template <typename CompilerSafety = void>
	static EPCGMetadataTypes GetMetadataType(const FSoftObjectPath Dummy) { return EPCGMetadataTypes::Unknown; }

#endif


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
	static void InitMetadataArray(TArray<T>& MetadataValues, const int32 Num)
	{
		if constexpr (std::is_trivially_copyable_v<T>) { MetadataValues.SetNumUninitialized(Num); }
		else { MetadataValues.SetNum(Num); }
	}

#pragma endregion
}
