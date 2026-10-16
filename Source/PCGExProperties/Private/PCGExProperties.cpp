// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExProperties.h"
#include "PCGExLog.h"
#include "PCGExProperty.h"
#include "PCGExPropertyRowMatching.h"
#include "PCGExPropertySchemaAsset.h"
#include "PCGExPropertyTypes.h"
#include "Data/PCGExDataHelpers.h"
#include "Helpers/PCGExMetaHelpers.h"
#include "Types/PCGExTypes.h"

#if WITH_EDITOR
void FPCGExPropertiesModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle)
{
	// No special editor registration needed for properties module
	IPCGExModuleInterface::RegisterToEditor(InStyle);
}
#endif

PCGEX_IMPLEMENT_MODULE(FPCGExPropertiesModule, PCGExProperties)

#pragma region FPCGExProperty

int32 PCGExProperties::GetPackedFloatWidth(const EPCGMetadataTypes InType)
{
	switch (InType)
	{
	case EPCGMetadataTypes::Boolean:
	case EPCGMetadataTypes::Float:
	case EPCGMetadataTypes::Double:
	case EPCGMetadataTypes::Integer32:
	case EPCGMetadataTypes::Integer64:
		return 1;
	case EPCGMetadataTypes::Vector2:
		return 2;
	case EPCGMetadataTypes::Vector:
	case EPCGMetadataTypes::Rotator:
		return 3;
	case EPCGMetadataTypes::Vector4:
	case EPCGMetadataTypes::Quaternion:
		return 4;
	default:
		return 0;
	}
}

bool PCGExProperties::MakePropertyForMetadataType(const EPCGMetadataTypes InType, const FName InPropertyName, FInstancedStruct& OutProperty)
{
	switch (InType)
	{
#define PCGEX_MAKE_PROPERTY_CASE(_TYPE, _STRUCT) case EPCGMetadataTypes::_TYPE: OutProperty.InitializeAs<_STRUCT>(); break;
	PCGEX_MAKE_PROPERTY_CASE(Float, FPCGExProperty_Float)
	PCGEX_MAKE_PROPERTY_CASE(Double, FPCGExProperty_Double)
	PCGEX_MAKE_PROPERTY_CASE(Integer32, FPCGExProperty_Int32)
	PCGEX_MAKE_PROPERTY_CASE(Integer64, FPCGExProperty_Int64)
	PCGEX_MAKE_PROPERTY_CASE(Vector2, FPCGExProperty_Vector2)
	PCGEX_MAKE_PROPERTY_CASE(Vector, FPCGExProperty_Vector)
	PCGEX_MAKE_PROPERTY_CASE(Vector4, FPCGExProperty_Vector4)
	PCGEX_MAKE_PROPERTY_CASE(Quaternion, FPCGExProperty_Quat)
	PCGEX_MAKE_PROPERTY_CASE(Transform, FPCGExProperty_Transform)
	PCGEX_MAKE_PROPERTY_CASE(String, FPCGExProperty_String)
	PCGEX_MAKE_PROPERTY_CASE(Boolean, FPCGExProperty_Bool)
	PCGEX_MAKE_PROPERTY_CASE(Rotator, FPCGExProperty_Rotator)
	PCGEX_MAKE_PROPERTY_CASE(Name, FPCGExProperty_Name)
	PCGEX_MAKE_PROPERTY_CASE(SoftObjectPath, FPCGExProperty_SoftObjectPath)
	PCGEX_MAKE_PROPERTY_CASE(SoftClassPath, FPCGExProperty_SoftClassPath)
#undef PCGEX_MAKE_PROPERTY_CASE
	default:
		return false;
	}

	OutProperty.GetMutablePtr<FPCGExProperty>()->PropertyName = InPropertyName;
	return true;
}

int32 FPCGExProperty::GetPackedFloatCount() const
{
	return PCGExProperties::GetPackedFloatWidth(GetOutputType());
}

bool FPCGExProperty::PackFloats(TArrayView<float> OutFloats) const
{
	const EPCGMetadataTypes OutputType = GetOutputType();

	// Bound against the width this body writes, not the virtual count -- a subclass that narrows
	// GetPackedFloatCount() would otherwise hand us a short view we'd overrun.
	if (OutFloats.Num() < PCGExProperties::GetPackedFloatWidth(OutputType))
	{
		return false;
	}

	// Component order mirrors PCGInstanceDataPackerBase; diverging misaligns the stock packers.
	switch (OutputType)
	{
	case EPCGMetadataTypes::Boolean:
	case EPCGMetadataTypes::Float:
	case EPCGMetadataTypes::Double:
	case EPCGMetadataTypes::Integer32:
	case EPCGMetadataTypes::Integer64:
	{
		double Value = 0;
		if (!TryGetValue<double>(Value))
		{
			return false;
		}
		OutFloats[0] = static_cast<float>(Value);
		return true;
	}
	case EPCGMetadataTypes::Vector2:
	{
		FVector2D Value = FVector2D::ZeroVector;
		if (!TryGetValue<FVector2D>(Value))
		{
			return false;
		}
		OutFloats[0] = static_cast<float>(Value.X);
		OutFloats[1] = static_cast<float>(Value.Y);
		return true;
	}
	case EPCGMetadataTypes::Vector:
	{
		FVector Value = FVector::ZeroVector;
		if (!TryGetValue<FVector>(Value))
		{
			return false;
		}
		OutFloats[0] = static_cast<float>(Value.X);
		OutFloats[1] = static_cast<float>(Value.Y);
		OutFloats[2] = static_cast<float>(Value.Z);
		return true;
	}
	case EPCGMetadataTypes::Vector4:
	{
		FVector4 Value = FVector4::Zero();
		if (!TryGetValue<FVector4>(Value))
		{
			return false;
		}
		OutFloats[0] = static_cast<float>(Value.X);
		OutFloats[1] = static_cast<float>(Value.Y);
		OutFloats[2] = static_cast<float>(Value.Z);
		OutFloats[3] = static_cast<float>(Value.W);
		return true;
	}
	case EPCGMetadataTypes::Quaternion:
	{
		FQuat Value = FQuat::Identity;
		if (!TryGetValue<FQuat>(Value))
		{
			return false;
		}
		OutFloats[0] = static_cast<float>(Value.X);
		OutFloats[1] = static_cast<float>(Value.Y);
		OutFloats[2] = static_cast<float>(Value.Z);
		OutFloats[3] = static_cast<float>(Value.W);
		return true;
	}
	case EPCGMetadataTypes::Rotator:
	{
		FRotator Value = FRotator::ZeroRotator;
		if (!TryGetValue<FRotator>(Value))
		{
			return false;
		}
		OutFloats[0] = static_cast<float>(Value.Roll);
		OutFloats[1] = static_cast<float>(Value.Pitch);
		OutFloats[2] = static_cast<float>(Value.Yaw);
		return true;
	}
	default:
		return false;
	}
}

bool FPCGExProperty::WriteDataDomainValue(UPCGData* OutData, const FName OutName) const
{
	bool bWritten = false;
	// An unsupported output type falls through the dispatcher's default arm, leaving bWritten false.
	PCGExMetaHelpers::ExecuteWithRightType(GetOutputType(), [&](auto DummyValue)
	{
		using T = decltype(DummyValue);
		T Value{};
		if (!this->TryGetValue<T>(Value))
		{
			return;
		}
		bWritten = PCGExData::Helpers::SetDataValue<T>(OutData, OutName, Value);
	});
	return bWritten;
}

#pragma endregion

#pragma region FPCGExPropertyOverrideEntry

#if WITH_EDITORONLY_DATA
void FPCGExPropertyOverrideEntry::SeedOuterIdentityFromInner()
{
	if (const FPCGExProperty* Prop = Value.GetPtr<FPCGExProperty>())
	{
		PropertyName = Prop->PropertyName;
		HeaderId = Prop->HeaderId;
	}
}

bool FPCGExPropertyOverrideEntry::SetChoicesPatch(const FPCGExPropertyChoicesPatch& Patch)
{
	const FPCGExPropertyChoicesPatch* Stored = ChoicesPatch.GetPtr();
	if (Patch.IsEmpty())
	{
		ChoicesPatch.Reset();
		return Stored != nullptr;
	}
	// Also what makes Patch safe to be the stored patch itself: InitializeAs frees the storage before it copies.
	if (Stored && *Stored == Patch)
	{
		return false;
	}
	ChoicesPatch.InitializeAs(Patch);
	return true;
}
#endif

FName FPCGExPropertyOverrideEntry::GetPropertyName() const
{
#if WITH_EDITORONLY_DATA
	if (!PropertyName.IsNone())
	{
		return PropertyName;
	}
#endif
	if (const FPCGExProperty* Prop = Value.GetPtr<FPCGExProperty>())
	{
		return Prop->PropertyName;
	}
	return NAME_None;
}

const FPCGExProperty* FPCGExPropertyOverrideEntry::GetProperty() const
{
	return Value.GetPtr<FPCGExProperty>();
}

FPCGExProperty* FPCGExPropertyOverrideEntry::GetPropertyMutable()
{
	return Value.GetMutablePtr<FPCGExProperty>();
}

#if WITH_EDITORONLY_DATA
namespace PCGExPropertyChoicesSync
{
	/** How a host's pick stands against the list the host holds. */
	enum class EPick : uint8
	{
		None,     // nothing picked
		Unlisted, // picked a choice the list does not offer: hidden by a patch, removed, or no longer of the host's type
		OnChoice, // the value still equals the picked choice
		Diverged, // the picked choice is offered, the value moved away from it
	};

	/** Whether Host's Value equals Choice's. Choice must be compatible with Host. */
	bool HoldsValueOf(const FConstStructView Host, const FPCGExPropertyChoice& Choice)
	{
		const FProperty* ValueProperty = PCGExProperties::FindValueProperty(Host.GetScriptStruct());
		return ValueProperty && ValueProperty->Identical_InContainer(static_cast<const void*>(Host.GetMemory()), Choice.Value.GetMemory());
	}

	/** Host's pick against Host's own list. Taken BEFORE that list is replaced, settled after (SettlePick). */
	EPick SamplePick(const FConstStructView Host)
	{
		const FPCGExProperty* Property = Host.GetPtr<FPCGExProperty>();
		if (!Property || !Property->ChosenChoiceId.IsValid())
		{
			return EPick::None;
		}
		const int32 Chosen = PCGExProperties::FindChoiceById(Property->Choices, Property->ChosenChoiceId);
		if (Chosen == INDEX_NONE || !PCGExProperties::IsChoiceCompatible(Host, Property->Choices.Items[Chosen]))
		{
			return EPick::Unlisted;
		}
		return HoldsValueOf(Host, Property->Choices.Items[Chosen]) ? EPick::OnChoice : EPick::Diverged;
	}

