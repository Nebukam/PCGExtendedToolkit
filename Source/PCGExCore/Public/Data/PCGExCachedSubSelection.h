// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubSelection.h"
#include "Types/PCGExTypeOps.h"

/**
 * PCGEx Cached Sub-Selection Operations
 * 
 * This header provides FCachedSubSelection - a structure that caches all
 * function pointers and type operations needed for sub-selection at construction
 * time, eliminating repeated registry lookups during hot loops.
 * 
 * Usage:
 *   // At proxy construction:
 *   CachedSubSelection.Initialize(SubSelection, RealType, WorkingType);
 *   
 *   // At runtime (hot path):
 *   CachedSubSelection.ApplyGet(Source, OutValue);  // No lookups!
 *   CachedSubSelection.ApplySet(Target, Source);    // No lookups!
 */

namespace PCGExData
{
	/**
	 * Function pointer types for sub-selection operations
	 */

	// Extract field value: double = ExtractField(const void* Value, ESingleField Field)
	using FExtractFieldFn = double (*)(const void* Value, PCGExTypeOps::ESingleField Field);

	// Inject field value: void InjectField(void* Target, double Value, ESingleField Field)  
	using FInjectFieldFn = void (*)(void* Target, double Value, PCGExTypeOps::ESingleField Field);

	// Extract axis direction: FVector = ExtractAxis(const void* Rotation, EPCGExAxis Axis)
	using FExtractAxisFn = FVector (*)(const void* Value, EPCGExAxis Axis);

	// Extract transform component: void ExtractComponent(const void* Transform, void* Out)
	using FExtractComponentFn = void (*)(const void* Transform, PCGExTypeOps::ETransformPart Part, void* OutValue, EPCGMetadataTypes& OutType);

	// Inject transform component: void InjectComponent(void* Transform, const void* Value)
	using FInjectComponentFn = void (*)(void* Transform, PCGExTypeOps::ETransformPart Part, const void* Value, EPCGMetadataTypes ValueType);

	/**
	 * Sub-selection function implementations per type
	 * 
	 * These are the actual implementations that get cached as function pointers.
	 * Only one set exists per type (14 total), not 14x14 combinations.
	 */
	namespace SubSelectionImpl
	{
		static FORCEINLINE FVector ExtractAxisDefault(const void* Value, EPCGExAxis Axis) { return FVector::ForwardVector; }

		PCGEXCORE_API FExtractFieldFn GetExtractFieldFn(EPCGMetadataTypes Type);
		PCGEXCORE_API FInjectFieldFn GetInjectFieldFn(EPCGMetadataTypes Type);
		PCGEXCORE_API FExtractAxisFn GetExtractAxisFn(EPCGMetadataTypes Type);
	}

	/**
	 * FCachedSubSelection - Pre-resolved sub-selection operations
	 * 
	 * Caches all function pointers and type operations at construction time.
	 * Designed to be embedded in IBufferProxy for zero-overhead sub-selection
	 * during hot loops.
	 * 
	 * Memory: ~80 bytes (function pointers + configuration)
	 */
	struct PCGEXCORE_API FCachedSubSelection
	{
		//
		// Configuration (copied from FSubSelection)
		//
		bool bIsValid = false;
		bool bIsFieldSet = false;
		bool bIsAxisSet = false;
		bool bIsComponentSet = false;

		PCGExTypeOps::ESingleField Field = PCGExTypeOps::ESingleField::X;
		EPCGExAxis Axis = EPCGExAxis::Forward;
		PCGExTypeOps::ETransformPart Component = PCGExTypeOps::ETransformPart::Position;

		//
		// Cached type information
		//
		EPCGMetadataTypes RealType = EPCGMetadataTypes::Unknown;
		EPCGMetadataTypes WorkingType = EPCGMetadataTypes::Unknown;
		EPCGMetadataTypes ComponentType = EPCGMetadataTypes::Unknown; // Vector or Quaternion for transform components

