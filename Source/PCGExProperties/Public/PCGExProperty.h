// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Data/PCGExData.h"

#include "PCGExProperty.generated.h"

/**
 * Entry in the property registry.
 * Built at compile time to provide a read-only view of available properties.
 *
 * The registry is used by:
 * - FPCGExPropertyOutputSettings::AutoPopulateFromRegistry() to auto-create output configs
 * - UI systems to display available property types and their capabilities
 *
 * Custom property types are automatically included when BuildRegistry() is called
 * on an FInstancedStruct array containing your type.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyRegistryEntry
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
 * Base struct for all PCGEx property types.
 *
 * ============================================================================
 * CREATING A CUSTOM PROPERTY TYPE
 * ============================================================================
 *
 * To add a new property type that integrates with the entire plugin:
 *
 * 1. DEFINE your struct deriving from FPCGExProperty in PCGExPropertyTypes.h
 *    (or in your own module if it has special dependencies):
 *
 *    USTRUCT(BlueprintType, meta=(PCGExInlineValue))
 *    struct FPCGExProperty_MyType : public FPCGExProperty
 *    {
 *        GENERATED_BODY()
 *
 *        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
 *        FMyValueType Value;  // <-- your authored value
 *
 *    protected:
 *        TSharedPtr<PCGExData::TBuffer<FMyOutputType>> OutputBuffer;
 *        // Note: OutputType can differ from Value type (see Color: FLinearColor -> FVector4)
 *
 *    public:
 *        // Override ALL virtual methods listed below.
 *    };
 *
 * 2. IMPLEMENT the methods. For simple 1:1 type mappings, use PCGEX_PROPERTY_IMPL
 *    macro in the .cpp (see PCGExPropertyTypes.cpp). For type conversions, implement
 *    each method manually (see Color and Enum for examples).
 *
 * 3. The USTRUCT meta=(PCGExInlineValue) tag is optional - it controls how the
 *    property appears in the FInstancedStruct picker UI.
 *
 * 4. No registration step is needed. UHT discovers the USTRUCT automatically.
 *    Your type will appear in any FInstancedStruct picker that uses
 *    meta=(BaseStruct="/Script/PCGExProperties.PCGExProperty").
 *
 * ============================================================================
 * TWO OUTPUT PATHS
 * ============================================================================
 *
 * Properties support two independent output mechanisms:
 *
 * A) POINT ATTRIBUTE OUTPUT (via FPCGExPropertyWriter):
 *    InitializeOutput() -> creates a TBuffer on a facade
 *    WriteOutput()      -> writes Value to buffer at a point index
 *    WriteOutputFrom()  -> writes from source property directly (thread-safe)
 *    Used by: Collections, Distribute Tuple, any node outputting properties to points
 *
 * B) METADATA ATTRIBUTE OUTPUT (via Tuple node):
 *    CreateMetadataAttribute() -> creates attribute on UPCGParamData
 *    WriteMetadataValue()      -> writes Value to a metadata entry key
 *    Used by: Tuple node for creating param data tables
 *
 * Both paths are optional. Return false/nullptr from SupportsOutput()/
 * CreateMetadataAttribute() if your type doesn't support a path.
 *
 * ============================================================================
 * THREAD SAFETY
 * ============================================================================
 *
 * - WriteOutputFrom() is the ONLY method safe for parallel processing loops.
 *   It reads from Source and writes directly to the buffer without mutating 'this'.
 * - CopyValueFrom() + WriteOutput() is NOT thread-safe (mutates Value field).
 *   Only use this pattern in single-threaded contexts.
 * - InitializeOutput() must be called during boot phase (single-threaded).
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExProperty
{
	GENERATED_BODY()

	/**
	 * User-defined name for disambiguation when multiple properties exist.
	 * This name is used to match properties across schemas, overrides, and output configs.
	 * Must be unique within a schema collection.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Settings, meta=(DisplayPriority = -1))
	FName PropertyName;

	#if WITH_EDITORONLY_DATA
	/**
	 * Stable identity for override matching across schema changes.
	 * Auto-generated on construction, preserved by FPCGExPropertySchema through:
	 * - Property renames (HeaderId stays same, overrides follow)
	 * - Property reordering (HeaderId stays same, values stay correct)
	 * - Type changes (HeaderId preserved, bEnabled state preserved, value reset to default)
	 * Custom properties inherit this automatically - no action needed.
	 */
	UPROPERTY(meta=(IgnoreForMemberInitializationTest))
	int32 HeaderId = 0;
	#endif

	FPCGExProperty()
	{
		#if WITH_EDITOR
		HeaderId = GetTypeHash(FGuid::NewGuid());
		#endif
	}

	virtual ~FPCGExProperty() = default;

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
	 * WARNING: Not thread-safe if Value was modified. Use WriteOutputFrom() for parallel processing.
	 * @param PointIndex The point index to write to
	 */
	virtual void WriteOutput(int32 PointIndex) const {}

	/**
	 * Thread-safe: Write value from source property directly to buffer.
	 * Use this in parallel processing (PCGEX_SCOPE_LOOP) instead of CopyValueFrom + WriteOutput.
	 * @param PointIndex The point index to write to
	 * @param Source The source property to read value from (must be same concrete type)
	 */
	virtual void WriteOutputFrom(int32 PointIndex, const FPCGExProperty* Source) const {}

	/**
	 * Copy value from another property of the same type.
	 * WARNING: Not thread-safe. Mutates this property's Value field.
	 * For parallel processing, use WriteOutputFrom() instead.
	 * @param Source The source property to copy from (must be same concrete type)
	 */
	virtual void CopyValueFrom(const FPCGExProperty* Source) {}

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

	// --- Metadata Interface (for Tuple/ParamData) ---

	/**
	 * Create a metadata attribute on param data.
	 * Override in derived types that support metadata output (most types do).
	 * @param Metadata The metadata to create attribute on
	 * @param AttributeName The attribute name to use
	 * @return Pointer to created attribute, or nullptr if failed
	 */
	virtual FPCGMetadataAttributeBase* CreateMetadataAttribute(UPCGMetadata* Metadata, FName AttributeName) const { return nullptr; }

	/**
	 * Write this property's value to a metadata attribute.
	 * @param Attribute The attribute to write to (must match type)
	 * @param EntryKey The metadata entry key to write to
	 */
	virtual void WriteMetadataValue(FPCGMetadataAttributeBase* Attribute, int64 EntryKey) const {}

	/**
	 * Copy default value from another property (for Tuple header initialization).
	 * Similar to CopyValueFrom but called during header initialization.
	 * @param Source The source property to copy from
	 */
	virtual void InitializeFrom(const FPCGExProperty* Source) { CopyValueFrom(Source); }

	// --- Registry ---

	/**
	 * Create a registry entry for this property.
	 */
	FPCGExPropertyRegistryEntry ToRegistryEntry() const
	{
		return FPCGExPropertyRegistryEntry(PropertyName, GetTypeName(), GetOutputType(), SupportsOutput());
	}
};

