// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExData.h"
#include "PCGExValencyBondingRules.h"

/**
 * Configuration for property output.
 */
struct PCGEXELEMENTSVALENCY_API FPCGExValencyPropertyWriterConfig
{
	/** Output metadata property keys as string attributes */
	bool bOutputMetadata = false;

	/** Output module tags as comma-separated string attribute */
	bool bOutputTags = false;

	/** Attribute name for tags output */
	FName TagsAttributeName = FName("ModuleTags");
};

/**
 * Helper for writing cage property data to point attributes.
 * Reusable across staging, pattern matching, and other valency nodes.
 *
 * Usage:
 * 1. Create instance with config
 * 2. Call Initialize() during boot phase to create writers
 * 3. Call WriteModuleProperties() during processing for each point
 */
class PCGEXELEMENTSVALENCY_API FPCGExValencyPropertyWriter
{
public:
	FPCGExValencyPropertyWriter() = default;
	explicit FPCGExValencyPropertyWriter(const FPCGExValencyPropertyWriterConfig& InConfig);

	/**
	 * Initialize writers by scanning compiled rules for property names.
	 * Call during OnProcessingPreparationComplete or similar boot phase.
	 *
	 * @param CompiledRules The compiled bonding rules to scan
	 * @param OutputFacade The facade to create writers on
	 * @return true if initialization succeeded
	 */
	bool Initialize(
		const FPCGExValencyBondingRulesCompiled* CompiledRules,
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

	/** Get the metadata writers map (for forwarding to processors) */
	const TMap<FName, TSharedPtr<PCGExData::TBuffer<FString>>>& GetMetadataWriters() const { return MetadataWriters; }

	/** Get the tags writer (for forwarding to processors) */
	TSharedPtr<PCGExData::TBuffer<FString>> GetTagsWriter() const { return TagsWriter; }

protected:
	FPCGExValencyPropertyWriterConfig Config;

	/** Cached reference to compiled rules */
	const FPCGExValencyBondingRulesCompiled* CompiledRules = nullptr;

	/** Per-metadata-key writers */
	TMap<FName, TSharedPtr<PCGExData::TBuffer<FString>>> MetadataWriters;

	/** Tags writer */
	TSharedPtr<PCGExData::TBuffer<FString>> TagsWriter;
};
