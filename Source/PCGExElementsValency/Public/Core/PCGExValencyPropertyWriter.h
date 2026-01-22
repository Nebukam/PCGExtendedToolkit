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

	/** Property name to output (must match a property in bonding rules) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	FName PropertyName;

	/** Attribute name for output (if empty, uses PropertyName) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
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
		return !PropertyName.IsNone() && !GetEffectiveOutputName().IsNone();
	}
};

/**
 * Configuration for property output (tags output).
 */
struct PCGEXELEMENTSVALENCY_API FPCGExValencyPropertyWriterConfig
{
	/** Output module tags as comma-separated string attribute */
	bool bOutputTags = false;

	/** Attribute name for tags output */
	FName TagsAttributeName = FName("ModuleTags");
};

/**
 * Helper for writing property data to point attributes.
 * Orchestrates property initialization and per-point writing using the
 * property-owned output interface.
 *
 * Usage:
 * 1. Create instance
 * 2. Call Initialize() during boot phase with output configs
 * 3. Call WriteModuleProperties() during processing for each point
 */
class PCGEXELEMENTSVALENCY_API FPCGExValencyPropertyWriter
{
public:
	FPCGExValencyPropertyWriter() = default;
	explicit FPCGExValencyPropertyWriter(const FPCGExValencyPropertyWriterConfig& InConfig);

	/**
	 * Initialize writers from compiled rules with output configs.
	 * Creates writer instances for each configured property.
	 * Call during OnProcessingPreparationComplete or similar boot phase.
	 *
	 * @param InCompiledRules The compiled bonding rules to scan
	 * @param OutputFacade The facade to create writers on
	 * @param OutputConfigs Array of property output configurations
	 * @return true if at least one output was initialized
	 */
	bool Initialize(
		const FPCGExValencyBondingRulesCompiled* InCompiledRules,
		const TSharedRef<PCGExData::FFacade>& OutputFacade,
		const TArray<FPCGExValencyPropertyOutputConfig>& OutputConfigs
	);

	/**
	 * Legacy initialize for tags-only output.
	 * @deprecated Use Initialize with OutputConfigs instead
	 */
	bool Initialize(
		const FPCGExValencyBondingRulesCompiled* InCompiledRules,
		const TSharedRef<PCGExData::FFacade>& OutputFacade
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

	/** Get the tags writer (for legacy support) */
	TSharedPtr<PCGExData::TBuffer<FString>> GetTagsWriter() const { return TagsWriter; }

protected:
	FPCGExValencyPropertyWriterConfig Config;

	/** Cached reference to compiled rules */
	const FPCGExValencyBondingRulesCompiled* CompiledRules = nullptr;

	/**
	 * Per-property writer instances.
	 * Key = PropertyName, Value = cloned property used as writer.
	 */
	TMap<FName, FInstancedStruct> WriterInstances;

	/** Tags writer (legacy) */
	TSharedPtr<PCGExData::TBuffer<FString>> TagsWriter;

	/**
	 * Find a prototype property from any module.
	 * Used to create writer instances.
	 */
	const FInstancedStruct* FindPrototypeProperty(FName PropertyName) const;
};