/**
 * Single property override entry.
 * Stores enabled state + typed value. PropertyName comes from the inner struct.
 *
 * Override entries are kept in parallel arrays with the schema:
 * - Schema[0] <-> Override[0], Schema[1] <-> Override[1], etc.
 * - This enables efficient per-column iteration and index-based access.
 * - SyncToSchema() maintains this parallel structure automatically.
 *
 * Custom properties work transparently here - the FInstancedStruct Value
 * holds any FPCGExProperty derivative polymorphically.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyOverrideEntry
{
	GENERATED_BODY()

	/** Whether this override is active (false = use collection default) */
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bEnabled = false;

	/** The typed property value (contains PropertyName internally) */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(BaseStruct="/Script/PCGExProperties.PCGExProperty", ExcludeBaseStruct, EditCondition="bEnabled"))
	FInstancedStruct Value;

	FPCGExPropertyOverrideEntry() = default;

	explicit FPCGExPropertyOverrideEntry(const FInstancedStruct& InValue, bool bInEnabled = false)
		: bEnabled(bInEnabled)
		, Value(InValue)
	{
	}

	/** Get the property name from the inner struct */
	FName GetPropertyName() const
	{
		if (const FPCGExProperty* Prop = Value.GetPtr<FPCGExProperty>())
		{
			return Prop->PropertyName;
		}
		return NAME_None;
	}

	/** Get the property from Value (may be nullptr) */
	const FPCGExProperty* GetProperty() const
	{
		return Value.GetPtr<FPCGExProperty>();
	}

	FPCGExProperty* GetPropertyMutable()
	{
		return Value.GetMutablePtr<FPCGExProperty>();
	}

	bool IsValid() const
	{
		return Value.IsValid() && !GetPropertyName().IsNone();
	}
};

