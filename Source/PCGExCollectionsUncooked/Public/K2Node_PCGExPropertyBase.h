// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "EdGraph/EdGraphPin.h"

#include "K2Node_PCGExPropertyBase.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;
class UEdGraph;
class UEdGraphPin;
class UGraphNodeContextMenuContext;
class UK2Node_CallFunction;
class UToolMenu;

/**
 * Shared machinery for the custom-property access nodes over a UPCGExAssetCollection:
 * Get/Set Entry Property, Get/Set Category Property, Get/Set Collection Property. Leaves only
 * declare their scope (which resolution tier the node addresses) and direction (get vs set);
 * everything else -- pin layout, wildcard typing, Object/Class dispatch, expansion -- lives here.
 *
 * Scopes:
 *  - Entry: Collection + EntryIndex. Reads resolve entry override -> category row -> default;
 *    writes target the entry's override slot.
 *  - Category: Collection + Category name. Reads resolve category row -> default; writes target
 *    the category's override row (minted on demand).
 *  - Collection: Collection only. Reads the schema default; writes the local schema entry or the
 *    import override standing in for an imported one.
 *
 * Get nodes are pure with a single wildcard output; Set nodes carry exec pins plus a NewValue /
 * Readback wildcard pair locked to one concrete type. The connected pin's type drives the
 * EPCGMetadataTypes conversion at runtime; any concrete type can also be picked from the wildcard
 * pin's right-click menu. Object/Class pins compile to dedicated well-typed library functions
 * because they don't round-trip through the CustomStructureParam wildcard (see
 * UPCGExCollectionEntryBlueprintLibrary).
 */
UCLASS(Abstract)
class PCGEXCOLLECTIONSUNCOOKED_API UK2Node_PCGExPropertyBase : public UK2Node
{
	GENERATED_BODY()

public:
	enum class EScope : uint8
	{
		Entry,
		Category,
		Collection
	};

	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;

	// UK2Node
	virtual bool IsNodePure() const override
	{
		return !IsSetNode();
	}

	virtual FText GetMenuCategory() const override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

protected:
	/** True for Set nodes (exec pins, NewValue input + Readback output); false for pure Get nodes. */
	virtual bool IsSetNode() const PURE_VIRTUAL(UK2Node_PCGExPropertyBase::IsSetNode, return false;);

	/** Which resolution tier the node addresses; decides the key pin and the backing functions. */
	virtual EScope GetScope() const PURE_VIRTUAL(UK2Node_PCGExPropertyBase::GetScope, return EScope::Entry;);

	/**
	 * Persisted resolved type of the wildcard pin(s). AllocateDefaultPins re-stamps it on graph
	 * reload so manually-picked types (right-click menu) survive save/reopen even with no
	 * connections. Wildcard until resolved.
	 */
	UPROPERTY()
	FEdGraphPinType ResolvedPinType;

	// Pin accessors. GetKeyPin is EntryIndex (Entry), Category (Category) or null (Collection).
	UEdGraphPin* GetExecInputPin() const;
	UEdGraphPin* GetThenPin() const;
	UEdGraphPin* GetCollectionPin() const;
	UEdGraphPin* GetKeyPin() const;
	UEdGraphPin* GetPropertyNamePin() const;
	/** OutValue on Get nodes, NewValue on Set nodes. */
	UEdGraphPin* GetValuePin() const;
	/** Set nodes only. */
	UEdGraphPin* GetReadbackPin() const;
	UEdGraphPin* GetSuccessPin() const;

	/** The wildcard pins that share one concrete type: {OutValue} or {NewValue, Readback}. */
	void GetWildcardPins(TArray<UEdGraphPin*>& OutPins) const;

	/** Revert the wildcard pins to PC_Wildcard, breaking their links. */
	void ResetWildcardPins();
	/** Adopt another pin's type (container stripped) onto the wildcard pins. */
	void AdoptTypeFromOther(const UEdGraphPin* OtherPin);
	/** Stamp a specific type onto the wildcard pins (right-click menu), breaking incompatible links. */
	void SetWildcardPinsType(const FEdGraphPinType& InType);

	/** Key pin name for a scope: EntryIndex, Category, or None for the collection scope. */
	static FName GetKeyPinName(EScope Scope);
	/** "Entry" / "Category" / "Collection" -- the noun in node titles. */
	static FText GetScopeNoun(EScope Scope);

	static const FName CollectionPinName;
	static const FName EntryIndexPinName;
	static const FName CategoryPinName;
	static const FName PropertyNamePinName;
	static const FName OutValuePinName;
	static const FName NewValuePinName;
	static const FName ReadbackPinName;
	static const FName SuccessPinName;

private:
	enum class EFlavor : uint8
	{
		Wildcard,
		Object,
		Class
	};

	/** Backing UPCGExCollectionEntryBlueprintLibrary function for (scope, direction, flavor). */
	static FName GetLibraryFunctionName(EScope Scope, bool bSet, EFlavor Flavor);

	/** Spawn the intermediate call for (scope, direction, flavor) with pins allocated. */
	UK2Node_CallFunction* SpawnLibraryCall(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, bool bSet, EFlavor Flavor);
};
