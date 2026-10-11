// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExPropertyInlineWidgets.h"

#include "Editor.h"
#include "PCGExPropertyTypes.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "Styling/AppStyle.h"
#include "UObject/SoftObjectPath.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Input/SNumericEntryBox.h"

namespace PCGExPropertyInlineWidgets
{
	namespace Private
	{
		// Address of a sibling field's value, reached through AccessOwnerRaw rather than handle navigation (see
		// the header). Null, with OutSiblingProp cleared, when unreachable: no such field, no raw data, or a
		// multi-object edit. Callers treat that as "no constraint".
		void* AccessSiblingRaw(
			const TSharedRef<IPropertyHandle>& ValueHandle,
			FName SiblingName,
			FProperty*& OutSiblingProp)
		{
			OutSiblingProp = nullptr;

			const FProperty* ValueProp = ValueHandle->GetProperty();
			if (!ValueProp)
			{
				return nullptr;
			}
			const UStruct* OwnerStruct = ValueProp->GetOwnerStruct();
			if (!OwnerStruct)
			{
				return nullptr;
			}
			OutSiblingProp = OwnerStruct->FindPropertyByName(SiblingName);
			if (!OutSiblingProp)
			{
				return nullptr;
			}

			TArray<void*> RawData;
			ValueHandle->AccessRawData(RawData);
			if (RawData.Num() != 1 || !RawData[0])
			{
				OutSiblingProp = nullptr;
				return nullptr;
			}

			return OutSiblingProp->ContainerPtrToValuePtr<void>(AccessOwnerRaw(ValueProp, RawData[0]));
		}

		template <typename T>
		T* AccessSibling(const TSharedRef<IPropertyHandle>& ValueHandle, FName SiblingName)
		{
			FProperty* IgnoredProp = nullptr;
			return static_cast<T*>(AccessSiblingRaw(ValueHandle, SiblingName, IgnoredProp));
		}

		// Read a TSubclassOf-backed sibling field. Routes through FObjectPropertyBase so
		// both raw UClass* and TObjectPtr<UClass> storage work.
		UClass* ReadSiblingClass(const TSharedRef<IPropertyHandle>& ValueHandle, FName SiblingName)
		{
			FProperty* SiblingProp = nullptr;
			void* ContainerPtr = AccessSiblingRaw(ValueHandle, SiblingName, SiblingProp);
			if (!ContainerPtr)
			{
				return nullptr;
			}
			FObjectPropertyBase* ClassProp = CastField<FObjectPropertyBase>(SiblingProp);
			if (!ClassProp)
			{
				return nullptr;
			}
			// ContainerPtr is the value slot directly (ContainerPtrToValuePtr already
			// applied the offset). FObjectPropertyBase::GetObjectPropertyValue takes a
			// pointer-to-the-slot, not container-relative.
			return Cast<UClass>(ClassProp->GetObjectPropertyValue(ContainerPtr));
		}
	}

	TSharedRef<SWidget> MakeSoftObjectPathWidget(const TSharedRef<IPropertyHandle>& ValueHandle)
	{
		// Snapshot AllowedClass once. The factory re-runs on PostEditChangeProperty
		// (which fires when the schema author changes AllowedClass), so live updates
		// arrive via fresh widgets, not via per-frame attribute polling. Snapshotting
		// matters for the filter callback specifically -- the Content Browser invokes
		// OnShouldFilterAsset thousands of times during picker scroll.
		UClass* AllowedClass = Private::ReadSiblingClass(ValueHandle, TEXT("AllowedClass"));
		if (!AllowedClass)
		{
			AllowedClass = UObject::StaticClass();
		}

		auto OnShouldFilter = FOnShouldFilterAsset::CreateLambda([AllowedClass](const FAssetData& AssetData) -> bool
		{
			if (AllowedClass == UObject::StaticClass())
			{
				return false;
			}
			const UClass* AssetClass = AssetData.GetClass();
			return !AssetClass || !AssetClass->IsChildOf(AllowedClass);
		});

		return SNew(SObjectPropertyEntryBox)
			.PropertyHandle(ValueHandle)
			.AllowedClass(AllowedClass)
			.OnShouldFilterAsset(OnShouldFilter)
			.AllowClear(true)
			.DisplayBrowse(true)
			.DisplayThumbnail(false)
			.DisplayUseSelected(true);
	}