/**
 * Wrapper struct for property overrides array.
 * Used by Collections (entry-level overrides) and Tuple (row values).
 *
 * The Overrides array is kept parallel with the schema array:
 * - Same size, same order as the schema that created it
 * - Each entry has bEnabled flag to toggle that column for this row
 * - Disabled entries use collection/schema defaults
 *
 * USAGE PATTERN (for nodes that use properties):
 *
 *   // In your settings class:
 *   FPCGExPropertySchemaCollection MySchema;           // Define columns
 *   TArray<FPCGExPropertyOverrides> MyRows;            // Row values
 *
 *   // In PostEditChangeProperty:
 *   MySchema.SyncOverridesArray(MyRows);               // Keep rows in sync
 *
 *   // At runtime, read values:
 *   for (int Col = 0; Col < MySchema.Num(); ++Col) {
 *       if (MyRows[RowIdx].IsOverrideEnabled(Col)) {
 *           const FPCGExProperty* Prop = MyRows[RowIdx].Overrides[Col].GetProperty();
 *           // Use Prop->Value...
 *       }
 *   }
 *
 * Schema Source: The editor customization looks for a "CollectionProperties" or "Properties"
 * property on the outer object to determine available property types for the picker.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyOverrides
{
	GENERATED_BODY()

	/** Overrides array - parallel with schema (same size, same order) */
	UPROPERTY(EditAnywhere, Category = Settings)
	TArray<FPCGExPropertyOverrideEntry> Overrides;

	/**
	 * Sync overrides to match schema - ensures parallel array structure.
	 *
	 * This is the core mechanism that keeps overrides aligned with their schema.
	 * In the editor, it uses HeaderId for stable matching:
	 * - Existing overrides matched by HeaderId preserve their bEnabled state
	 * - Same-type matches also preserve the override value
	 * - Type changes preserve bEnabled but reset the value to schema default
	 * - New properties (no HeaderId match) are added as disabled
	 *
	 * At runtime (no editor data), overrides are rebuilt from schema defaults.
	 *
	 * @param Schema The schema to sync to (use FPCGExPropertySchemaCollection::BuildSchema())
	 */
	void SyncToSchema(const TArray<FInstancedStruct>& Schema);

	/** Check if override at index is enabled */
	bool IsOverrideEnabled(int32 Index) const
	{
		return Overrides.IsValidIndex(Index) && Overrides[Index].bEnabled;
	}

	/** Set override enabled state at index */
	void SetOverrideEnabled(int32 Index, bool bEnabled)
	{
		if (Overrides.IsValidIndex(Index))
		{
			Overrides[Index].bEnabled = bEnabled;
		}
	}

	/** Check if an enabled override exists for the given property name */
	bool HasOverride(FName PropertyName) const
	{
		return GetOverride(PropertyName) != nullptr;
	}

	/** Get enabled override by name (returns nullptr if not found or disabled) */
	const FInstancedStruct* GetOverride(FName PropertyName) const;

	/** Count enabled overrides */
	int32 GetEnabledCount() const
	{
		int32 Count = 0;
		for (const FPCGExPropertyOverrideEntry& Entry : Overrides)
		{
			if (Entry.bEnabled) { ++Count; }
		}
		return Count;
	}

	/**
	 * Get typed property from enabled overrides by name.
	 * @param PropertyName The property name to search for
	 * @return Pointer to typed property if found and enabled, nullptr otherwise
	 */
	template <typename T>
	const T* GetProperty(FName PropertyName) const
	{
		static_assert(TIsDerivedFrom<T, FPCGExProperty>::Value,
			"T must derive from FPCGExProperty");

		for (const FPCGExPropertyOverrideEntry& Entry : Overrides)
		{
			if (Entry.bEnabled && Entry.GetPropertyName() == PropertyName)
			{
				if (const T* Typed = Entry.Value.GetPtr<T>())
				{
					return Typed;
				}
			}
		}
		return nullptr;
	}
};