	/**
	 * Bring Host's pick in line with the list it now holds. A value that stood on its choice follows it (bFollow);
	 * any other value is kept, and keeps its pick only when it equals the choice. A pick the list does not offer
	 * stays dormant: it binds again if the choice returns holding this very value. True if Host changed.
	 */
	bool SettlePick(const FStructView Host, const EPick Before, const bool bFollow)
	{
		FPCGExProperty* Property = Host.GetPtr<FPCGExProperty>();
		if (!Property || Before == EPick::None)
		{
			return false;
		}
		if (Before == EPick::Diverged)
		{
			Property->ChosenChoiceId.Invalidate();
			return true;
		}

		const int32 Chosen = PCGExProperties::FindChoiceById(Property->Choices, Property->ChosenChoiceId);
		if (Chosen == INDEX_NONE || !PCGExProperties::IsChoiceCompatible(Host, Property->Choices.Items[Chosen]))
		{
			return false;
		}

		// Reference into Host's own Choices: copying into the sibling Value field leaves it put.
		const FPCGExPropertyChoice& Choice = Property->Choices.Items[Chosen];
		if (HoldsValueOf(Host, Choice))
		{
			return false;
		}
		if (Before == EPick::OnChoice && bFollow)
		{
			PCGExProperties::FindValueProperty(Host.GetScriptStruct())->CopyCompleteValue_InContainer(Host.GetMemory(), Choice.Value.GetMemory());
		}
		else
		{
			Property->ChosenChoiceId.Invalidate();
		}
		return true;
	}

	/** Mirror Incoming onto a row. Compared first: most syncs change nothing, and most properties have no choices. */
	bool Mirror(FPCGExPropertyChoices& Mine, const FPCGExPropertyChoices& Incoming)
	{
		if (Mine.Items.IsEmpty() && Incoming.Items.IsEmpty())
		{
			if (Mine.bLocked == Incoming.bLocked)
			{
				return false;
			}
			Mine.bLocked = Incoming.bLocked;
			return true;
		}
		if (Mine == Incoming)
		{
			return false;
		}
		Mine = Incoming;
		return true;
	}
}
#endif

namespace PCGExPropertyRowSync
{
	/**
	 * Bring Row, a value held against Schema, in line with it: the type's own schema-owned fields, then (editor) the
	 * choice list, EffectiveChoices standing in for Schema's own when given. Nothing unless both are one property type.
	 */
	bool MirrorSchemaOwnedFields(const FStructView Row, const FConstStructView Schema, const FPCGExPropertyChoices* EffectiveChoices, const bool bFollowPick)
	{
		// The hook casts its argument to its own type unchecked: same script struct, or nothing.
		FPCGExProperty* Mine = Row.GetPtr<FPCGExProperty>();
		const FPCGExProperty* Theirs = Schema.GetPtr<FPCGExProperty>();
		if (!Mine || !Theirs || Row.GetScriptStruct() != Schema.GetScriptStruct())
		{
			return false;
		}

#if WITH_EDITORONLY_DATA
		// Before either sync below can move the value or the list it is judged against.
		const PCGExPropertyChoicesSync::EPick Before = PCGExPropertyChoicesSync::SamplePick(Row);
#endif

		bool bChanged = Mine->SyncStructuralFromSchema(*Theirs);

#if WITH_EDITORONLY_DATA
		// Not short-circuited: each step must run even when an earlier one already changed the row.
		bChanged |= PCGExPropertyChoicesSync::Mirror(Mine->Choices, EffectiveChoices ? *EffectiveChoices : Theirs->Choices);
		bChanged |= PCGExPropertyChoicesSync::SettlePick(Row, Before, bFollowPick);
#endif
		return bChanged;
	}
}

bool FPCGExPropertyOverrideEntry::SyncStructuralFields(const FInstancedStruct& SchemaValue, const FPCGExPropertyChoices* EffectiveChoices, const bool bFollowPick)
{
	return PCGExPropertyRowSync::MirrorSchemaOwnedFields(Value, SchemaValue, EffectiveChoices, bFollowPick);
}

#pragma endregion

#pragma region Choices

namespace PCGExPropertyChoicesSync
{
	bool NormalizeIds(TArray<FPCGExPropertyChoice>& Items)
	{
		// The last holder keeps a shared Id: the details panel's Duplicate inserts the copy before its source.
		bool bChanged = false;
		for (int32 i = Items.Num() - 1; i >= 0; --i)
		{
			FGuid& Id = Items[i].Id;
			bool bMint = !Id.IsValid();
			for (int32 j = i + 1; !bMint && j < Items.Num(); ++j)
			{
				bMint = Items[j].Id == Id;
			}
			if (bMint)
			{
				Id = FGuid::NewGuid();
				bChanged = true;
			}
		}
		return bChanged;
	}
}

bool FPCGExPropertyChoices::NormalizeIds()
{
	return PCGExPropertyChoicesSync::NormalizeIds(Items);
}

bool FPCGExPropertyChoicesPatch::NormalizeIds()
{
	return PCGExPropertyChoicesSync::NormalizeIds(Items);
}

void FPCGExPropertyChoicesPatch::Apply(FPCGExPropertyChoices& InOut) const
{
	switch (Mode)
	{
	case EPCGExChoicesPatchMode::Inherit:
		break;
	case EPCGExChoicesPatchMode::Modify:
		if (!RemovedIds.IsEmpty())
		{
			InOut.Items.RemoveAll([this](const FPCGExPropertyChoice& Item)
			{
				return Item.Id.IsValid() && RemovedIds.Contains(Item.Id);
			});
		}
		for (const FPCGExPropertyChoice& Added : Items)
		{
			FPCGExPropertyChoice* Existing = Added.Id.IsValid()
				? InOut.Items.FindByPredicate([&Added](const FPCGExPropertyChoice& Item) { return Item.Id == Added.Id; })
				: nullptr;
			if (Existing)
			{
				*Existing = Added;
			}
			else
			{
				InOut.Items.Add(Added);
			}
		}
		break;
	case EPCGExChoicesPatchMode::Replace:
		InOut.Items = Items;
		break;
	default:
		checkNoEntry();
		break;
	}

	switch (Lock)
	{
	case EPCGExChoicesLock::Inherit:
		break;
	case EPCGExChoicesLock::Locked:
		InOut.bLocked = true;
		break;
	case EPCGExChoicesLock::Unlocked:
		InOut.bLocked = false;
		break;
	default:
		checkNoEntry();
		break;
	}
}

namespace PCGExProperties
{
	const FProperty* FindValueProperty(const UScriptStruct* InStruct)
	{
		static const FName ValueFieldName(TEXT("Value"));
		return InStruct ? InStruct->FindPropertyByName(ValueFieldName) : nullptr;
	}

	bool ValuesIdentical(const FConstStructView A, const FConstStructView B)
	{
		if (A.GetScriptStruct() != B.GetScriptStruct())
		{
			return false;
		}
		// Two unset properties hold the same nothing.
		if (!A.IsValid() || !B.IsValid())
		{
			return A.IsValid() == B.IsValid();
		}
		// A type with no Value field has nothing narrower to compare than itself.
		const FProperty* ValueProperty = FindValueProperty(A.GetScriptStruct());
		return ValueProperty
			? ValueProperty->Identical_InContainer(static_cast<const void*>(A.GetMemory()), B.GetMemory())
			: A.GetScriptStruct()->CompareScriptStruct(A.GetMemory(), B.GetMemory(), PPF_None);
	}

	bool CopyValue(const FStructView Dest, const FConstStructView Src)
	{
		FPCGExProperty* DestProperty = Dest.GetPtr<FPCGExProperty>();
		const FPCGExProperty* SrcProperty = Src.GetPtr<FPCGExProperty>();
		const FProperty* ValueProperty = DestProperty && SrcProperty && Dest.GetScriptStruct() == Src.GetScriptStruct()
			? FindValueProperty(Dest.GetScriptStruct())
			: nullptr;
		if (!ValueProperty)
		{
			return false;
		}
		ValueProperty->CopyCompleteValue_InContainer(Dest.GetMemory(), Src.GetMemory());
#if WITH_EDITORONLY_DATA
		DestProperty->ChosenChoiceId = SrcProperty->ChosenChoiceId;
#endif
		return true;
	}
}

FInstancedStruct FPCGExPropertyResolved::MakeEffectiveCopy() const
{
	FInstancedStruct Copy = GetEffectiveProperty();
#if WITH_EDITORONLY_DATA
	// A declaration nothing patches already carries the effective list; an override row's is a mirror that may be stale.
	if (OverrideValue || !ChoicePatches.IsEmpty())
	{
		if (FPCGExProperty* Property = Copy.GetMutablePtr<FPCGExProperty>())
		{
			GetEffectiveChoices(Property->Choices);
		}
	}
#endif
	return Copy;
}

#if WITH_EDITORONLY_DATA
void FPCGExPropertyResolved::GetEffectiveChoices(FPCGExPropertyChoices& Out) const
{
	const FPCGExProperty* Declared = Source ? Source->GetProperty() : nullptr;
	Out = Declared ? Declared->Choices : FPCGExPropertyChoices();
	for (int32 i = ChoicePatches.Num() - 1; i >= 0; --i)
	{
		ChoicePatches[i]->Apply(Out);
	}
	if (Source)
	{
		// Patch-added carriers were authored against an importer's copy: every carrier follows the declaration.
		PCGExProperties::SanitizeChoices(Out.Items);
		PCGExProperties::SyncChoiceCarriers(Out.Items, FConstStructView(Source->Property));
	}
}

const FPCGExPropertyChoices& FPCGExResolvedChoicesCache::Get(const TConstArrayView<FPCGExPropertyResolved> Resolved, const int32 Index)
{
	if (Lists.Num() != Resolved.Num())
	{
		Lists.Reset();
		Lists.SetNum(Resolved.Num());
		Built.Init(false, Resolved.Num());
	}
	if (!Built[Index])
	{
		Resolved[Index].GetEffectiveChoices(Lists[Index]);
		Built[Index] = true;
	}
	return Lists[Index];
}