	TSharedRef<SWidget> MakeSoftClassPathWidget(const TSharedRef<IPropertyHandle>& ValueHandle)
	{
		UClass* MetaClass = Private::ReadSiblingClass(ValueHandle, TEXT("AllowedClass"));
		if (!MetaClass)
		{
			MetaClass = UObject::StaticClass();
		}

		auto SelectedClassAttr = TAttribute<const UClass*>::Create([ValueHandle]() -> const UClass*
		{
			void* RawData = nullptr;
			if (ValueHandle->GetValueData(RawData) != FPropertyAccess::Success || !RawData)
			{
				return nullptr;
			}
			const FSoftClassPath* Path = static_cast<const FSoftClassPath*>(RawData);
			return Path ? Path->ResolveClass() : nullptr;
		});

		auto OnSetClass = FOnSetClass::CreateLambda([ValueHandle](const UClass* NewClass)
		{
			const FSoftClassPath NewPath(const_cast<UClass*>(NewClass));
			ValueHandle->SetValueFromFormattedString(NewPath.ToString());
		});

		return SNew(SClassPropertyEntryBox)
			.MetaClass(MetaClass)
			.AllowAbstract(true)
			.AllowNone(true)
			.SelectedClass(SelectedClassAttr)
			.OnSetClass(OnSetClass)
			.ShowDisplayNames(false)
			.IsBlueprintBaseOnly(false);
	}

	namespace Private
	{
		// Snapshot Range bounds at factory time. The schema's bClampMin/Max toggle and
		// Min/Max values can only change via PostEditChangeProperty, which re-runs the
		// factory; on override rows they're synced read-only from the schema and stable
		// for the row's lifetime. Snapshotting saves a FindPropertyByName + AccessRawData
		// per paint frame per row.
		template <typename T>
		TSharedRef<SWidget> MakeClampedNumericWidget(const TSharedRef<IPropertyHandle>& ValueHandle)
		{
			TOptional<T> MinBound;
			TOptional<T> MaxBound;
			if (const FPCGExNumericRange* Range = AccessSibling<FPCGExNumericRange>(ValueHandle, TEXT("Range")))
			{
				if (Range->bClampMin)
				{
					MinBound = static_cast<T>(Range->Min);
				}
				if (Range->bClampMax)
				{
					MaxBound = static_cast<T>(Range->Max);
				}
			}

			// The drag in progress, if any: the editor transaction that spans it is open exactly while this is true.
			const TSharedRef<bool> bDragging = MakeShared<bool>(false);
			auto EndDrag = [bDragging]()
			{
				if (*bDragging)
				{
					*bDragging = false;
					GEditor->EndTransaction();
				}
			};

			return SNew(SNumericEntryBox<T>)
				.AllowSpin(true)
				// SSpinBox reports a wheel notch as a bare OnValueChanged, which is also what its echoes look like
				// (see below): there is no telling them apart, so the wheel is off rather than half applied.
				.AllowWheel(false)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.MinValue(MinBound)
				.MaxValue(MaxBound)
				.MinSliderValue(MinBound)
				.MaxSliderValue(MaxBound)
				.MinDesiredValueWidth(60.0f)
				.Value_Lambda([ValueHandle]() -> TOptional<T>
				{
					T Out = T(0);
					if (ValueHandle->GetValue(Out) == FPropertyAccess::Success)
					{
						return Out;
					}
					return TOptional<T>();
				})
				// Only inside a drag, as SPropertyEditorNumeric does: outside one SSpinBox raises this for things
				// nobody did (a late flush after mouse-up or Escape, an echo of the bound value), and hosts
				// drop Interactive changes anyway, so applying it would leave the value written and nothing synced.
				.OnValueChanged_Lambda([ValueHandle, bDragging](T NewValue)
				{
					if (*bDragging)
					{
						ValueHandle->SetValue(NewValue, EPropertyValueSetFlags::InteractiveChange);
					}
				})
				// SSpinBox commits right before it ends a drag, and can lose mouse capture without ever ending
				// one: a commit closes the drag either way, so its transaction cannot stay open.
				.OnValueCommitted_Lambda([ValueHandle, EndDrag](T NewValue, ETextCommit::Type)
				{
					ValueHandle->SetValue(NewValue, EPropertyValueSetFlags::DefaultFlags);
					EndDrag();
				})
				.OnBeginSliderMovement_Lambda([bDragging]()
				{
					if (!*bDragging)
					{
						*bDragging = true;
						GEditor->BeginTransaction(NSLOCTEXT("PCGExPropertyInlineWidgets", "SetNumericValue", "Set Value"));
					}
				})
				.OnEndSliderMovement_Lambda([EndDrag](T)
				{
					EndDrag();
				});
		}
	}

	TSharedRef<SWidget> MakeClampedFloatWidget(const TSharedRef<IPropertyHandle>& ValueHandle)
	{
		return Private::MakeClampedNumericWidget<float>(ValueHandle);
	}

	TSharedRef<SWidget> MakeClampedDoubleWidget(const TSharedRef<IPropertyHandle>& ValueHandle)
	{
		return Private::MakeClampedNumericWidget<double>(ValueHandle);
	}

	TSharedRef<SWidget> MakeClampedInt32Widget(const TSharedRef<IPropertyHandle>& ValueHandle)
	{
		return Private::MakeClampedNumericWidget<int32>(ValueHandle);
	}

	TSharedRef<SWidget> MakeClampedInt64Widget(const TSharedRef<IPropertyHandle>& ValueHandle)
	{
		return Private::MakeClampedNumericWidget<int64>(ValueHandle);
	}
}