/**
 * Schema entry for property definitions.
 * Used by Collections, Valency, and Tuple to define available properties with stable identity.
 *
 * A schema entry binds together:
 * - A Name (shown in UI, used as attribute name for output)
 * - A Property (FInstancedStruct holding any FPCGExProperty derivative)
 * - A HeaderId (editor-only, for stable override matching)
 *
 * HeaderId is preserved through type changes (stored outside FInstancedStruct), enabling:
 * - Rename property -> HeaderId stays same -> override state preserved
 * - Reorder properties -> HeaderId stays same -> values stay correct
 * - Change type -> HeaderId preserved -> bEnabled state preserved, value reset
 *
 * The FInstancedStruct picker is constrained via meta=(BaseStruct=".../PCGExProperty",
 * ExcludeBaseStruct) so only concrete property types appear in the dropdown.
 * Custom property types automatically appear here once their USTRUCT is compiled.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertySchema
{
	GENERATED_BODY()

	#if WITH_EDITORONLY_DATA
	/** Stable identity for override matching, preserved through type changes */
	UPROPERTY(meta=(IgnoreForMemberInitializationTest))
	int32 HeaderId = 0;
	#endif

	/** Property name (shown in UI, used for attribute output) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FName Name = NAME_None;

	/** The typed property definition */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(BaseStruct="/Script/PCGExProperties.PCGExProperty", ExcludeBaseStruct, ShowOnlyInnerProperties))
	FInstancedStruct Property;

	FPCGExPropertySchema();  // Implemented in .cpp (needs full type definitions)

	/** Sync Name to Property.PropertyName and HeaderId */
	void SyncPropertyName()
	{
		if (FPCGExProperty* Prop = GetPropertyMutable())
		{
			Prop->PropertyName = Name;
			#if WITH_EDITOR
			Prop->HeaderId = HeaderId;
			#endif
		}
	}

	/** Get the property from Property (may be nullptr) */
	const FPCGExProperty* GetProperty() const
	{
		return Property.GetPtr<FPCGExProperty>();
	}

	FPCGExProperty* GetPropertyMutable()
	{
		return Property.GetMutablePtr<FPCGExProperty>();
	}

	bool IsValid() const
	{
		return Property.IsValid() && !Name.IsNone();
	}
};

/**
 * Collection of property schemas with embedded utilities.
 * This is the primary container for defining a set of typed properties.
 *
 * Used by:
 * - Tuple node (Composition field) - defines columns of a param data table
 * - Collections (CollectionProperties) - defines per-entry properties on asset collections
 * - Valency (via UPCGExPropertyCollectionComponent) - defines cage/pattern properties
 * - Any custom node that needs user-definable typed properties
 *
 * INTEGRATING INTO YOUR OWN NODE:
 *
 *   // In your UPCGExSettings subclass:
 *   UPROPERTY(EditAnywhere, Category = Settings)
 *   FPCGExPropertySchemaCollection MyProperties;
 *
 *   // If you have override rows (like Tuple):
 *   UPROPERTY(EditAnywhere, Category = Settings)
 *   TArray<FPCGExPropertyOverrides> MyValues;
 *
 *   // In PostEditChangeProperty, sync on any schema change:
 *   MyProperties.SyncOverridesArray(MyValues);
 *
 *   // At runtime, access properties:
 *   const auto* FloatProp = MyProperties.GetProperty<FPCGExProperty_Float>(FName("MyFloat"));
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertySchemaCollection
{
	GENERATED_BODY()

	/** Schema array */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(TitleProperty="{Name}"))
	TArray<FPCGExPropertySchema> Schemas;

	/** Find schema by property name */
	const FPCGExPropertySchema* FindByName(FName PropertyName) const;

	/** Check if property exists by name */
	bool HasProperty(FName PropertyName) const { return FindByName(PropertyName) != nullptr; }

	/** Get property instance by name (returns FInstancedStruct for compatibility with existing code) */
	const FInstancedStruct* GetPropertyByName(FName PropertyName) const
	{
		const FPCGExPropertySchema* Schema = FindByName(PropertyName);
		return Schema ? &Schema->Property : nullptr;
	}

	/** Build FInstancedStruct array for SyncToSchema calls */
	TArray<FInstancedStruct> BuildSchema() const;

	/** Validate all property names are unique (returns true if valid) */
	bool ValidateUniqueNames(TArray<FName>& OutDuplicates) const;

	/** Get typed property by name */
	template <typename T>
	const T* GetProperty(FName PropertyName) const
	{
		static_assert(TIsDerivedFrom<T, FPCGExProperty>::Value,
			"T must derive from FPCGExProperty");

		const FPCGExPropertySchema* Schema = FindByName(PropertyName);
		return Schema ? Schema->Property.GetPtr<T>() : nullptr;
	}

	/** Count valid schemas */
	int32 Num() const { return Schemas.Num(); }
	bool IsEmpty() const { return Schemas.IsEmpty(); }

	/**
	 * Sync all schemas - updates PropertyName and HeaderId into each Property.
	 * Call this before BuildSchema() to ensure schema has current data.
	 */
	void SyncAllSchemas();

	/**
	 * Sync a single PropertyOverrides instance to this schema.
	 * Convenience method that calls BuildSchema() then SyncToSchema().
	 */
	void SyncOverrides(FPCGExPropertyOverrides& Overrides);

	/**
	 * Sync an array of PropertyOverrides to this schema.
	 * Convenience method that syncs all schemas then syncs each override.
	 */
	void SyncOverridesArray(TArray<FPCGExPropertyOverrides>& OverridesArray);
};