namespace PCGExPropertyChoicesSync
{
	/** A row overwritten in place (a paste) keeps its own identity: the incoming Id belongs to the row it came from. */
	void KeepOverwrittenIds(const TConstArrayView<FPCGExPropertyChoice> Previous, TArray<FPCGExPropertyChoice>& Items)
	{
		if (Previous.Num() != Items.Num())
		{
			return;
		}
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			const FGuid& PreviousId = Previous[i].Id;
			if (!PreviousId.IsValid() || Items[i].Id == PreviousId)
			{
				continue;
			}
			// Still held by another row: this one was moved, not overwritten.
			if (!Items.ContainsByPredicate([&PreviousId](const FPCGExPropertyChoice& Item) { return Item.Id == PreviousId; }))
			{
				Items[i].Id = PreviousId;
			}
		}
	}

	/** What every committed list goes through: identities settled, then each carrier made a plain value under Host's rules. */
	void PrepareCommittedItems(const TConstArrayView<FPCGExPropertyChoice> Previous, TArray<FPCGExPropertyChoice>& Items, const FConstStructView Host)
	{
		KeepOverwrittenIds(Previous, Items);
		NormalizeIds(Items);
		for (FPCGExPropertyChoice& Choice : Items)
		{
			// The details panel's Insert adds an element with no carrier, which no row editor can fill.
			if (!Choice.Value.IsValid())
			{
				Choice.Value = PCGExProperties::MakeChoiceCarrier(Host);
			}
		}
		PCGExProperties::SanitizeChoices(Items);
		PCGExProperties::SyncChoiceCarriers(Items, Host);
	}

	/** Whether a value of From's type can be carried into To's through From's attribute projection. */
	bool CanConvertValue(const FPCGExProperty& From, const FPCGExProperty& To)
	{
		if (!From.SupportsValueConversion() || !To.SupportsValueConversion())
		{
			return false;
		}
		// TryWriteValue on a type outside the legacy set reports success and writes nothing.
		const EPCGMetadataTypes FromType = From.GetOutputType();
		const EPCGMetadataTypes ToType = To.GetOutputType();
		if (!PCGExMetaHelpers::IsLegacyScalarType(FromType) || !PCGExMetaHelpers::IsLegacyScalarType(ToType))
		{
			return false;
		}
		// Only a path makes a path: FSoftObjectPath silently turns any text it cannot parse into a null path.
		const bool bFromPath = FromType == EPCGMetadataTypes::SoftObjectPath || FromType == EPCGMetadataTypes::SoftClassPath;
		const bool bToPath = ToType == EPCGMetadataTypes::SoftObjectPath || ToType == EPCGMetadataTypes::SoftClassPath;
		return bFromPath || !bToPath;
	}

	/**
	 * Items re-made for NewHost's type after a retype: ids and labels kept, a value converted where CanConvertValue
	 * allows, else the new type's default. An item already of NewHost's type is kept as is; one with no carrier is dropped.
	 */
	TArray<FPCGExPropertyChoice> ConvertItems(const TConstArrayView<FPCGExPropertyChoice> OldItems, const FConstStructView NewHost)
	{
		TArray<FPCGExPropertyChoice> Converted;
		const FPCGExProperty* NewProperty = NewHost.GetPtr<FPCGExProperty>();
		if (!NewProperty)
		{
			return Converted;
		}

		Converted.Reserve(OldItems.Num());
		for (const FPCGExPropertyChoice& OldChoice : OldItems)
		{
			const FPCGExProperty* OldCarrier = OldChoice.Value.GetPtr<FPCGExProperty>();
			if (!OldCarrier)
			{
				continue;
			}
			if (OldChoice.Value.GetScriptStruct() == NewHost.GetScriptStruct())
			{
				Converted.Add(OldChoice);
				continue;
			}

			FPCGExPropertyChoice& NewChoice = Converted.AddDefaulted_GetRef();
			NewChoice.Id = OldChoice.Id;
			NewChoice.Label = OldChoice.Label;
			NewChoice.Value.InitializeAs(NewHost.GetScriptStruct());
			FPCGExProperty& NewCarrier = NewChoice.Value.GetMutable<FPCGExProperty>();
			NewCarrier.SyncStructuralFromSchema(*NewProperty);

			if (CanConvertValue(*OldCarrier, NewCarrier))
			{
				const EPCGMetadataTypes Pivot = OldCarrier->GetOutputType();
				PCGExTypes::FScopedTypedValue PivotValue(Pivot);
				if (OldCarrier->TryWriteValue(Pivot, PivotValue.GetRaw()))
				{
					NewCarrier.TryReadValue(Pivot, PivotValue.GetRaw());
				}
			}
		}
		return Converted;
	}

	/** Whether any item's carrier is of another type than Host. */
	bool HasForeignCarrier(const TConstArrayView<FPCGExPropertyChoice> Items, const FConstStructView Host)
	{
		return Items.ContainsByPredicate([&Host](const FPCGExPropertyChoice& Item)
		{
			return Item.Value.IsValid() && Item.Value.GetScriptStruct() != Host.GetScriptStruct();
		});
	}
}

namespace PCGExProperties
{
	bool IsChoiceCompatible(const FConstStructView Host, const FPCGExPropertyChoice& Choice)
	{
		const FPCGExProperty* HostProperty = Host.GetPtr<FPCGExProperty>();
		const FPCGExProperty* Carrier = Choice.Value.GetPtr<FPCGExProperty>();
		return HostProperty && Carrier
			&& Host.GetScriptStruct() == Choice.Value.GetScriptStruct()
			&& HostProperty->IsChoiceCompatible(*Carrier);
	}

	int32 FindChoiceById(const FPCGExPropertyChoices& Choices, const FGuid& Id)
	{
		if (!Id.IsValid())
		{
			return INDEX_NONE;
		}
		return Choices.Items.IndexOfByPredicate([&Id](const FPCGExPropertyChoice& Choice) { return Choice.Id == Id; });
	}

	int32 FindMatchingChoice(const FConstStructView Host, const FPCGExPropertyChoices& Choices)
	{
		const FProperty* ValueProperty = Host.IsValid() ? FindValueProperty(Host.GetScriptStruct()) : nullptr;
		if (!ValueProperty)
		{
			return INDEX_NONE;
		}
		for (int32 i = 0; i < Choices.Items.Num(); ++i)
		{
			const FPCGExPropertyChoice& Choice = Choices.Items[i];
			if (IsChoiceCompatible(Host, Choice)
				&& ValueProperty->Identical_InContainer(static_cast<const void*>(Host.GetMemory()), Choice.Value.GetMemory()))
			{
				return i;
			}
		}
		return INDEX_NONE;
	}

	int32 FindBoundChoice(const FConstStructView Host, const FPCGExPropertyChoices& Choices)
	{
		const FPCGExProperty* HostProperty = Host.GetPtr<FPCGExProperty>();
		const int32 Chosen = HostProperty ? FindChoiceById(Choices, HostProperty->ChosenChoiceId) : INDEX_NONE;
		const bool bBound = Chosen != INDEX_NONE
			&& IsChoiceCompatible(Host, Choices.Items[Chosen])
			&& PCGExPropertyChoicesSync::HoldsValueOf(Host, Choices.Items[Chosen]);
		return bBound ? Chosen : INDEX_NONE;
	}

	bool IsBoundToChoice(const FConstStructView Host)
	{
		return PCGExPropertyChoicesSync::SamplePick(Host) == PCGExPropertyChoicesSync::EPick::OnChoice;
	}

	bool AdoptValue(const FStructView Dest, const FConstStructView Src)
	{
		// Src's own list is the one its value was authored against, which is what tells a pick from a leftover.
		const PCGExPropertyChoicesSync::EPick SrcPick = PCGExPropertyChoicesSync::SamplePick(Src);
		if (!CopyValue(Dest, Src))
		{
			return false;
		}
		PCGExPropertyChoicesSync::SettlePick(Dest, SrcPick, true);
		return true;
	}

	int32 ResolveSelectedChoice(const FConstStructView Host, const FPCGExPropertyChoices& Choices)
	{
		const int32 Bound = FindBoundChoice(Host, Choices);
		return Bound != INDEX_NONE ? Bound : FindMatchingChoice(Host, Choices);
	}

	bool ApplyChoice(const FStructView Host, const FPCGExPropertyChoice& Choice)
	{
		const FProperty* ValueProperty = Host.IsValid() ? FindValueProperty(Host.GetScriptStruct()) : nullptr;
		if (!ValueProperty || !IsChoiceCompatible(Host, Choice))
		{
			return false;
		}
		ValueProperty->CopyCompleteValue_InContainer(Host.GetMemory(), Choice.Value.GetMemory());
		Host.GetPtr<FPCGExProperty>()->ChosenChoiceId = Choice.Id;
		return true;
	}

	bool UnbindDivergedChoice(const FStructView Host)
	{
		const PCGExPropertyChoicesSync::EPick Pick = PCGExPropertyChoicesSync::SamplePick(Host);
		if (Pick == PCGExPropertyChoicesSync::EPick::None || Pick == PCGExPropertyChoicesSync::EPick::OnChoice)
		{
			return false;
		}
		Host.GetPtr<FPCGExProperty>()->ChosenChoiceId.Invalidate();
		return true;
	}

	FInstancedStruct MakeChoiceCarrier(const FConstStructView Host)
	{
		FInstancedStruct Carrier;
		const FPCGExProperty* HostProperty = Host.GetPtr<FPCGExProperty>();
		const FProperty* ValueProperty = HostProperty ? FindValueProperty(Host.GetScriptStruct()) : nullptr;
		if (ValueProperty)
		{
			// Built up from a default instance: copying Host would drag its whole choice list into the carrier first.
			Carrier.InitializeAs(Host.GetScriptStruct());
			ValueProperty->CopyCompleteValue_InContainer(Carrier.GetMutableMemory(), Host.GetMemory());
			Carrier.GetMutable<FPCGExProperty>().SyncStructuralFromSchema(*HostProperty);
		}
		return Carrier;
	}

	FPCGExPropertyChoice MakeChoiceFromValue(const FConstStructView Host, const FString& Label)
	{
		FPCGExPropertyChoice Choice;
		Choice.Id = FGuid::NewGuid();
		Choice.Label = Label;
		Choice.Value = MakeChoiceCarrier(Host);
		return Choice;
	}

	bool SanitizeChoices(const TArrayView<FPCGExPropertyChoice> Items)
	{
		bool bChanged = false;
		for (FPCGExPropertyChoice& Choice : Items)
		{
			FPCGExProperty* Carrier = Choice.Value.GetMutablePtr<FPCGExProperty>();
			if (!Carrier)
			{
				continue;
			}
			const bool bClean = Carrier->PropertyName.IsNone() && Carrier->HeaderId == 0
				&& Carrier->Choices.Items.IsEmpty() && !Carrier->Choices.bLocked && !Carrier->ChosenChoiceId.IsValid();
			if (!bClean)
			{
				Carrier->PropertyName = NAME_None;
				Carrier->HeaderId = 0;
				Carrier->Choices = FPCGExPropertyChoices();
				Carrier->ChosenChoiceId.Invalidate();
				bChanged = true;
			}
		}
		return bChanged;
	}

	bool SyncChoiceCarriers(const TArrayView<FPCGExPropertyChoice> Items, const FConstStructView Host)
	{
		const FPCGExProperty* HostProperty = Host.GetPtr<FPCGExProperty>();
		if (!HostProperty)
		{
			return false;
		}
		bool bChanged = false;
		for (FPCGExPropertyChoice& Choice : Items)
		{
			FPCGExProperty* Carrier = Choice.Value.GetScriptStruct() == Host.GetScriptStruct() ? Choice.Value.GetMutablePtr<FPCGExProperty>() : nullptr;
			if (Carrier)
			{
				bChanged |= Carrier->SyncStructuralFromSchema(*HostProperty);
			}
		}
		return bChanged;
	}

