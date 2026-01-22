// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Data/PCGExData.h"

#include "PCGExCagePropertyCompiled.generated.h"

/**
 * Entry in the property registry.
 * Built at compile time to provide a read-only view of available properties.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExPropertyRegistryEntry
{
	GENERATED_BODY()

	/** Property name */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Property")
	FName PropertyName;

	/** Property type name (e.g., "String", "Int32", "Vector") */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Property")
	FName TypeName;

	/** PCG metadata type for attribute output */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Property")
	EPCGMetadataTypes OutputType = EPCGMetadataTypes::Unknown;

	/** Whether this property supports attribute output */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Property")
	bool bSupportsOutput = false;

	FPCGExPropertyRegistryEntry() = default;

	FPCGExPropertyRegistryEntry(FName InName, FName InTypeName, EPCGMetadataTypes InOutputType, bool bInSupportsOutput)
		: PropertyName(InName)
		, TypeName(InTypeName)
		, OutputType(InOutputType)
		, bSupportsOutput(bInSupportsOutput)
	{
	}
};

/**
 * Base struct for compiled cage properties.
 * All property types derive from this and must include PropertyName.
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
struct PCGEXELEMENTSVALENCY_API FPCGExCagePropertyCompiled
{
	GENERATED_BODY()

	/** User-defined name for disambiguation when multiple properties exist */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	FName PropertyName;

	virtual ~FPCGExCagePropertyCompiled() = default;

	// --- Output Interface ---

	/**
	 * Initialize output buffer(s) on the facade.
	 * Override in derived types that support output.
	 * @param OutputFacade The facade to create buffers on
	 * @param OutputName The attribute name to use
	 * @return true if initialization succeeded
	 */
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) { return false; }

	/**
	 * Write this property's value(s) to the initialized buffer(s).
	 * Call after InitializeOutput() succeeded.
	 * @param PointIndex The point index to write to
	 */
	virtual void WriteOutput(int32 PointIndex) const {}

	/**
	 * Copy value from another property of the same type.
	 * Used when switching between modules that have the same property.
	 * @param Source The source property to copy from (must be same concrete type)
	 */
	virtual void CopyValueFrom(const FPCGExCagePropertyCompiled* Source) {}

	/**
	 * Check if this property type supports attribute output.
	 */
	virtual bool SupportsOutput() const { return false; }

	/**
	 * Get the PCG metadata type for this property (for UI/validation).
	 * Return EPCGMetadataTypes::Unknown if not applicable or multi-valued.
	 */
	virtual EPCGMetadataTypes GetOutputType() const { return EPCGMetadataTypes::Unknown; }

	/**
	 * Get the human-readable type name for this property (e.g., "String", "Int32", "Vector").
	 * Used for registry display.
	 */
	virtual FName GetTypeName() const { return FName("Unknown"); }

	/**
	 * Create a registry entry for this property.
	 */
	FPCGExPropertyRegistryEntry ToRegistryEntry() const
	{
		return FPCGExPropertyRegistryEntry(PropertyName, GetTypeName(), GetOutputType(), SupportsOutput());
	}
};

/**
 * Query helpers for accessing properties from FInstancedStruct arrays.
 * All functions accept TConstArrayView to work with both TArray and TArrayView.
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
