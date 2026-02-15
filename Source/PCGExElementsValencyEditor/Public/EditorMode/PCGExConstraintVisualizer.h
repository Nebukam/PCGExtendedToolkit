// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

struct FPCGExConnectorConstraint;
class FPrimitiveDrawInterface;

/**
 * Detail level for constraint visualization, driven by selection state.
 */
enum class EPCGExConstraintDetailLevel : uint8
{
	/** Nothing selected — small indicator dot on connector diamond */
	Indicator,

	/** Cage selected — wireframe shapes showing constraint extent */
	Zone,

	/** Connector selected — full zones + labels + interactive handles */
	Detail
};

/**
 * Interface for constraint-specific viewport visualization and interaction.
 * Each concrete constraint type can register a companion visualizer to provide
 * viewport feedback at three progressive detail levels.
 */
class PCGEXELEMENTSVALENCYEDITOR_API IConstraintVisualizer
{
public:
	virtual ~IConstraintVisualizer() = default;

	// --- Progressive Detail Drawing ---

	/** Indicator only (cage NOT selected): small icon/color on connector diamond. */
	virtual void DrawIndicator(
		FPrimitiveDrawInterface* PDI,
		const FTransform& ConnectorWorld,
		const FPCGExConnectorConstraint& Constraint,
		const FLinearColor& Color) const
	{
	}

	/** Zone preview (cage selected, connector NOT): wireframe shape showing constraint extent. */
	virtual void DrawZone(
		FPrimitiveDrawInterface* PDI,
		const FTransform& ConnectorWorld,
		const FPCGExConnectorConstraint& Constraint,
		const FLinearColor& Color) const
	{
	}

	/** Full detail (connector selected): zone + parameter labels + interactive handles. */
	virtual void DrawDetail(
		FPrimitiveDrawInterface* PDI,
		const FTransform& ConnectorWorld,
		const FPCGExConnectorConstraint& Constraint,
		const FLinearColor& Color,
		bool bIsActiveConstraint) const
	{
	}

	// --- Interactive Handles ---

	/** Whether this constraint type supports viewport handle manipulation. */
	virtual bool HasHandles() const { return false; }

	/** Draw interactive handles (hit proxy enqueued by caller). */
	virtual void DrawHandles(
		FPrimitiveDrawInterface* PDI,
		const FTransform& ConnectorWorld,
		const FPCGExConnectorConstraint& Constraint) const
	{
	}

	/** Handle viewport delta (drag). Returns true if handled. */
	virtual bool HandleDelta(
		const FVector& DeltaTranslate,
		const FRotator& DeltaRotate,
		FPCGExConnectorConstraint& Constraint) const
	{
		return false;
	}
};

/**
 * Registry mapping UScriptStruct* to its companion visualizer instance.
 * Singleton-style access via Get().
 */
class PCGEXELEMENTSVALENCYEDITOR_API FConstraintVisualizerRegistry
{
public:
	/** Register a visualizer for a constraint type */
	template <typename TConstraint, typename TVisualizer>
	void Register()
	{
		Visualizers.Add(TConstraint::StaticStruct(), MakeShared<TVisualizer>());
	}

	/** Find the visualizer for a constraint type (nullptr if not registered) */
	IConstraintVisualizer* Find(const UScriptStruct* Type) const
	{
		if (const TSharedPtr<IConstraintVisualizer>* Found = Visualizers.Find(Type))
		{
			return Found->Get();
		}
		return nullptr;
	}

	/** Get the singleton registry */
	static FConstraintVisualizerRegistry& Get()
	{
		static FConstraintVisualizerRegistry Instance;
		return Instance;
	}

private:
	TMap<const UScriptStruct*, TSharedPtr<IConstraintVisualizer>> Visualizers;
};