	bool CommitDeclaredChoices(const FStructView Host, FPCGExPropertyChoices Edited)
	{
		FPCGExProperty* Property = Host.GetPtr<FPCGExProperty>();
		if (!Property)
		{
			return false;
		}

		PCGExPropertyChoicesSync::PrepareCommittedItems(Property->Choices.Items, Edited.Items, Host);
		if (Property->Choices == Edited)
		{
			return false;
		}

		const PCGExPropertyChoicesSync::EPick Before = PCGExPropertyChoicesSync::SamplePick(Host);
		Property->Choices = MoveTemp(Edited);
		PCGExPropertyChoicesSync::SettlePick(Host, Before, true);
		// A declaration's own list is the whole truth: a pick it no longer offers is dead, not dormant.
		UnbindDivergedChoice(Host);
		return true;
	}

	bool CommitChoicesPatch(FPCGExPropertyOverrideEntry& Row, FPCGExPropertyChoicesPatch Edited)
	{
		const FPCGExPropertyChoicesPatch* Stored = Row.GetChoicesPatch();
		PCGExPropertyChoicesSync::PrepareCommittedItems(
			Stored ? TConstArrayView<FPCGExPropertyChoice>(Stored->Items) : TConstArrayView<FPCGExPropertyChoice>(), Edited.Items, Row.Value);

		// A stored patch holds only what its Mode reads, so a later Mode change cannot bring stale edits back.
		switch (Edited.Mode)
		{
		case EPCGExChoicesPatchMode::Inherit:
			Edited.Items.Reset();
			Edited.RemovedIds.Reset();
			break;
		case EPCGExChoicesPatchMode::Modify:
			break;
		case EPCGExChoicesPatchMode::Replace:
			Edited.RemovedIds.Reset();
			break;
		default:
			checkNoEntry();
			break;
		}

		return Row.SetChoicesPatch(Edited);
	}

	bool CarryChoicesAcrossRetype(const FConstStructView Old, const FStructView New)
	{
		const FPCGExProperty* OldProperty = Old.GetPtr<FPCGExProperty>();
		FPCGExProperty* NewProperty = New.GetPtr<FPCGExProperty>();
		if (!OldProperty || !NewProperty || Old.GetScriptStruct() == New.GetScriptStruct())
		{
			return false;
		}
		// A property that arrives with a list of its own was pasted whole: nothing of the old one applies to it.
		if (OldProperty->Choices.Items.IsEmpty() || !NewProperty->Choices.Items.IsEmpty())
		{
			return false;
		}

		// Both read before New is written: Old's pick crosses only onto a New still holding its type's default.
		const bool bOldOnChoice = PCGExPropertyChoicesSync::SamplePick(Old) == PCGExPropertyChoicesSync::EPick::OnChoice;
		FInstancedStruct DefaultOfNew;
		DefaultOfNew.InitializeAs(New.GetScriptStruct());
		const bool bNewIsDefault = ValuesIdentical(New, DefaultOfNew);

		FPCGExPropertyChoices Carried;
		Carried.Items = PCGExPropertyChoicesSync::ConvertItems(OldProperty->Choices.Items, New);
		if (Carried.Items.IsEmpty())
		{
			return false;
		}
		// A lock over values that could not cross would trap the property on its type's default.
		Carried.bLocked = OldProperty->Choices.bLocked && PCGExPropertyChoicesSync::CanConvertValue(*OldProperty, *NewProperty);
		NewProperty->Choices = MoveTemp(Carried);

		if (bOldOnChoice && bNewIsDefault)
		{
			const int32 Chosen = FindChoiceById(NewProperty->Choices, OldProperty->ChosenChoiceId);
			if (Chosen != INDEX_NONE)
			{
				ApplyChoice(New, NewProperty->Choices.Items[Chosen]);
			}
		}
		return true;
	}

	FText GetValuePreviewText(const FConstStructView Host)
	{
		const FPCGExProperty* Property = Host.GetPtr<FPCGExProperty>();
		if (!Property)
		{
			return FText::GetEmpty();
		}

		FText Typed;
		if (Property->GetValuePreviewText(Typed))
		{
			return Typed;
		}

		const FProperty* ValueProperty = FindValueProperty(Host.GetScriptStruct());
		if (!ValueProperty)
		{
			return FText::GetEmpty();
		}

		FString Exported;
		ValueProperty->ExportTextItem_Direct(Exported, ValueProperty->ContainerPtrToValuePtr<void>(Host.GetMemory()), nullptr, nullptr, PPF_None);
		return FText::FromString(Exported);
	}
}
#endif

#pragma endregion

#pragma region FPCGExPropertySchema

FPCGExPropertySchema::FPCGExPropertySchema()
{
#if WITH_EDITOR
	Property.InitializeAs<FPCGExProperty_Float>();
#endif
}

void FPCGExPropertySchema::SyncPropertyName()
{
	if (FPCGExProperty* Prop = GetPropertyMutable())
	{
		Prop->PropertyName = Name;
#if WITH_EDITOR
		Prop->HeaderId = HeaderId;
#endif
	}
}

const FPCGExProperty* FPCGExPropertySchema::GetProperty() const
{
	return Property.GetPtr<FPCGExProperty>();
}

FPCGExProperty* FPCGExPropertySchema::GetPropertyMutable()
{
	return Property.GetMutablePtr<FPCGExProperty>();
}

#if WITH_EDITORONLY_DATA
bool FPCGExPropertySchema::ConsumeRetype()
{
	if (!RetypedFrom.IsValid())
	{
		return false;
	}
	const bool bCarried = PCGExProperties::CarryChoicesAcrossRetype(RetypedFrom, Property);
	RetypedFrom.Reset();
	return bCarried;
}
#endif

#pragma endregion

#pragma region FPCGExPropertySchemaCollection

namespace PCGExPropertySchemaResolve
{
	using FImportPath = TArray<const UPCGExPropertySchemaAsset*, TInlineAllocator<8>>;
	using FLayerChain = TArray<const FPCGExPropertyOverrides*, TInlineAllocator<8>>;
	// Import trees are a handful of assets deep: a linear scan beats a set, and stays off the heap on per-row lookups.
	using FVisitedAssets = TArray<const UPCGExPropertySchemaAsset*, TInlineAllocator<16>>;

	/**
	 * Chain plus Asset's own ImportOverrides: what the locals of the assets Asset imports read. An asset's layer joins
	 * after every importer's (the outermost consumer wins, then each level down) and never applies to its own locals.
	 */
	FLayerChain ExtendChain(const TConstArrayView<const FPCGExPropertyOverrides*> Chain, const UPCGExPropertySchemaAsset* Asset)
	{
		FLayerChain Extended;
		Extended.Reserve(Chain.Num() + 1);
		Extended.Append(Chain.GetData(), Chain.Num());
		Extended.Add(&Asset->Collection.ImportOverrides);
		return Extended;
	}

	/**
	 * What Chain says about an imported entry, one scan per layer: returns the enabled value override nearest the root,
	 * and hands PatchSink every choices patch (editor data), which applies whether or not its row's value override is on.
	 */
	const FInstancedStruct* ReadLayers(const TConstArrayView<const FPCGExPropertyOverrides*> Chain, const FPCGExPropertySchema& Schema, FPCGExPropertyResolved* PatchSink = nullptr)
	{
#if WITH_EDITORONLY_DATA
		const bool bWantsPatches = PatchSink != nullptr;
#else
		const bool bWantsPatches = false;
#endif

		const FInstancedStruct* Override = nullptr;
		for (const FPCGExPropertyOverrides* Layer : Chain)
		{
			if (!Layer)
			{
				continue;
			}

			bool bValueRead = Override != nullptr;
			bool bPatchRead = !bWantsPatches;
			for (const FPCGExPropertyOverrideEntry& Row : Layer->Overrides)
			{
				if (Row.GetPropertyName() != Schema.Name)
				{
					continue;
				}
				if (!bValueRead && Row.bEnabled)
				{
					bValueRead = true;
					// A row still holding another type than its declaration is stale, not an override: readers cast unchecked.
					if (Row.Value.GetScriptStruct() == Schema.Property.GetScriptStruct())
					{
						Override = &Row.Value;
					}
				}
#if WITH_EDITORONLY_DATA
				if (!bPatchRead)
				{
					bPatchRead = true;
					const FPCGExPropertyChoicesPatch* Patch = Row.GetChoicesPatch();
					if (Patch && !Patch->IsIdentity())
					{
						PatchSink->ChoicePatches.Add(Patch);
					}
				}
#endif
				if (bValueRead && bPatchRead)
				{
					break;
				}
			}

			if (Override && !bWantsPatches)
			{
				return Override;
			}
		}
		return Override;
	}

	// Depth-first walk: locals first, then ImportedSchemas in array order.
	// Seen / Visited accumulate across the whole walk: first-wins dedup by Name, one resolution per asset.
	// Path is the active recursion stack: a revisit on Path is a cycle (warned; IsDataValid flags it too),
	// a revisit off Path is a diamond, which is supported authoring and skipped at Verbose.
	// LocalsChain is what Collection's own locals read, and only an imported asset's locals read anything:
	// the root's are edited in place.
	void Walk(
		const FPCGExPropertySchemaCollection& Collection,
		UPCGExPropertySchemaAsset* OwningAsset,
		TConstArrayView<const FPCGExPropertyOverrides*> LocalsChain,
		TArray<FPCGExPropertyResolved>& Out,
		TSet<FName>& Seen,
		TSet<const UPCGExPropertySchemaAsset*>& Visited,
		FImportPath& Path)
	{
		for (int32 i = 0; i < Collection.Schemas.Num(); ++i)
		{
			const FPCGExPropertySchema& Schema = Collection.Schemas[i];
			if (!Schema.IsValid())
			{
				continue;
			}

			bool bAlreadySeen = false;
			Seen.Add(Schema.Name, &bAlreadySeen);
			if (bAlreadySeen)
			{
				continue;
			}

			FPCGExPropertyResolved& Resolved = Out.Emplace_GetRef(&Schema, OwningAsset, i);
			if (OwningAsset)
			{
				Resolved.OverrideValue = ReadLayers(LocalsChain, Schema, &Resolved);
			}
		}

		if (Collection.ImportedSchemas.IsEmpty())
		{
			return;
		}

		// The root's own layer is already in the chain its caller built (or left out of it on purpose).
		FLayerChain Extended;
		TConstArrayView<const FPCGExPropertyOverrides*> ImportsChain = LocalsChain;
		if (OwningAsset)
		{
			Extended = ExtendChain(LocalsChain, OwningAsset);
			ImportsChain = MakeArrayView(Extended);
		}

		for (const TObjectPtr<UPCGExPropertySchemaAsset>& AssetPtr : Collection.ImportedSchemas)
		{
			UPCGExPropertySchemaAsset* Asset = AssetPtr.Get();
			if (!Asset)
			{
				continue;
			}

			bool bAlreadyVisited = false;
			Visited.Add(Asset, &bAlreadyVisited);
			if (bAlreadyVisited)
			{
				if (Path.Contains(Asset))
				{
					UE_LOG(LogPCGEx, Warning,
					       TEXT("PCGExPropertySchemaCollection: cyclic schema asset import skipped: %s (imports itself through its own import chain)"),
					       *Asset->GetPathName());
				}
				else
				{
					UE_LOG(LogPCGEx, Verbose,
					       TEXT("PCGExPropertySchemaCollection: schema asset %s already resolved earlier in this walk (redundant import), skipped."),
					       *Asset->GetPathName());
				}
				continue;
			}

			Path.Push(Asset);
			Walk(Asset->Collection, Asset, ImportsChain, Out, Seen, Visited, Path);
			Path.Pop(EAllowShrinking::No);
		}
	}

