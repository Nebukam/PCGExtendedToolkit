// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExProperty.h"

namespace PCGExPropertyOverridesSync
{
	/** What ties a row to the property it stands for, weakest first. */
	enum class ERowTie : uint8
	{
		None,
		Name,     // the name alone: the row carries no HeaderId, or one that identifies nothing
		HeaderId, // the HeaderId, under the property's name or another one (a rename)
	};

	/** A row's HeaderId, outer identity first like GetPropertyName; 0 when it has none or it is compiled out. */
	inline int32 HeaderIdOf(const FPCGExPropertyOverrideEntry& Row)
	{
#if WITH_EDITORONLY_DATA
		if (Row.HeaderId != 0)
		{
			return Row.HeaderId;
		}
		if (const FPCGExProperty* Property = Row.GetProperty())
		{
			return Property->HeaderId;
		}
#endif
		return 0;
	}

	/** A resolved property is identified by its schema's outer identity: the copy inside its value can be stale. */
	inline int32 HeaderIdOf(const FPCGExPropertyResolved& Property)
	{
#if WITH_EDITORONLY_DATA
		return Property.Source->HeaderId;
#else
		return 0;
#endif
	}

	inline FName NameOf(const FPCGExPropertyResolved& Property)
	{
		return Property.Source->Name;
	}

	/** A schema value, as SyncToSchema is handed them, carries its identity itself. */
	inline int32 HeaderIdOf(const FInstancedStruct& Property)
	{
#if WITH_EDITORONLY_DATA
		if (const FPCGExProperty* Typed = Property.GetPtr<FPCGExProperty>())
		{
			return Typed->HeaderId;
		}
#endif
		return 0;
	}

	inline FName NameOf(const FInstancedStruct& Property)
	{
		const FPCGExProperty* Typed = Property.GetPtr<FPCGExProperty>();
		return Typed ? Typed->PropertyName : FName();
	}

	/**
	 * Index in Properties of the property row RowIndex stands for, INDEX_NONE when it stands for none; OutTie says how.
	 * The one rule for "which property is this row": a HeaderId outranks the name only while it links one row to one
	 * property. Duplicated schema rows and duplicated import assets leave several carrying one id, which then
	 * identifies nothing: the name decides. A zero HeaderId and a None name tie nothing.
	 */
	template <typename PropertyT>
	int32 FindPropertyIndex(const TConstArrayView<FPCGExPropertyOverrideEntry> Rows, const int32 RowIndex, const TConstArrayView<PropertyT> Properties, ERowTie* OutTie = nullptr)
	{
		const FName RowName = Rows[RowIndex].GetPropertyName();
		const int32 RowId = HeaderIdOf(Rows[RowIndex]);

		auto Tied = [OutTie](const int32 PropertyIndex, const ERowTie Tie)
		{
			if (OutTie)
			{
				*OutTie = PropertyIndex == INDEX_NONE ? ERowTie::None : Tie;
			}
			return PropertyIndex;
		};

		// Rows sit parallel to the properties unless the schema drifted, so the row's own index is tried first.
		if (!RowName.IsNone() && Properties.IsValidIndex(RowIndex))
		{
			const PropertyT& Parallel = Properties[RowIndex];
			if (NameOf(Parallel) == RowName && (RowId == 0 || RowId == HeaderIdOf(Parallel)))
			{
				return Tied(RowIndex, RowId == 0 ? ERowTie::Name : ERowTie::HeaderId);
			}
		}

		int32 ByName = INDEX_NONE;
		int32 ById = INDEX_NONE;
		int32 NumById = 0;
		for (int32 i = 0; i < Properties.Num(); ++i)
		{
			const bool bSharesName = !RowName.IsNone() && NameOf(Properties[i]) == RowName;
			if (RowId != 0 && RowId == HeaderIdOf(Properties[i]))
			{
				if (bSharesName)
				{
					return Tied(i, ERowTie::HeaderId);
				}
				ById = i;
				++NumById;
			}
			else if (bSharesName && ByName == INDEX_NONE)
			{
				ByName = i;
			}
		}

		// A HeaderId under another name is a rename only while one property and one row carry it.
		if (NumById == 1)
		{
			int32 NumRowsWithId = 0;
			for (const FPCGExPropertyOverrideEntry& Other : Rows)
			{
				NumRowsWithId += HeaderIdOf(Other) == RowId ? 1 : 0;
			}
			if (NumRowsWithId == 1)
			{
				return Tied(ById, ERowTie::HeaderId);
			}
		}
		return Tied(ByName, ERowTie::Name);
	}

	/** FindPropertyIndex against a resolved schema: the property row RowIndex stands for, null when there is none. */
	const FPCGExPropertyResolved* FindProperty(TConstArrayView<FPCGExPropertyOverrideEntry> Rows, int32 RowIndex, TConstArrayView<FPCGExPropertyResolved> Resolved);

	/** Whether Rows sit entry for entry on Resolved: one row per property, in its order and of its type. */
	bool IsParallel(TConstArrayView<FPCGExPropertyOverrideEntry> Rows, TConstArrayView<FPCGExPropertyResolved> Resolved);
}
