// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExData.h"
#include "PCGExProperty.h"

#include "PCGExPropertyWriter.generated.h"

/**
 * Configuration for a single property output.
 * Associates a property (by name) with an output attribute name.
 *
 * This is the user-facing config for "which properties should become point attributes".
 * PropertyName must match a property defined in the source schema/provider.
 * OutputAttributeName lets the user rename the output attribute (defaults to PropertyName).
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyOutputConfig
{
	GENERATED_BODY()

	/** Whether this output config is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	bool bEnabled = true;

	/** Property name to output (must match a property in the source) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta=(EditCondition="bEnabled"))
	FName PropertyName;

	/** Attribute name for output (if empty, uses PropertyName) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta=(EditCondition="bEnabled"))
	FName OutputAttributeName;

	/**
	 * Get effective output name, validated for PCG compatibility.
	 * Returns NAME_None if the name is invalid.
	 */
	FName GetEffectiveOutputName() const;
	
	bool IsValid() const
	{
		return bEnabled && !PropertyName.IsNone() && !GetEffectiveOutputName().IsNone();
	}
};

/**
 * Reusable settings struct for property output configuration.
 * Can be embedded in any node that needs to output properties.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyOutputSettings
{
	GENERATED_BODY()

	/**
	 * Properties to output as point attributes.
	 * Each config maps a property name to an output attribute name.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(TitleProperty="PropertyName"))
	TArray<FPCGExPropertyOutputConfig> Configs;

	/** Check if any outputs are configured */
	bool HasOutputs() const
	{
		for (const FPCGExPropertyOutputConfig& Config : Configs)
		{
			if (Config.IsValid()) { return true; }
		}
		return false;
	}

	/**
	 * Auto-populate configs from a property registry.
	 * Adds configs for all properties that support output and aren't already configured.
	 * Skips properties already configured (enabled configs only).
	 * @param Registry The property registry to scan
	 * @return Number of configs added
	 */
	int32 AutoPopulateFromRegistry(TConstArrayView<FPCGExPropertyRegistryEntry> Registry)
	{
		// Collect existing enabled property names
		TSet<FName> ExistingNames;
		for (const FPCGExPropertyOutputConfig& Config : Configs)
		{
			if (Config.bEnabled && !Config.PropertyName.IsNone())
			{
				ExistingNames.Add(Config.PropertyName);
			}
		}

		// Add new configs for each registry entry that supports output and isn't already configured
		int32 AddedCount = 0;
		for (const FPCGExPropertyRegistryEntry& Entry : Registry)
		{
			if (Entry.bSupportsOutput && !ExistingNames.Contains(Entry.PropertyName))
			{
				FPCGExPropertyOutputConfig& NewConfig = Configs.AddDefaulted_GetRef();
				NewConfig.bEnabled = true;
				NewConfig.PropertyName = Entry.PropertyName;
				// OutputAttributeName left empty - will use PropertyName as default
				AddedCount++;
			}
		}

		return AddedCount;
	}
};

/**
 * Interface for providing properties to the property writer.
 * Implement this to customize how properties are looked up per-point.
 *
 * IMPLEMENTING A CUSTOM PROVIDER:
 *
 * This is needed when you want to use FPCGExPropertyWriter in your own node.
 * The provider abstracts how properties are stored so the writer can work generically.
 *
 *   class FMyProvider : public IPCGExPropertyProvider
 *   {
 *       // Return properties for a given source index (e.g., collection entry, row)
 *       TConstArrayView<FInstancedStruct> GetProperties(int32 Index) const override;
 *
 *       // Return the registry (built once during init via PCGExProperties::BuildRegistry)
 *       TConstArrayView<FPCGExPropertyRegistryEntry> GetPropertyRegistry() const override;
 *
 *       // Find a prototype property by name (used to clone writer instances)
 *       const FInstancedStruct* FindPrototypeProperty(FName PropertyName) const override;
 *   };
 *
 * The "prototype" property is cloned by the writer during Initialize() to create
 * writer instances that own their output buffers. The actual per-point values
 * come from GetProperties(SourceIndex) during WriteProperties().
 */