	// GetPropertyByName's imports leg: the first imported declaration of PropertyName in Walk order, read through the
	// layers Walk would hand it. ImportsChain is what the locals of Collection's imports read; Visited stops cycles.
	const FInstancedStruct* FindEffectiveInImports(
		const FPCGExPropertySchemaCollection& Collection,
		FName PropertyName,
		TConstArrayView<const FPCGExPropertyOverrides*> ImportsChain,
		FVisitedAssets& Visited)
	{
		for (const TObjectPtr<UPCGExPropertySchemaAsset>& AssetPtr : Collection.ImportedSchemas)
		{
			const UPCGExPropertySchemaAsset* Asset = AssetPtr.Get();
			if (!Asset || Visited.Contains(Asset))
			{
				continue;
			}
			Visited.Add(Asset);

			for (const FPCGExPropertySchema& Schema : Asset->Collection.Schemas)
			{
				if (Schema.Name == PropertyName)
				{
					const FInstancedStruct* Override = ReadLayers(ImportsChain, Schema);
					return Override ? Override : &Schema.Property;
				}
			}

			if (Asset->Collection.ImportedSchemas.IsEmpty())
			{
				continue;
			}

			const FLayerChain Extended = ExtendChain(ImportsChain, Asset);
			if (const FInstancedStruct* Found = FindEffectiveInImports(Asset->Collection, PropertyName, MakeArrayView(Extended), Visited))
			{
				return Found;
			}
		}

		return nullptr;
	}

	// FindByName equivalent of Walk -- early-exits on the first matching name without allocating
	// the full resolved list. Visited prevents infinite recursion through cyclic imports.
	const FPCGExPropertySchema* Find(
		const FPCGExPropertySchemaCollection& Collection,
		FName PropertyName,
		FVisitedAssets& Visited)
	{
		for (const FPCGExPropertySchema& Schema : Collection.Schemas)
		{
			if (Schema.Name == PropertyName)
			{
				return &Schema;
			}
		}

		for (const TObjectPtr<UPCGExPropertySchemaAsset>& AssetPtr : Collection.ImportedSchemas)
		{
			const UPCGExPropertySchemaAsset* Asset = AssetPtr.Get();
			if (!Asset || Visited.Contains(Asset))
			{
				continue;
			}
			Visited.Add(Asset);

			if (const FPCGExPropertySchema* Found = Find(Asset->Collection, PropertyName, Visited))
			{
				return Found;
			}
		}

		return nullptr;
	}

	/** Whether an import slot is empty anywhere in Collection's tree: an asset that failed to load leaves one. */
	bool HasMissingImport(const FPCGExPropertySchemaCollection& Collection, FVisitedAssets& Visited)
	{
		for (const TObjectPtr<UPCGExPropertySchemaAsset>& AssetPtr : Collection.ImportedSchemas)
		{
			const UPCGExPropertySchemaAsset* Asset = AssetPtr.Get();
			if (!Asset)
			{
				return true;
			}
			if (Visited.Contains(Asset))
			{
				continue;
			}
			Visited.Add(Asset);
			if (HasMissingImport(Asset->Collection, Visited))
			{
				return true;
			}
		}
		return false;
	}
}

void FPCGExPropertySchemaCollection::Resolve(TArray<FPCGExPropertyResolved>& Out, TConstArrayView<const FPCGExPropertyOverrides*> FallbackChain, bool bIncludeOwnOverrides) const
{
	Out.Reset();

	if (ImportedSchemas.IsEmpty())
	{
		// Same first-seen-wins contract as the imports path below -- duplicate names must resolve
		// identically either way. ValidateUniqueNames is the loud, caller-facing check.
		TSet<FName> LocalSeen;
		Out.Reserve(Schemas.Num());
		for (int32 i = 0; i < Schemas.Num(); ++i)
		{
			const FPCGExPropertySchema& Schema = Schemas[i];
			if (!Schema.IsValid())
			{
				continue;
			}
			bool bAlreadySeen = false;
			LocalSeen.Add(Schema.Name, &bAlreadySeen);
			if (!bAlreadySeen)
			{
				Out.Emplace(&Schema, nullptr, i);
			}
		}
		return;
	}

	// This collection's own ImportOverrides leads the chain when bIncludeOwnOverrides is true;
	// fallback layers come after. When false, only the fallback layers participate -- callers use
	// this to extract the "no instance overrides" view (asset/CDO defaults only).
	TArray<const FPCGExPropertyOverrides*, TInlineAllocator<4>> Chain;
	Chain.Reserve((bIncludeOwnOverrides ? 1 : 0) + FallbackChain.Num());
	if (bIncludeOwnOverrides)
	{
		Chain.Add(&ImportOverrides);
	}
	for (const FPCGExPropertyOverrides* Layer : FallbackChain)
	{
		Chain.Add(Layer);
	}

	TSet<FName> Seen;
	TSet<const UPCGExPropertySchemaAsset*> Visited;
	PCGExPropertySchemaResolve::FImportPath Path;
	PCGExPropertySchemaResolve::Walk(*this, nullptr, MakeArrayView(Chain), Out, Seen, Visited, Path);
}

const FPCGExPropertySchema* FPCGExPropertySchemaCollection::FindByName(FName PropertyName) const
{
	if (PropertyName.IsNone())
	{
		return nullptr;
	}

	if (ImportedSchemas.IsEmpty())
	{
		for (const FPCGExPropertySchema& Schema : Schemas)
		{
			if (Schema.Name == PropertyName)
			{
				return &Schema;
			}
		}
		return nullptr;
	}

	PCGExPropertySchemaResolve::FVisitedAssets Visited;
	return PCGExPropertySchemaResolve::Find(*this, PropertyName, Visited);
}

const FInstancedStruct* FPCGExPropertySchemaCollection::GetPropertyByName(FName PropertyName) const
{
	if (PropertyName.IsNone())
	{
		return nullptr;
	}

	for (const FPCGExPropertySchema& Schema : Schemas)
	{
		if (Schema.Name == PropertyName)
		{
			return &Schema.Property;
		}
	}

	if (ImportedSchemas.IsEmpty())
	{
		return nullptr;
	}

	// Same chain as Resolve: an override only surfaces for a name the import tree still declares.
	const FPCGExPropertyOverrides* Chain[] = {&ImportOverrides};
	PCGExPropertySchemaResolve::FVisitedAssets Visited;
	return PCGExPropertySchemaResolve::FindEffectiveInImports(*this, PropertyName, MakeArrayView(Chain), Visited);
}

FPCGExPropertySchema* FPCGExPropertySchemaCollection::FindByNameMutable(FName PropertyName)
{
	if (PropertyName.IsNone())
	{
		return nullptr;
	}
	for (FPCGExPropertySchema& Schema : Schemas)
	{
		if (Schema.Name == PropertyName)
		{
			return &Schema;
		}
	}
	return nullptr;
}

TArray<FInstancedStruct> FPCGExPropertySchemaCollection::BuildSchema(TConstArrayView<const FPCGExPropertyOverrides*> FallbackChain, bool bIncludeOwnOverrides) const
{
	TArray<FPCGExPropertyResolved> Resolved;
	Resolve(Resolved, FallbackChain, bIncludeOwnOverrides);

	TArray<FInstancedStruct> Result;
	Result.Reserve(Resolved.Num());
	for (const FPCGExPropertyResolved& Entry : Resolved)
	{
		Result.Add(Entry.MakeEffectiveCopy());
	}
	return Result;
}

bool FPCGExPropertySchemaCollection::ValidateUniqueNames(TArray<FName>& OutDuplicates) const
{
	OutDuplicates.Empty();
	TSet<FName> Seen;

	for (const FPCGExPropertySchema& Schema : Schemas)
	{
		if (Schema.Name.IsNone())
		{
			continue;
		}

		bool bAlreadyInSet = false;
		Seen.Add(Schema.Name, &bAlreadyInSet);

		if (bAlreadyInSet)
		{
			OutDuplicates.AddUnique(Schema.Name);
		}
	}

	return OutDuplicates.IsEmpty();
}

void FPCGExPropertySchemaCollection::SyncAllSchemas()
{
	// Discarded; in the typical (no collision) case the array never allocates.
	TArray<FPCGExHeaderIdRemap> Unused;
	SyncAllSchemas(Unused);
}

void FPCGExPropertySchemaCollection::SyncAllSchemas(TArray<FPCGExHeaderIdRemap>& OutRemaps)
{
#if WITH_EDITOR
	TSet<int32> SeenHeaderIds;
	SeenHeaderIds.Reserve(Schemas.Num());

	auto GenerateUniqueHeaderId = [&SeenHeaderIds]() -> int32
	{
		int32 NewId = 0;
		do
		{
			NewId = GetTypeHash(FGuid::NewGuid());
		}
		while (NewId == 0 || SeenHeaderIds.Contains(NewId));
		return NewId;
	};

	for (FPCGExPropertySchema& Schema : Schemas)
	{
		// Copy-paste preserves the source's HeaderId; without this catch, SyncToSchema's
		// IndexByHeaderId aliases the duplicates and one side's overrides get silently dropped.
		bool bAlreadySeen = false;
		if (Schema.HeaderId != 0)
		{
			SeenHeaderIds.Add(Schema.HeaderId, &bAlreadySeen);
		}
		if (Schema.HeaderId == 0 || bAlreadySeen)
		{
			const int32 OldId = Schema.HeaderId;
			Schema.HeaderId = GenerateUniqueHeaderId();
			SeenHeaderIds.Add(Schema.HeaderId);

			// Zero-bootstraps never appeared in saved overrides, so there is nothing to remap.
			if (bAlreadySeen)
			{
				OutRemaps.Emplace(OldId, Schema.HeaderId, Schema.Name);
			}
		}
		Schema.SyncPropertyName();
	}

	SyncDeclaredChoices();
#else
	for (FPCGExPropertySchema& Schema : Schemas)
	{
		Schema.SyncPropertyName();
	}
#endif
}