		//
		// Cached function pointers - resolved once at initialization
		//

		// Field operations on RealType
		FExtractFieldFn ExtractFieldFromReal = nullptr;
		FInjectFieldFn InjectFieldToReal = nullptr;

		// Field operations on WorkingType (for when we need to work with working type directly)
		FExtractFieldFn ExtractFieldFromWorking = nullptr;
		FInjectFieldFn InjectFieldToWorking = nullptr;

		// Axis extraction from RealType (rotation types only)
		FExtractAxisFn ExtractAxisFromReal = nullptr;

		// Transform component operations (transform type only)
		FExtractComponentFn ExtractComponent = nullptr;
		FInjectComponentFn InjectComponent = nullptr;

		// Conversion functions
		PCGExTypeOps::FConvertFn ConvertRealToWorking = nullptr;
		PCGExTypeOps::FConvertFn ConvertWorkingToReal = nullptr;
		PCGExTypeOps::FConvertFn ConvertWorkingToDouble = nullptr;
		PCGExTypeOps::FConvertFn ConvertDoubleToWorking = nullptr;
		PCGExTypeOps::FConvertFn ConvertRealToDouble = nullptr;
		PCGExTypeOps::FConvertFn ConvertDoubleToReal = nullptr;

		// Type ops for copy/default operations
		const PCGExTypeOps::ITypeOpsBase* RealOps = nullptr;
		const PCGExTypeOps::ITypeOpsBase* WorkingOps = nullptr;

		//
		// Constructors
		//

		FCachedSubSelection() = default;

		/**
		 * Initialize from FSubSelection and type information
		 * 
		 * This resolves all function pointers once. After this call,
		 * ApplyGet/ApplySet use only cached pointers with no lookups.
		 */
		void Initialize(const FSubSelection& Selection,
		                EPCGMetadataTypes InRealType,
		                EPCGMetadataTypes InWorkingType);

		/**
		 * Check if sub-selection applies to source reads
		 * 
		 * Returns true if reading from RealType should apply sub-selection.
		 * For scalar sources (like double), field selection doesn't apply to reads.
		 */
		bool AppliesToSourceRead() const;

		/**
		 * Check if sub-selection applies to target writes
		 * 
		 * Returns true if writing to RealType should apply sub-selection.
		 */
		bool AppliesToTargetWrite() const;

		//
		// Hot-path operations - use only cached function pointers
		//

		/**
		 * Apply sub-selection when reading (Get direction)
		 * 
		 * Reads from Source (RealType), applies sub-selection, outputs to OutValue (WorkingType).
		 * Uses only cached function pointers - no registry lookups.
		 * 
		 * @param Source Pointer to source value (RealType)
		 * @param OutValue Pointer to output buffer (WorkingType)
		 */
		void ApplyGet(const void* Source, void* OutValue) const;

		/**
		 * Apply sub-selection when writing (Set direction)
		 * 
		 * Takes Source (WorkingType), applies sub-selection, writes to Target (RealType).
		 * Uses only cached function pointers - no registry lookups.
		 * 
		 * @param Target Pointer to target value (RealType), modified in place
		 * @param Source Pointer to source value (WorkingType)
		 */
		void ApplySet(void* Target, const void* Source) const;

		/**
		 * Get without sub-selection - just convert RealType → WorkingType
		 */
		FORCEINLINE void ConvertGet(const void* Source, void* OutValue) const
		{
			if (ConvertRealToWorking) { ConvertRealToWorking(Source, OutValue); }
		}

		/**
		 * Set without sub-selection - just convert WorkingType → RealType
		 */
		FORCEINLINE void ConvertSet(void* Target, const void* Source) const
		{
			if (ConvertWorkingToReal) { ConvertWorkingToReal(Source, Target); }
		}

	private:
		// Internal helpers for complex sub-selection paths
		void ApplyGetWithComponent(const void* Source, void* OutValue) const;
		void ApplySetWithComponent(void* Target, const void* Source) const;
	};
}