class PCGEXPROPERTIES_API IPCGExPropertyProvider
{
public:
	virtual ~IPCGExPropertyProvider() = default;

	/**
	 * Get the properties for a specific index (e.g., module index, entry index).
	 * @param Index The index to get properties for
	 * @return Array view of property instanced structs
	 */
	virtual TConstArrayView<FInstancedStruct> GetProperties(int32 Index) const = 0;

	/**
	 * Get the property registry for this provider.
	 * Used to find prototype properties for writer initialization.
	 */
	virtual TConstArrayView<FPCGExPropertyRegistryEntry> GetPropertyRegistry() const = 0;

	/**
	 * Find a prototype property by name from the provider.
	 * @param PropertyName The name to search for
	 * @return Pointer to FInstancedStruct if found, nullptr otherwise
	 */
	virtual const FInstancedStruct* FindPrototypeProperty(FName PropertyName) const = 0;
};

/**
 * Generic helper for writing property data to point attributes.
 * Orchestrates property initialization and per-point writing using the
 * property-owned output interface.
 *
 * This is a general-purpose writer that works with any IPCGExPropertyProvider.
 * For Valency-specific needs, use FPCGExValencyPropertyWriter which adds
 * module tags support.
 *
 * LIFECYCLE:
 *
 *   // 1. Boot phase (single-threaded):
 *   FPCGExPropertyWriter Writer;
 *   Writer.Initialize(MyProvider, OutputFacade, OutputSettings);
 *   // Initialize() clones prototype properties, creates output buffers.
 *   // Returns false if no outputs were successfully initialized.
 *
 *   // 2. Processing phase (per-point, potentially parallel):
 *   Writer.WriteProperties(PointIndex, SourceIndex);
 *   // Looks up properties from provider at SourceIndex,
 *   // copies values into writer instances, writes to buffers.
 *
 * NOTE: WriteProperties() uses CopyValueFrom + WriteOutput internally,
 * which is NOT thread-safe. If you need parallel writes, access the
 * property's WriteOutputFrom() directly instead.
 *
 * Custom property types work transparently with this writer - no changes needed here.
 */
class PCGEXPROPERTIES_API FPCGExPropertyWriter
{
public:
	FPCGExPropertyWriter() = default;

	/**
	 * Initialize writers from a property provider using output settings.
	 * Creates writer instances for each configured property.
	 * Call during OnProcessingPreparationComplete or similar boot phase.
	 *
	 * @param InProvider The property provider to use
	 * @param OutputFacade The facade to create writers on
	 * @param OutputSettings The property output settings
	 * @return true if at least one output was initialized
	 */
	bool Initialize(
		const IPCGExPropertyProvider* InProvider,
		const TSharedRef<PCGExData::FFacade>& OutputFacade,
		const FPCGExPropertyOutputSettings& OutputSettings
	);

	/**
	 * Write property values for a resolved index to a point.
	 * Call during ProcessRange for each point.
	 *
	 * @param PointIndex The point index to write to
	 * @param SourceIndex The source index to get properties from (e.g., module index, entry index)
	 */
	void WriteProperties(int32 PointIndex, int32 SourceIndex);

	/** Check if this writer has any active outputs */
	bool HasOutputs() const;

protected:
	/** Cached output settings */
	FPCGExPropertyOutputSettings Settings;

	/** Cached reference to property provider */
	const IPCGExPropertyProvider* Provider = nullptr;

	/**
	 * Per-property writer instances.
	 * Key = PropertyName, Value = cloned property (owns its OutputBuffer).
	 *
	 * Each writer instance is a deep copy of the prototype property from the provider.
	 * The clone's InitializeOutput() is called during Initialize() to create the buffer.
	 * During WriteProperties(), values are copied from source into the clone, then written.
	 */
	TMap<FName, FInstancedStruct> WriterInstances;
};