bool FPCGExPropertySchemaCollection::SyncDeclaredChoices()
{
	bool bChanged = false;
#if WITH_EDITORONLY_DATA
	for (FPCGExPropertySchema& Schema : Schemas)
	{
		// First: the carry writes the list the steps below maintain.
		bChanged |= Schema.ConsumeRetype();

		FPCGExProperty* Declared = Schema.GetPropertyMutable();
		if (!Declared || (Declared->Choices.Items.IsEmpty() && !Declared->ChosenChoiceId.IsValid()))
		{
			continue;
		}

		// Choice identities follow the same rule as HeaderId: minted here, never by a constructor. Carriers are made
		// plain values under this declaration's rules here too, which is what lets rows mirror the list as it stands.
		bChanged |= Declared->Choices.NormalizeIds();
		bChanged |= PCGExProperties::SanitizeChoices(Declared->Choices.Items);
		bChanged |= PCGExProperties::SyncChoiceCarriers(Declared->Choices.Items, FConstStructView(Schema.Property));
		// The list is this declaration's own, so a pick it no longer offers is dead. The default is never written here:
		// it follows a choice only through CommitDeclaredChoices, which knows the list it stood on.
		bChanged |= PCGExProperties::UnbindDivergedChoice(Schema.Property);
	}
#endif
	return bChanged;
}

void FPCGExPropertySchemaCollection::SyncAllSchemasAndRemap(TFunctionRef<void(TConstArrayView<FPCGExHeaderIdRemap>)> ApplyRemap)
{
	TArray<FPCGExHeaderIdRemap> Remaps;
	SyncAllSchemas(Remaps);
	if (!Remaps.IsEmpty())
	{
		ApplyRemap(Remaps);
	}
}

void FPCGExPropertySchemaCollection::ApplyToOverrides(FPCGExPropertyOverrides& Overrides) const
{
	const TArray<FInstancedStruct> Schema = BuildSchema();
	Overrides.SyncToSchema(Schema);
}

void FPCGExPropertySchemaCollection::ApplyToOverrides(TArray<FPCGExPropertyOverrides>& OverridesArray) const
{
	const TArray<FInstancedStruct> Schema = BuildSchema();
	for (FPCGExPropertyOverrides& Overrides : OverridesArray)
	{
		Overrides.SyncToSchema(Schema);
	}
}

bool FPCGExPropertySchemaCollection::ReconcileImportOverrides()
{
	TArray<FPCGExPropertyResolved> Resolved;
	Resolve(Resolved);
	return ReconcileImportOverrides(Resolved);
}

bool FPCGExPropertySchemaCollection::ReconcileImportOverrides(const TArray<FPCGExPropertyResolved>& Resolved)
{
	check(IsInGameThread());

	bool bChanged = false;
#if WITH_EDITORONLY_DATA
	// Patches are settled in place before the bake below reads them through Resolved, which points at them: ids
	// minted, and the items of a row whose declaration was retyped re-made for the new type like the declaration's own.
	for (int32 i = 0; i < ImportOverrides.Overrides.Num(); ++i)
	{
		FPCGExPropertyChoicesPatch* Patch = ImportOverrides.Overrides[i].ChoicesPatch.GetMutablePtr();
		if (!Patch || Patch->Items.IsEmpty())
		{
			continue;
		}
		bChanged |= Patch->NormalizeIds();

		const FPCGExPropertyResolved* Match = PCGExPropertyOverridesSync::FindProperty(ImportOverrides.Overrides, i, Resolved);
		if (Match && Match->OwningAsset && PCGExPropertyChoicesSync::HasForeignCarrier(Patch->Items, Match->Source->Property))
		{
			Patch->Items = PCGExPropertyChoicesSync::ConvertItems(Patch->Items, Match->Source->Property);
			bChanged = true;
		}
	}
#endif

	TArray<FInstancedStruct> ImportedOnlySchema;
	ImportedOnlySchema.Reserve(Resolved.Num());
	for (const FPCGExPropertyResolved& Entry : Resolved)
	{
		if (!Entry.OwningAsset)
		{
			continue;
		}

		// Feed a local copy of Source->Property patched with the canonical name/HeaderId from
		// Schema. The asset's cached Property->PropertyName can be stale (no PostLoad sync), and
		// SyncToSchema both copies it into new entries and overwrites existing entries with it --
		// a stale empty cache would silently wipe every override's PropertyName to None.
		// GetEffectiveProperty would feed the override back into itself; Source->Property is the
		// canonical schema, patched here to guarantee the name truth.
		FInstancedStruct Patched = Entry.Source->Property;
		if (FPCGExProperty* Prop = Patched.GetMutablePtr<FPCGExProperty>())
		{
			Prop->PropertyName = Entry.Source->Name;
#if WITH_EDITOR
			Prop->HeaderId = Entry.Source->HeaderId;
#endif
#if WITH_EDITORONLY_DATA
			// Import rows show what this collection's consumers get: the declaration's list, every patch applied.
			if (!Entry.ChoicePatches.IsEmpty())
			{
				Entry.GetEffectiveChoices(Prop->Choices);
			}
#endif
		}
		ImportedOnlySchema.Add(MoveTemp(Patched));
	}

	bChanged |= ImportOverrides.SyncToSchema(ImportedOnlySchema);

#if WITH_EDITORONLY_DATA
	// Rows are parallel to the imports-only schema now: authored patch carriers follow their declaration too.
	for (int32 i = 0; i < ImportOverrides.Overrides.Num() && i < ImportedOnlySchema.Num(); ++i)
	{
		if (FPCGExPropertyChoicesPatch* Patch = ImportOverrides.Overrides[i].ChoicesPatch.GetMutablePtr())
		{
			bChanged |= PCGExProperties::SyncChoiceCarriers(Patch->Items, FConstStructView(ImportedOnlySchema[i]));
		}
	}
#endif
	return bChanged;
}

bool FPCGExPropertySchemaCollection::ReconcileImportOverridesAtLoad()
{
	for (const TObjectPtr<UPCGExPropertySchemaAsset>& AssetPtr : ImportedSchemas)
	{
		if (UPCGExPropertySchemaAsset* Asset = AssetPtr.Get())
		{
			Asset->ConditionalPostLoad();
		}
	}

	// A reshape drops the rows of whatever stopped resolving, and at load that can be an asset that failed to load.
	PCGExPropertySchemaResolve::FVisitedAssets Visited;
	return PCGExPropertySchemaResolve::HasMissingImport(*this, Visited) ? SyncImportOverridesInPlace() : ReconcileImportOverrides();
}

bool FPCGExPropertySchemaCollection::SyncImportOverridesInPlace(const TConstArrayView<const FPCGExPropertyOverrides*> FallbackChain)
{
	check(IsInGameThread());

	if (ImportedSchemas.IsEmpty() || ImportOverrides.Overrides.IsEmpty())
	{
		return false;
	}

	TArray<FPCGExPropertyResolved> Resolved;
	Resolve(Resolved, FallbackChain);
	// Import rows stand for imported entries only: a local shadowing one by name is not theirs to mirror.
	Resolved.RemoveAll([](const FPCGExPropertyResolved& Entry) { return Entry.OwningAsset == nullptr; });
	return ImportOverrides.SyncInPlace(Resolved);
}

#if WITH_EDITOR
bool FPCGExPropertySchemaCollection::SyncRowsAtLoad(const TStridedView<FPCGExPropertyOverrides> Rows) const
{
	TArray<FPCGExPropertyResolved> Resolved;
	Resolve(Resolved);

	// A rebuild drops the values of whatever stopped resolving, and at load that can be an asset that failed to load.
	PCGExPropertySchemaResolve::FVisitedAssets Visited;
	const bool bMayRebuild = !PCGExPropertySchemaResolve::HasMissingImport(*this, Visited);

	FPCGExResolvedChoicesCache ChoicesCache;
	TOptional<TArray<FInstancedStruct>> Schema;
	bool bChanged = false;
	for (FPCGExPropertyOverrides& Row : Rows)
	{
		// Rows still in step are never rebuilt: a load must not dirty what did not drift.
		if (!bMayRebuild || PCGExPropertyOverridesSync::IsParallel(Row.Overrides, Resolved))
		{
			bChanged |= Row.SyncInPlace(Resolved, &ChoicesCache);
			continue;
		}
		if (!Schema.IsSet())
		{
			Schema = BuildSchema();
		}
		bChanged |= Row.SyncToSchema(Schema.GetValue());
	}
	return bChanged;
}

bool FPCGExPropertySchemaCollection::ImportsAssetTransitive(const UPCGExPropertySchemaAsset* Asset) const
{
	if (!Asset)
	{
		return false;
	}

	TSet<const UPCGExPropertySchemaAsset*> Visited;
	TArray<const FPCGExPropertySchemaCollection*> Frontier;
	Frontier.Add(this);

	while (!Frontier.IsEmpty())
	{
		const FPCGExPropertySchemaCollection* Current = Frontier.Pop(EAllowShrinking::No);
		for (const TObjectPtr<UPCGExPropertySchemaAsset>& AssetPtr : Current->ImportedSchemas)
		{
			const UPCGExPropertySchemaAsset* Imported = AssetPtr.Get();
			if (!Imported)
			{
				continue;
			}
			if (Imported == Asset)
			{
				return true;
			}
			bool bAlreadyVisited = false;
			Visited.Add(Imported, &bAlreadyVisited);
			if (!bAlreadyVisited)
			{
				Frontier.Add(&Imported->Collection);
			}
		}
	}
	return false;
}
#endif