/**
 * Property overrides with per-row weight for distribution.
 * Used by Tuple : Distribute to assign weighted probability to each row.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExWeightedPropertyOverrides : public FPCGExPropertyOverrides
{
	GENERATED_BODY()

	/** Weight for this row in distribution (higher = more likely to be picked) */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(ClampMin=0, UIMin=0))
	int32 Weight = 1;
};

/**
 * Query helpers for accessing properties from FInstancedStruct arrays.
 * All functions accept TConstArrayView to work with both TArray and TArrayView.
 *
 * These are the primary runtime lookup functions. Use them when you have
 * a flat array of FInstancedStruct (e.g., from BuildSchema() or a provider).
 *
 * For lookups on FPCGExPropertySchemaCollection, prefer its member methods
 * (FindByName, GetProperty<T>) which operate on the schema directly.
 */
namespace PCGExProperties
{
	/**
	 * Get first property of specified type, optionally filtered by name.
	 * @param Properties - Array view of FInstancedStruct containing properties
	 * @param PropertyName - Optional name filter (NAME_None matches any)
	 * @return Pointer to property if found, nullptr otherwise
	 */
	template <typename T>
	const T* GetProperty(TConstArrayView<FInstancedStruct> Properties, FName PropertyName = NAME_None)
	{
		static_assert(TIsDerivedFrom<T, FPCGExProperty>::Value,
			"T must derive from FPCGExProperty");

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
	 * @param Properties - Array view of FInstancedStruct containing properties
	 * @return Array of pointers to matching properties
	 */
	template <typename T>
	TArray<const T*> GetAllProperties(TConstArrayView<FInstancedStruct> Properties)
	{
		static_assert(TIsDerivedFrom<T, FPCGExProperty>::Value,
			"T must derive from FPCGExProperty");

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
	 * @param Properties - Array view of FInstancedStruct containing properties
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
			if (const FPCGExProperty* Base = Prop.GetPtr<FPCGExProperty>())
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

	/**
	 * Build a registry from an array of property instanced structs.
	 * @param Properties - Array of FInstancedStruct containing FPCGExProperty derivatives
	 * @param OutRegistry - Output array to populate with registry entries
	 */
	inline void BuildRegistry(TConstArrayView<FInstancedStruct> Properties, TArray<FPCGExPropertyRegistryEntry>& OutRegistry)
	{
		OutRegistry.Empty(Properties.Num());
		for (const FInstancedStruct& Prop : Properties)
		{
			if (const FPCGExProperty* Base = Prop.GetPtr<FPCGExProperty>())
			{
				OutRegistry.Add(Base->ToRegistryEntry());
			}
		}
	}
}
