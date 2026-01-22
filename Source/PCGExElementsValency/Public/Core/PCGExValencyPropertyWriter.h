// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExData.h"
#include "Helpers/PCGExMetaHelpers.h"
#include "PCGExValencyBondingRules.h"

#include "PCGExValencyPropertyWriter.generated.h"

/**
 * Configuration for a single property output.
 * Associates a property (by name) with an output attribute name.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExValencyPropertyOutputConfig
{
	GENERATED_BODY()

	/** Whether this output config is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	bool bEnabled = true;

	/** Property name to output (must match a property in bonding rules) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta=(EditCondition="bEnabled"))
	FName PropertyName;

	/** Attribute name for output (if empty, uses PropertyName) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta=(EditCondition="bEnabled"))
	FName OutputAttributeName;

	/**
	 * Get effective output name, validated for PCG compatibility.
	 * Returns NAME_None if the name is invalid.
	 */
	FName GetEffectiveOutputName() const
	{
		FName Name = OutputAttributeName.IsNone() ? PropertyName : OutputAttributeName;
		if (Name.IsNone() || !PCGExMetaHelpers::IsWritableAttributeName(Name))
		{
			return NAME_None;
		}
		return Name;
	}

	bool IsValid() const
	{
		return bEnabled && !PropertyName.IsNone() && !GetEffectiveOutputName().IsNone();
	}
};

/**
 * Reusable settings struct for property output configuration.
 * Can be embedded in any node that needs to output cage properties.
 * Includes both individual property configs and module tags output.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExValencyPropertyOutputSettings
{
	GENERATED_BODY()

	/**
	 * Properties to output as point attributes.
	 * Each config maps a cage property name to an output attribute name.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property Output", meta=(TitleProperty="PropertyName"))
	TArray<FPCGExValencyPropertyOutputConfig> Configs;

	/**
	 * If enabled, outputs module actor tags as a single comma-separated string attribute.
	 * Tags are inherited from cage + palette sources.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property Output")
	bool bOutputModuleTags = false;

	/** Attribute name for the module tags output */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property Output", meta=(EditCondition="bOutputModuleTags"))
	FName ModuleTagsAttributeName = FName("ModuleTags");

	/** Check if any outputs are configured */
	bool HasOutputs() const
	{
		if (bOutputModuleTags) { return true; }
		for (const FPCGExValencyPropertyOutputConfig& Config : Configs)
		{
			if (Config.IsValid()) { return true; }
		}
		return false;
	}

	/**
	 * Auto-populate configs from compiled bonding rules.
	 * Adds configs for all unique properties that support output.
	 * Skips properties already configured (enabled configs only).
	 * @param CompiledRules The compiled bonding rules to scan
	 * @return Number of configs added
	 */
	int32 AutoPopulateFromRules(const FPCGExValencyBondingRulesCompiled* CompiledRules);
};

/**
 * Helper for writing property data to point attributes.
 * Orchestrates property initialization and per-point writing using the
 * property-owned output interface.
 *
 * Usage:
 * 1. Create instance with settings
 * 2. Call Initialize() during boot phase
 * 3. Call WriteModuleProperties() during processing for each point
 */
class PCGEXELEMENTSVALENCY_API FPCGExValencyPropertyWriter
{
public:
	FPCGExValencyPropertyWriter() = default;

	/**
	 * Initialize writers from compiled rules using output settings.
	 * Creates writer instances for each configured property.
	 * Call during OnProcessingPreparationComplete or similar boot phase.
	 *
	 * @param InCompiledRules The compiled bonding rules to scan
	 * @param OutputFacade The facade to create writers on
	 * @param OutputSettings The property output settings
	 * @return true if at least one output was initialized
	 */
	bool Initialize(
		const FPCGExValencyBondingRulesCompiled* InCompiledRules,
		const TSharedRef<PCGExData::FFacade>& OutputFacade,
		const FPCGExValencyPropertyOutputSettings& OutputSettings
	);

	/**
	 * Write property values for a resolved module to a point.
	 * Call during ProcessRange for each point.
	 *
	 * @param PointIndex The point index to write to
	 * @param ModuleIndex The resolved module index
	 */
	void WriteModuleProperties(int32 PointIndex, int32 ModuleIndex);

	/** Check if this writer has any active outputs */
	bool HasOutputs() const;

	/** Get the tags writer */
	TSharedPtr<PCGExData::TBuffer<FString>> GetTagsWriter() const { return TagsWriter; }

protected:
	/** Cached output settings */
	FPCGExValencyPropertyOutputSettings Settings;

	/** Cached reference to compiled rules */
	const FPCGExValencyBondingRulesCompiled* CompiledRules = nullptr;

	/**
	 * Per-property writer instances.
	 * Key = PropertyName, Value = cloned property used as writer.
	 */
	TMap<FName, FInstancedStruct> WriterInstances;

	/** Tags writer */
	TSharedPtr<PCGExData::TBuffer<FString>> TagsWriter;

	/**
	 * Find a prototype property from any module.
	 * Used to create writer instances.
	 */
	const FInstancedStruct* FindPrototypeProperty(FName PropertyName) const;
};