void FPCGExPropertySchemaCollection::SyncFromArchetype(const FPCGExPropertySchemaCollection& Archetype)
{
#if WITH_EDITOR
	// Fast path: structure already mirrors the archetype. Hits on every register after the first sync.
	if (Schemas.Num() == Archetype.Schemas.Num())
	{
		bool bStructureMatches = true;
		for (int32 i = 0; i < Schemas.Num(); ++i)
		{
			const FPCGExProperty* MyProp = Schemas[i].GetProperty();
			const FPCGExProperty* ArchProp = Archetype.Schemas[i].GetProperty();
			if (!MyProp || !ArchProp ||
				Schemas[i].Property.GetScriptStruct() != Archetype.Schemas[i].Property.GetScriptStruct() ||
				MyProp->HeaderId != ArchProp->HeaderId)
			{
				bStructureMatches = false;
				break;
			}
		}

		if (bStructureMatches)
		{
			// The instance owns its default values and nothing else: raw template writes (a choices commit) reach no
			// instance through the engine, so the schema-owned side is re-mirrored here.
			for (int32 i = 0; i < Schemas.Num(); ++i)
			{
				PCGExPropertyRowSync::MirrorSchemaOwnedFields(Schemas[i].Property, Archetype.Schemas[i].Property, nullptr, true);
			}
			return;
		}
	}

	TArray<FPCGExPropertySchema> OldSchemas = MoveTemp(Schemas);

	TMap<int32, int32> OldIndexByInnerHeaderId;
	OldIndexByInnerHeaderId.Reserve(OldSchemas.Num());
	for (int32 i = 0; i < OldSchemas.Num(); ++i)
	{
		if (const FPCGExProperty* Prop = OldSchemas[i].GetProperty())
		{
			if (Prop->HeaderId != 0)
			{
				OldIndexByInnerHeaderId.Add(Prop->HeaderId, i);
			}
		}
	}

	// Built lazily on the first HeaderId miss -- covers legacy instances saved before the
	// ctor change, when HeaderIds were random and won't match the CDO's
	TMap<FName, int32> OldIndexByName;
	bool bNameMapBuilt = false;

	Schemas.Reset(Archetype.Schemas.Num());

	for (const FPCGExPropertySchema& ArchetypeSchema : Archetype.Schemas)
	{
		FPCGExPropertySchema& NewSchema = Schemas.AddDefaulted_GetRef();
		NewSchema.HeaderId = ArchetypeSchema.HeaderId;
		NewSchema.Name = ArchetypeSchema.Name;

		int32 ExistingIndex = INDEX_NONE;
		if (const FPCGExProperty* ArchProp = ArchetypeSchema.GetProperty())
		{
			if (ArchProp->HeaderId != 0)
			{
				if (const int32* Found = OldIndexByInnerHeaderId.Find(ArchProp->HeaderId))
				{
					ExistingIndex = *Found;
				}
			}
		}

		if (ExistingIndex == INDEX_NONE && !ArchetypeSchema.Name.IsNone())
		{
			if (!bNameMapBuilt)
			{
				bNameMapBuilt = true;
				OldIndexByName.Reserve(OldSchemas.Num());
				for (int32 i = 0; i < OldSchemas.Num(); ++i)
				{
					if (!OldSchemas[i].Name.IsNone())
					{
						OldIndexByName.Add(OldSchemas[i].Name, i);
					}
				}
			}
			if (const int32* Found = OldIndexByName.Find(ArchetypeSchema.Name))
			{
				ExistingIndex = *Found;
			}
		}

		FPCGExPropertySchema* Existing = OldSchemas.IsValidIndex(ExistingIndex) ? &OldSchemas[ExistingIndex] : nullptr;

		if (Existing && Existing->Property.GetScriptStruct() == ArchetypeSchema.Property.GetScriptStruct())
		{
			NewSchema.Property = MoveTemp(Existing->Property);
			PCGExPropertyRowSync::MirrorSchemaOwnedFields(NewSchema.Property, ArchetypeSchema.Property, nullptr, true);
		}
		else
		{
			NewSchema.Property = ArchetypeSchema.Property;
		}

		NewSchema.SyncPropertyName();
	}
#endif
}

#pragma endregion

#pragma region FPCGExPropertyOverrides

// ============================================================================
// SyncToSchema - The heart of the override system
// ============================================================================
//
// This method rebuilds the Overrides array to match a new schema while preserving
// user state (enabled flags, values) through schema changes. It uses HeaderId
// (editor-only) as the stable key for matching.
//
// The algorithm:
// 1. Save old overrides and find the property each one stands for
//    (PCGExPropertyOverridesSync::FindPropertyIndex); a property takes at most one of them
// 2. For each schema property:
//    a. Take the existing override that stands for it, if any
//    b. If found AND same type: keep value + bEnabled, update PropertyName
//    c. If found but type changed: use schema default value, keep bEnabled
//    d. If not found (new property): use schema default, set bEnabled=false
// 3. Result: Overrides array is parallel with Schema array
//
// At runtime (no WITH_EDITOR), the HeaderId path is skipped and all overrides
// are rebuilt from schema defaults.

bool FPCGExPropertyOverrides::SyncToSchema(const TArray<FInstancedStruct>& Schema, const bool bFollowPicks)
{
#if WITH_EDITOR
	// Fast path: structure matches entry-for-entry. Refresh in place; the slow path below
	// reallocates Overrides, dangling FStructOnScope aliases held by per-entry detail
	// customizations (causes scrambled values, undo-time access violations). Reconcile
	// usually arrives with no structural change, so this is the hot path.
	if (Overrides.Num() == Schema.Num())
	{
		bool bStructureMatches = true;
		for (int32 i = 0; i < Schema.Num(); ++i)
		{
			const FPCGExProperty* SchemaProp = Schema[i].GetPtr<FPCGExProperty>();
			if (!SchemaProp ||
				Overrides[i].Value.GetScriptStruct() != Schema[i].GetScriptStruct() ||
				Overrides[i].HeaderId != SchemaProp->HeaderId)
			{
				bStructureMatches = false;
				break;
			}
			// A HeaderId under another name is a rename in place, unless that id ties the row to a property
			// elsewhere: two properties sharing it traded places, and each row follows its own.
			if (Overrides[i].PropertyName != SchemaProp->PropertyName)
			{
				PCGExPropertyOverridesSync::ERowTie Tie = PCGExPropertyOverridesSync::ERowTie::None;
				const int32 StandsFor = PCGExPropertyOverridesSync::FindPropertyIndex<FInstancedStruct>(Overrides, i, Schema, &Tie);
				if (StandsFor != i && Tie == PCGExPropertyOverridesSync::ERowTie::HeaderId)
				{
					bStructureMatches = false;
					break;
				}
			}
		}
		if (bStructureMatches)
		{
			bool bChanged = false;
			for (int32 i = 0; i < Schema.Num(); ++i)
			{
				const FPCGExProperty* SchemaProp = Schema[i].GetPtr<FPCGExProperty>();
				if (Overrides[i].PropertyName != SchemaProp->PropertyName)
				{
					Overrides[i].PropertyName = SchemaProp->PropertyName;
					bChanged = true;
				}
				if (FPCGExProperty* MyProp = Overrides[i].GetPropertyMutable())
				{
					if (MyProp->PropertyName != SchemaProp->PropertyName)
					{
						MyProp->PropertyName = SchemaProp->PropertyName;
						bChanged = true;
					}
				}
				if (Overrides[i].SyncStructuralFields(Schema[i], nullptr, bFollowPicks))
				{
					bChanged = true;
				}
			}
			return bChanged;
		}
	}
#endif

	// Match existing entries to the new schema by OUTER identity, under the rule every row sync shares
	// (PCGExPropertyOverridesSync::FindPropertyIndex). Outer identity lives on FPCGExPropertyOverrideEntry
	// directly, not inside Value's FInstancedStruct, so it survives UE's broken per-property delta propagation.
	TArray<FPCGExPropertyOverrideEntry> OldOverrides = MoveTemp(Overrides);

#if WITH_EDITOR
	// One-shot inner→outer migration for legacy data, before rows are matched on that identity.
	// Idempotent: skips entries that already carry outer identity. The CDO is the trigger
	// -- once any reconcile fires for a CDO, its entries acquire outer identity here, and UE
	// per-property delta propagates the outer scalars (FName, int32) to instances correctly
	// on the next CDO edit.
	for (FPCGExPropertyOverrideEntry& Entry : OldOverrides)
	{
		if (Entry.HeaderId == 0 || Entry.PropertyName.IsNone())
		{
			if (const FPCGExProperty* Prop = Entry.Value.GetPtr<FPCGExProperty>())
			{
				if (Entry.HeaderId == 0)
				{
					Entry.HeaderId = Prop->HeaderId;
				}
				if (Entry.PropertyName.IsNone())
				{
					Entry.PropertyName = Prop->PropertyName;
				}
			}
		}
	}

	// Each old row goes to the property it stands for, and to no other. A HeaderId tie outranks a name tie
	// whatever the row order; of two rows tied alike, the first keeps the property.
	using PCGExPropertyOverridesSync::ERowTie;
	TArray<int32, TInlineAllocator<32>> OldRowOf;
	TArray<ERowTie, TInlineAllocator<32>> TieOf;
	OldRowOf.Init(INDEX_NONE, Schema.Num());
	TieOf.Init(ERowTie::None, Schema.Num());
	for (int32 i = 0; i < OldOverrides.Num(); ++i)
	{
		ERowTie Tie = ERowTie::None;
		const int32 PropertyIndex = PCGExPropertyOverridesSync::FindPropertyIndex<FInstancedStruct>(OldOverrides, i, Schema, &Tie);
		if (PropertyIndex != INDEX_NONE && Tie > TieOf[PropertyIndex])
		{
			OldRowOf[PropertyIndex] = i;
			TieOf[PropertyIndex] = Tie;
		}
	}
#endif

	Overrides.Reset(Schema.Num());

	for (int32 SchemaIndex = 0; SchemaIndex < Schema.Num(); ++SchemaIndex)
	{
		const FInstancedStruct& SchemaProp = Schema[SchemaIndex];
		const FPCGExProperty* SchemaData = SchemaProp.GetPtr<FPCGExProperty>();
		if (!SchemaData)
		{
			continue;
		}

		FPCGExPropertyOverrideEntry& NewEntry = Overrides.AddDefaulted_GetRef();
#if WITH_EDITORONLY_DATA
		// Outer identity cache (editor-only). Cooked path skips this; the inner
		// FPCGExProperty::PropertyName carried inside SchemaProp is sufficient at runtime.
		NewEntry.PropertyName = SchemaData->PropertyName;
		NewEntry.HeaderId = SchemaData->HeaderId;
#endif

		FPCGExPropertyOverrideEntry* Existing = nullptr;

#if WITH_EDITOR
		if (OldRowOf[SchemaIndex] != INDEX_NONE)
		{
			Existing = &OldOverrides[OldRowOf[SchemaIndex]];
		}

		// Last-resort parallel-index fallback. The Overrides array is structurally parallel to
		// the imports-only schema by construction (every reconcile writes them in lockstep), so
		// when an entry has no usable outer OR inner identity -- which happens when UE's
		// per-property delta blanked the inner FInstancedStruct AND outer identity hadn't been
		// migrated onto the CDO yet -- the same-index slot is the only signal left. Critical
		// for preserving bEnabled toggles on instances of pre-fix authored data.
		//
		// Gated on the candidate ALSO being identity-less: an entry that still carries
		// HeaderId/Name but didn't match any schema slot is a removed property, not a
		// propagation casualty -- routing it here would misread "remove + add" as "rename"
		// and silently preserve a stale value (and stale inner HeaderId) under the new name.
		if (!Existing && OldOverrides.IsValidIndex(SchemaIndex))
		{
			FPCGExPropertyOverrideEntry& Candidate = OldOverrides[SchemaIndex];
			if (Candidate.HeaderId == 0 && Candidate.PropertyName.IsNone())
			{
				Existing = &Candidate;
			}
		}
#endif

		if (Existing)
		{
			NewEntry.bEnabled = Existing->bEnabled;
#if WITH_EDITORONLY_DATA
			// Kept through a retype as well: a patch binds to choice ids, which the declaration keeps across one.
			// ReconcileImportOverrides re-makes its items for the new type before it gets here.
			NewEntry.ChoicesPatch = MoveTemp(Existing->ChoicesPatch);
#endif

			if (Existing->Value.IsValid() && Existing->Value.GetScriptStruct() == SchemaProp.GetScriptStruct())
			{
				// Same type: the value is kept (one standing on its picked choice follows that choice), the inner
				// PropertyName and the schema-owned fields are refreshed.
				NewEntry.Value = MoveTemp(Existing->Value);
				if (FPCGExProperty* Prop = NewEntry.GetPropertyMutable())
				{
					Prop->PropertyName = SchemaData->PropertyName;
				}
				NewEntry.SyncStructuralFields(SchemaProp, nullptr, bFollowPicks);
			}
			else
			{
				// Type changed, or inner Value lost to broken FInstancedStruct propagation.
				// Take schema default; bEnabled (preserved above) carries the user's authoring intent.
				NewEntry.Value = SchemaProp;
			}
		}
		else
		{
			NewEntry.Value = SchemaProp;
			NewEntry.bEnabled = false;
		}
	}

	// Slow path always rebuilds storage -- the relocated backing memory is itself an
	// observable change regardless of bitwise content equality.
	return true;
}

