// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "PCGExPropertyRowHost.generated.h"

struct FPCGExPropertyOverrides;
struct FPCGExPropertySchemaCollection;

UINTERFACE()
class PCGEXPROPERTIES_API UPCGExPropertyRowHost : public UInterface
{
	GENERATED_BODY()
};

/**
 * An object that owns override rows against a property schema: a collection (entries, category rows), a Tuple
 * (its values), a property collection component (its import overrides). Editor tooling that walks rows across hosts
 * (the choices align lever) goes through this; the host keeps its row storage private.
 */
class PCGEXPROPERTIES_API IPCGExPropertyRowHost
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	/** The schema the rows are parallel to. */
	virtual const FPCGExPropertySchemaCollection* EDITOR_GetRowSchema() const = 0;

	/** Visit every row container. The visitor mutates rows in place; the caller has Modify()'d the host. */
	virtual void EDITOR_ForEachPropertyRows(TFunctionRef<void(FPCGExPropertyOverrides&)> Visitor) = 0;

	/** Rows were written in place: dirty, refresh views, re-run whatever the host derives from its rows. */
	virtual void EDITOR_NotifyRowsAligned() = 0;
#endif
};
