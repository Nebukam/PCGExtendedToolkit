// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"

#include "PCGExCagePropertyCompiled.generated.h"

class UPCGExAssetCollection;

/**
 * Base struct for compiled cage properties.
 * All property types derive from this and must include PropertyName.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExCagePropertyCompiled
{
	GENERATED_BODY()

	/** User-defined name for disambiguation when multiple properties exist */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	FName PropertyName;

	virtual ~FPCGExCagePropertyCompiled() = default;
};

/**
 * Compiled property referencing a PCGExAssetCollection.
 * Used for mesh/actor swapping based on pattern matches.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExCagePropertyCompiled_AssetCollection : public FPCGExCagePropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	TSoftObjectPtr<UPCGExAssetCollection> AssetCollection;
};

/**
 * Compiled property containing additional tags.
 * Supplements actor tags for more granular filtering.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExCagePropertyCompiled_Tags : public FPCGExCagePropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	TArray<FName> Tags;
};

/**
 * Compiled property containing generic key-value metadata.
 * For arbitrary user data that doesn't warrant a dedicated property type.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExCagePropertyCompiled_Metadata : public FPCGExCagePropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	TMap<FName, FString> Metadata;
};

/**
 * Query helpers for accessing properties from FInstancedStruct arrays.
 */
namespace PCGExValency
{
	/**
	 * Get first property of specified type, optionally filtered by name.
	 * @param Properties - Array of FInstancedStruct containing compiled properties
	 * @param PropertyName - Optional name filter (NAME_None matches any)
	 * @return Pointer to property if found, nullptr otherwise
	 */
	template <typename T>
	const T* GetProperty(const TArray<FInstancedStruct>& Properties, FName PropertyName = NAME_None)
	{
		static_assert(TIsDerivedFrom<T, FPCGExCagePropertyCompiled>::Value,
			"T must derive from FPCGExCagePropertyCompiled");

		for (const FInstancedStruct& Prop : Properties)
		{
			if (const T* Typed = Prop.GetPtr<T>())
			{
				if (PropertyName.IsNone() || Typed->PropertyName == PropertyName)
				{
					return Typed;
				}
			}
		}
		return nullptr;
	}

	/**
	 * Get all properties of specified type.
	 * @param Properties - Array of FInstancedStruct containing compiled properties
	 * @return Array of pointers to matching properties
	 */
	template <typename T>
	TArray<const T*> GetAllProperties(const TArray<FInstancedStruct>& Properties)
	{
		static_assert(TIsDerivedFrom<T, FPCGExCagePropertyCompiled>::Value,
			"T must derive from FPCGExCagePropertyCompiled");

		TArray<const T*> Result;
		for (const FInstancedStruct& Prop : Properties)
		{
			if (const T* Typed = Prop.GetPtr<T>())
			{
				Result.Add(Typed);
			}
		}
		return Result;
	}

	/**
	 * Get property by name regardless of type.
	 * @param Properties - Array of FInstancedStruct containing compiled properties
	 * @param PropertyName - Name to search for
	 * @return Pointer to FInstancedStruct if found, nullptr otherwise
	 */
	inline const FInstancedStruct* GetPropertyByName(const TArray<FInstancedStruct>& Properties, FName PropertyName)
	{
		if (PropertyName.IsNone())
		{
			return nullptr;
		}

		for (const FInstancedStruct& Prop : Properties)
		{
			if (const FPCGExCagePropertyCompiled* Base = Prop.GetPtr<FPCGExCagePropertyCompiled>())
			{
				if (Base->PropertyName == PropertyName)
				{
					return &Prop;
				}
			}
		}
		return nullptr;
	}

	/**
	 * Check if properties array contains a property with given name.
	 */
	inline bool HasProperty(const TArray<FInstancedStruct>& Properties, FName PropertyName)
	{
		return GetPropertyByName(Properties, PropertyName) != nullptr;
	}

	/**
	 * Check if properties array contains any property of given type.
	 */
	template <typename T>
	bool HasPropertyOfType(const TArray<FInstancedStruct>& Properties)
	{
		return GetProperty<T>(Properties) != nullptr;
	}
}