namespace PCGExPropertyOverridesSync
{
	const FPCGExPropertyResolved* FindProperty(const TConstArrayView<FPCGExPropertyOverrideEntry> Rows, const int32 RowIndex, const TConstArrayView<FPCGExPropertyResolved> Resolved)
	{
		const int32 PropertyIndex = FindPropertyIndex(Rows, RowIndex, Resolved);
		return PropertyIndex == INDEX_NONE ? nullptr : &Resolved[PropertyIndex];
	}

	bool IsParallel(const TConstArrayView<FPCGExPropertyOverrideEntry> Rows, const TConstArrayView<FPCGExPropertyResolved> Resolved)
	{
		if (Rows.Num() != Resolved.Num())
		{
			return false;
		}
		for (int32 i = 0; i < Rows.Num(); ++i)
		{
			if (FindPropertyIndex(Rows, i, Resolved) != i || Rows[i].Value.GetScriptStruct() != Resolved[i].Source->Property.GetScriptStruct())
			{
				return false;
			}
		}
		return true;
	}
}

bool FPCGExPropertyOverrides::SyncInPlace(TConstArrayView<FPCGExPropertyResolved> Resolved, FPCGExResolvedChoicesCache* ChoicesCache)
{
#if WITH_EDITORONLY_DATA
	FPCGExResolvedChoicesCache LocalCache;
	FPCGExResolvedChoicesCache& Cache = ChoicesCache ? *ChoicesCache : LocalCache;
#endif

	bool bChanged = false;
	for (int32 i = 0; i < Overrides.Num(); ++i)
	{
		FPCGExPropertyOverrideEntry& Entry = Overrides[i];
		const FPCGExPropertyResolved* Match = PCGExPropertyOverridesSync::FindProperty(Overrides, i, Resolved);
		if (!Match)
		{
			continue;
		}

		// The declaration, not the effective property: an override standing in for it supplies a value, and its own
		// schema-owned fields are mirrors that may be stale (or are this very row). Both are one type by Resolve's guard.
		// A row of another type than its property is not touched: only SyncToSchema may replace a value.
		const FInstancedStruct& SchemaValue = Match->Source->Property;
		FPCGExProperty* Row = Entry.GetPropertyMutable();
		if (!Row || Entry.Value.GetScriptStruct() != SchemaValue.GetScriptStruct())
		{
			continue;
		}

		// Names are only written for a rename: a row matched by name already has it, and back-filling its
		// identity caches is SyncToSchema's migration. The schema's outer Name is the truth, because an
		// import that has not run PostLoad can carry a stale inner PropertyName.
		const FName SchemaName = Match->Source->Name;
		if (Entry.GetPropertyName() != SchemaName)
		{
			Row->PropertyName = SchemaName;
#if WITH_EDITORONLY_DATA
			Entry.PropertyName = SchemaName;
#endif
			bChanged = true;
		}

#if WITH_EDITORONLY_DATA
		// Resolved hands out pointers, not baked copies. A declaration nothing patches already holds the effective list.
		const FPCGExPropertyChoices* EffectiveChoices = Match->ChoicePatches.IsEmpty()
			? nullptr
			: &Cache.Get(Resolved, UE_PTRDIFF_TO_INT32(Match - Resolved.GetData()));
		if (Entry.SyncStructuralFields(SchemaValue, EffectiveChoices))
#else
		if (Entry.SyncStructuralFields(SchemaValue))
#endif
		{
			bChanged = true;
		}
	}
	return bChanged;
}

void FPCGExPropertyOverrides::ApplyHeaderIdRemap(TConstArrayView<FPCGExHeaderIdRemap> Remaps)
{
#if WITH_EDITOR
	if (Remaps.IsEmpty())
	{
		return;
	}

	for (FPCGExPropertyOverrideEntry& Entry : Overrides)
	{
		// Match on (OldId, Name): OldId alone aliases multiple schema entries pre-regeneration,
		// so Name is required to pin the remap to the specific entry whose HeaderId changed.
		for (const FPCGExHeaderIdRemap& Remap : Remaps)
		{
			if (Entry.HeaderId == Remap.OldId && Entry.PropertyName == Remap.Name)
			{
				Entry.HeaderId = Remap.NewId;
				if (FPCGExProperty* Prop = Entry.GetPropertyMutable())
				{
					Prop->HeaderId = Remap.NewId;
				}
				break;
			}
		}
	}
#endif
}

const FInstancedStruct* FPCGExPropertyOverrides::GetOverride(FName PropertyName) const
{
	for (const FPCGExPropertyOverrideEntry& Entry : Overrides)
	{
		if (Entry.bEnabled && Entry.GetPropertyName() == PropertyName)
		{
			return &Entry.Value;
		}
	}
	return nullptr;
}

const FInstancedStruct* FPCGExPropertyOverrides::FindEnabledSlot(const FName PropertyName, const UScriptStruct* RequiredType, const int32 IndexHint) const
{
	if (!RequiredType)
	{
		return nullptr;
	}

	auto IsSlot = [PropertyName, RequiredType](const FPCGExPropertyOverrideEntry& Entry)
	{
		return Entry.bEnabled && Entry.GetPropertyName() == PropertyName && Entry.Value.IsValid()
			&& Entry.Value.GetScriptStruct()->IsChildOf(RequiredType);
	};

	if (Overrides.IsValidIndex(IndexHint) && IsSlot(Overrides[IndexHint]))
	{
		return &Overrides[IndexHint].Value;
	}
	for (const FPCGExPropertyOverrideEntry& Entry : Overrides)
	{
		if (IsSlot(Entry))
		{
			return &Entry.Value;
		}
	}
	return nullptr;
}

#pragma endregion

#pragma region PCGExProperties soft object path walk

namespace PCGExPropertySoftPaths
{
	// Visited tracks UPCGExPropertySchemaAsset pointers so an A->B->A import cycle
	// terminates instead of looping. Peer to PCGExPropertySchemaResolve::Walk; distinct
	// because Resolve dedups locals/imports by Name (first-wins) and walks override
	// layers as a chain -- both wrong here, which must surface every soft path
	// regardless of shadowing.
	void WalkOverrides(const FPCGExPropertyOverrides& Overrides, const TFunctionRef<void(const FPCGExProperty&)> Visitor)
	{
		for (const FPCGExPropertyOverrideEntry& Entry : Overrides.Overrides)
		{
			if (!Entry.bEnabled)
			{
				continue;
			}
			if (const FPCGExProperty* Prop = Entry.GetProperty())
			{
				Visitor(*Prop);
			}
		}
	}

	void WalkCollection(
		const FPCGExPropertySchemaCollection& Collection,
		TSet<const UPCGExPropertySchemaAsset*>& Visited,
		const TFunctionRef<void(const FPCGExProperty&)> Visitor)
	{
		for (const FPCGExPropertySchema& Schema : Collection.Schemas)
		{
			if (const FPCGExProperty* Prop = Schema.GetProperty())
			{
				Visitor(*Prop);
			}
		}

		WalkOverrides(Collection.ImportOverrides, Visitor);

		// Imported schema assets are auto-cooked via hard refs, but we still walk into
		// each one's Collection to surface its leaf soft paths.
		for (const TObjectPtr<UPCGExPropertySchemaAsset>& Asset : Collection.ImportedSchemas)
		{
			if (!Asset)
			{
				continue;
			}
			bool bAlreadyVisited = false;
			Visited.Add(Asset.Get(), &bAlreadyVisited);
			if (bAlreadyVisited)
			{
				continue;
			}
			WalkCollection(Asset->Collection, Visited, Visitor);
		}
	}
}

namespace PCGExProperties
{
	void GatherSoftObjectPaths(const FPCGExPropertyOverrides& Overrides, TSet<FSoftObjectPath>& OutPaths)
	{
		PCGExPropertySoftPaths::WalkOverrides(Overrides, [&OutPaths](const FPCGExProperty& Prop) { Prop.GatherSoftObjectPaths(OutPaths); });
	}

	void GatherSoftObjectPaths(const FPCGExPropertySchemaCollection& Collection, TSet<FSoftObjectPath>& OutPaths)
	{
		TSet<const UPCGExPropertySchemaAsset*> Visited;
		PCGExPropertySoftPaths::WalkCollection(Collection, Visited, [&OutPaths](const FPCGExProperty& Prop) { Prop.GatherSoftObjectPaths(OutPaths); });
	}

	void GatherOutputDependencies(const FPCGExPropertyOverrides& Overrides, TSet<FSoftObjectPath>& OutPaths)
	{
		PCGExPropertySoftPaths::WalkOverrides(Overrides, [&OutPaths](const FPCGExProperty& Prop) { Prop.GatherOutputDependencies(OutPaths); });
	}

	void GatherOutputDependencies(const FPCGExPropertySchemaCollection& Collection, TSet<FSoftObjectPath>& OutPaths)
	{
		TSet<const UPCGExPropertySchemaAsset*> Visited;
		PCGExPropertySoftPaths::WalkCollection(Collection, Visited, [&OutPaths](const FPCGExProperty& Prop) { Prop.GatherOutputDependencies(OutPaths); });
	}

	const FInstancedStruct* ResolveEffective(
		const FPCGExPropertySchemaCollection& InSchema, const FPCGExPropertyOverrides* InOverrides, const FName InName)
	{
		// Already walks locals -> ImportOverrides -> imported defaults.
		const FInstancedStruct* SchemaEntry = InSchema.GetPropertyByName(InName);
		if (!InOverrides || !SchemaEntry)
		{
			return SchemaEntry;
		}

		// A type mismatch means the schema was retyped without this override being re-synced. Every
		// CopyValueFrom is an unchecked static_cast, so handing the override back would type-pun.
		const FInstancedStruct* Override = InOverrides->GetOverride(InName);
		return Override && Override->GetScriptStruct() == SchemaEntry->GetScriptStruct() ? Override : SchemaEntry;
	}
}

#pragma endregion
