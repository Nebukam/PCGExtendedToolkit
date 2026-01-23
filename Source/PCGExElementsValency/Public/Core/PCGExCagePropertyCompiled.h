// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPropertyCompiled.h"

#include "PCGExCagePropertyCompiled.generated.h"

/**
 * Base struct for compiled cage properties.
 * Derives from the shared FPCGExPropertyCompiled to inherit the output interface.
 *
 * All Valency property types derive from this and must include PropertyName.
 *
 * Properties support an output interface for writing values to point attributes:
 * - InitializeOutput(): Creates buffer(s) on a facade
 * - WriteOutput(): Writes value(s) to initialized buffer(s)
 * - CopyValueFrom(): Copies value from another property of same type
 *
 * To add a new property type:
 * 1. Create derived struct in PCGExCagePropertyCompiledTypes.h (runtime module)
 * 2. Create derived component in PCGExCagePropertyTypes.h/.cpp (editor module)
 * 3. See .claude/Cage_Property_System.md for full documentation
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExCagePropertyCompiled : public FPCGExPropertyCompiled
{
	GENERATED_BODY()

	virtual ~FPCGExCagePropertyCompiled() = default;

	// Inherit all functionality from FPCGExPropertyCompiled
};

/**
 * Query helpers for accessing properties from FInstancedStruct arrays.
 * All functions accept TConstArrayView to work with both TArray and TArrayView.
 *
 * These are Valency-specific wrappers that work with FPCGExCagePropertyCompiled.
 * For generic property access, use the PCGExProperties namespace.
 */
namespace PCGExValency
{
	/**
	 * Get first property of specified type, optionally filtered by name.
	 * @param Properties - Array view of FInstancedStruct containing compiled properties
	 * @param PropertyName - Optional name filter (NAME_None matches any)
	 * @return Pointer to property if found, nullptr otherwise
	 */
	template <typename T>
	const T* GetProperty(TConstArrayView<FInstancedStruct> Properties, FName PropertyName = NAME_None)
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
	 * @param Properties - Array view of FInstancedStruct containing compiled properties
	 * @return Array of pointers to matching properties
	 */
	template <typename T>
	TArray<const T*> GetAllProperties(TConstArrayView<FInstancedStruct> Properties)
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
	 * @param Properties - Array view of FInstancedStruct containing compiled properties
	 * @param PropertyName - Name to search for
	 * @return Pointer to FInstancedStruct if found, nullptr otherwise
	 */
	inline const FInstancedStruct* GetPropertyByName(TConstArrayView<FInstancedStruct> Properties, FName PropertyName)
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
	inline bool HasProperty(TConstArrayView<FInstancedStruct> Properties, FName PropertyName)
	{
		return GetPropertyByName(Properties, PropertyName) != nullptr;
	}

	/**
	 * Check if properties array contains any property of given type.
	 */
	template <typename T>
	bool HasPropertyOfType(TConstArrayView<FInstancedStruct> Properties)
	{
		return GetProperty<T>(Properties) != nullptr;
	}
}
