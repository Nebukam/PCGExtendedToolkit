// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExPropertyChoicesCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Details/PCGExPropertyChoicesEditTargets.h"
#include "Details/PCGExRawPropertyEdit.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailsView.h"
#include "IPropertyUtilities.h"
#include "InstancedStructDetails.h"
#include "PCGExInlineWidgetRegistry.h"
#include "PCGExProperty.h"
#include "PCGExPropertyChoicesAlign.h"
#include "PCGExPropertyRowHost.h"
#include "PCGExPropertySchemaAsset.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Textures/SlateIcon.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SPCGExPropertyChoicesEditor.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FPCGExPropertyChoicesCustomization"

namespace PCGExPropertyChoicesCustomization
{
	/** The nearest ancestor handle whose struct is exactly Struct; null when there is none. */
	TSharedPtr<IPropertyHandle> FindAncestor(const TSharedRef<IPropertyHandle>& Handle, const UScriptStruct* Struct)
	{
		for (TSharedPtr<IPropertyHandle> Parent = Handle->GetParentHandle(); Parent.IsValid(); Parent = Parent->GetParentHandle())
		{
			const FStructProperty* StructProperty = CastField<FStructProperty>(Parent->GetProperty());
			if (StructProperty && StructProperty->Struct == Struct)
			{
				return Parent;
			}
		}
		return nullptr;
	}

	/** The layers Collection resolves through below its own, when Owner is the row host that collection belongs to. */
	void GetFallbackChain(UObject* Owner, const FPCGExPropertySchemaCollection* Collection, IPCGExPropertyRowHost::FLayerChain& OutChain)
	{
		const IPCGExPropertyRowHost* RowHost = Cast<IPCGExPropertyRowHost>(Owner);
		if (RowHost && RowHost->EDITOR_GetRowSchema() == Collection)
		{
			RowHost->EDITOR_GetRowSchemaFallbackChain(OutChain);
		}
	}

	/** The imported entry named PropertyName in a resolved view; null for a local or an unknown name. */
	const FPCGExPropertyResolved* FindImported(const TArray<FPCGExPropertyResolved>& Resolved, const FName PropertyName)
	{
		return Resolved.FindByPredicate([PropertyName](const FPCGExPropertyResolved& Candidate)
		{
			return Candidate.OwningAsset && Candidate.Source->Name == PropertyName;
		});
	}

	/** "Choice N" for the lowest N neither list uses: a count would repeat after a delete or clash with an inherited label. */
	FString MakeUniqueLabel(const TArray<FPCGExPropertyChoice>& Own, const TArray<FPCGExPropertyChoice>* Inherited = nullptr)
	{
		for (int32 Number = 1; ; ++Number)
		{
			const FString Candidate = FString::Printf(TEXT("Choice %d"), Number);
			auto Uses = [&Candidate](const FPCGExPropertyChoice& Choice)
			{
				return Choice.Label.Equals(Candidate, ESearchCase::IgnoreCase);
			};
			if (!Own.ContainsByPredicate(Uses) && !(Inherited && Inherited->ContainsByPredicate(Uses)))
			{
				return Candidate;
			}
		}
	}

	/** Editor toast saying what an align run reached, including when that was nothing. */
	void NotifyAlignResult(const PCGExPropertyChoicesAlign::FResult& Result)
	{
		FText Message;
		if (Result.RowsAligned > 0)
		{
			Message = FText::Format(
				LOCTEXT("Aligned", "Aligned {0} row(s) in {1} of the {2} host(s) checked."),
				FText::AsNumber(Result.RowsAligned), FText::AsNumber(Result.HostsTouched), FText::AsNumber(Result.HostsVisited));
		}
		else if (Result.HostsVisited > 0)
		{
			Message = FText::Format(LOCTEXT("NothingToAlign", "No row needed aligning: {0} host(s) checked."), FText::AsNumber(Result.HostsVisited));
		}
		else
		{
			Message = LOCTEXT("NothingVisited", "Nothing to align: no loaded host resolves this property with choices.");
		}

		FNotificationInfo Info(Message);
		Info.ExpireDuration = 4.0f;
		Info.bUseSuccessFailIcons = true;
		if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(Result.RowsAligned > 0 ? SNotificationItem::CS_Success : SNotificationItem::CS_None);
		}
	}

	// By value: CreateStatic binds payloads to parameters of their decayed type, a reference does not match.
	void RunAlign(const TWeakPtr<FPCGExChoicesEditSession> WeakSession, const bool bImporters)
	{
		const TSharedPtr<FPCGExChoicesEditSession> Session = WeakSession.Pin();
		if (!Session.IsValid() || !Session->IsValid())
		{
			return;
		}

		UObject* Owner = Session->GetOwner();
		const FName PropertyName = Session->GetPropertyName();
		const UPCGExPropertySchemaAsset* DeclaringAsset = Session->GetDeclaringAsset();

		const FScopedTransaction Transaction(bImporters
			? LOCTEXT("AlignImportersTransaction", "Align Importers to Choices")
			: LOCTEXT("AlignHostRowsTransaction", "Align Rows to Choices"));
		NotifyAlignResult(bImporters
			? PCGExPropertyChoicesAlign::AlignImporters(Cast<UPCGExPropertySchemaAsset>(Owner), PropertyName, DeclaringAsset)
			: PCGExPropertyChoicesAlign::AlignHostRows(Owner, PropertyName, Session->IsPatch() ? DeclaringAsset : nullptr));
	}

	/** The align lever as a small combo: this host's rows, or every loaded importer of a schema asset. */
	TSharedRef<SWidget> MakeAlignButton(const TWeakPtr<FPCGExChoicesEditSession>& WeakSession)
	{
		return SNew(SComboButton)
			.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
			.HasDownArrow(true)
			.ToolTipText(LOCTEXT("AlignToolTip", "Snap rows whose value is none of the choices onto the default choice (or the first that fits). Rows already on a choice are left alone."))
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AlignLabel", "Align"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.OnGetMenuContent_Lambda([WeakSession]() -> TSharedRef<SWidget>
			{
				const TSharedPtr<FPCGExChoicesEditSession> Session = WeakSession.Pin();
				UObject* Owner = Session.IsValid() && Session->IsValid() ? Session->GetOwner() : nullptr;
				const bool bAsset = Owner && Owner->IsA<UPCGExPropertySchemaAsset>();
				const bool bRowHost = Owner && Owner->GetClass()->ImplementsInterface(UPCGExPropertyRowHost::StaticClass());
				// A schema asset holds no row of a property it declares itself: only its importers do.
				const bool bHostRows = bRowHost && !(bAsset && !Session->IsPatch());

				// bCloseSelfOnly: the default dismisses every open menu, and this one lives inside the popover.
				FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection*/ true, nullptr, nullptr, /*bInCloseSelfOnly*/ true);
				MenuBuilder.AddMenuEntry(
					LOCTEXT("AlignHostRows", "Align this host's rows"),
					LOCTEXT("AlignHostRowsToolTip", "Every enabled row of this collection, node, component or asset whose value is not one of the choices, its own import override included."),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateStatic(&RunAlign, WeakSession, false),
						FCanExecuteAction::CreateLambda([bHostRows]() { return bHostRows; })));
				MenuBuilder.AddMenuEntry(
					LOCTEXT("AlignImporters", "Align all loaded importers"),
					LOCTEXT("AlignImportersToolTip", "Same, in every loaded collection, node, component or schema asset importing this asset. Unloaded assets and Blueprint templates are not visited."),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateStatic(&RunAlign, WeakSession, true),
						FCanExecuteAction::CreateLambda([bAsset]() { return bAsset; })));
				return MenuBuilder.MakeWidget();
			});
	}
}

#pragma region FPCGExChoicesEditSession

TSharedRef<FPCGExChoicesEditSession> FPCGExChoicesEditSession::MakeForDeclaration(const TSharedRef<IPropertyHandle>& ChoicesHandle, const FPCGExPropertyHostRef& Host)
{
	const TSharedRef<FPCGExChoicesEditSession> Session = MakeShareable(new FPCGExChoicesEditSession());
	Session->bPatch = false;
	Session->TargetHandle = ChoicesHandle;
	Session->Host = Host;

	Session->Scratch = MakeShared<FStructOnScope>(FPCGExPropertyChoicesEditTarget::StaticStruct());
	Session->ScratchMember = &reinterpret_cast<FPCGExPropertyChoicesEditTarget*>(Session->Scratch->GetStructMemory())->Choices;

	TArray<UObject*> Outers;
	ChoicesHandle->GetOuterObjects(Outers);
	Session->Owner = Outers.Num() == 1 ? Outers[0] : nullptr;

	Session->PullFromReal();
	return Session;
}

TSharedRef<FPCGExChoicesEditSession> FPCGExChoicesEditSession::MakeForPatch(const TSharedRef<IPropertyHandle>& EntryHandle, const FPCGExPropertyHostRef& Host)
{
	const TSharedRef<FPCGExChoicesEditSession> Session = MakeShareable(new FPCGExChoicesEditSession());
	Session->bPatch = true;
	Session->TargetHandle = EntryHandle;
	Session->Host = Host;
	Session->CollectionHandle = PCGExPropertyChoicesCustomization::FindAncestor(EntryHandle, FPCGExPropertySchemaCollection::StaticStruct());

	Session->Scratch = MakeShared<FStructOnScope>(FPCGExPropertyChoicesPatchEditTarget::StaticStruct());
	Session->ScratchMember = &reinterpret_cast<FPCGExPropertyChoicesPatchEditTarget*>(Session->Scratch->GetStructMemory())->Patch;

	TArray<UObject*> Outers;
	EntryHandle->GetOuterObjects(Outers);
	Session->Owner = Outers.Num() == 1 ? Outers[0] : nullptr;

	Session->PullFromReal();
	return Session;
}

void* FPCGExChoicesEditSession::GetTargetRaw() const
{
	return FPCGExPropertyHostRef::GetSingleRawData(TargetHandle);
}

FPCGExPropertySchemaCollection* FPCGExChoicesEditSession::ResolveCollection() const
{
	return static_cast<FPCGExPropertySchemaCollection*>(FPCGExPropertyHostRef::GetSingleRawData(CollectionHandle));
}

bool FPCGExChoicesEditSession::IsValid() const
{
	const void* Raw = GetTargetRaw();
	const FStructView HostView = Host.Resolve();
	const FPCGExProperty* Property = HostView.GetPtr<FPCGExProperty>();
	if (!Raw || !Property)
	{
		return false;
	}
	// Both handles have to agree on what they reach: one left behind by a rebuild fails here.
	return bPatch
		? static_cast<const FPCGExPropertyOverrideEntry*>(Raw)->Value.GetMemory() == HostView.GetMemory()
		: Raw == &Property->Choices;
}

FStructView FPCGExChoicesEditSession::ResolveHost() const
{
	return IsValid() ? Host.Resolve() : FStructView();
}

FPCGExPropertyChoices* FPCGExChoicesEditSession::GetScratchChoices() const
{
	return bPatch ? nullptr : static_cast<FPCGExPropertyChoices*>(ScratchMember);
}

FPCGExPropertyChoicesPatch* FPCGExChoicesEditSession::GetScratchPatch() const
{
	return bPatch ? static_cast<FPCGExPropertyChoicesPatch*>(ScratchMember) : nullptr;
}

FName FPCGExChoicesEditSession::GetPropertyName() const
{
	const FPCGExProperty* Property = ResolveHost().GetPtr<FPCGExProperty>();
	return Property ? Property->PropertyName : NAME_None;
}

bool FPCGExChoicesEditSession::GetInheritedChoices(FPCGExPropertyChoices& Out) const
{
	Out = FPCGExPropertyChoices();
	const FPCGExPropertySchemaCollection* Collection = bPatch && IsValid() ? ResolveCollection() : nullptr;
	if (!Collection)
	{
		return false;
	}

	IPCGExPropertyRowHost::FLayerChain Chain;
	PCGExPropertyChoicesCustomization::GetFallbackChain(Owner.Get(), Collection, Chain);

	// Without this collection's own layer: its patch is the very thing being edited.
	TArray<FPCGExPropertyResolved> Resolved;
	Collection->Resolve(Resolved, MakeArrayView(Chain), /*bIncludeOwnOverrides=*/false);
	const FPCGExPropertyResolved* Imported = PCGExPropertyChoicesCustomization::FindImported(Resolved, GetPropertyName());
	if (!Imported)
	{
		return false;
	}
	Imported->GetEffectiveChoices(Out);
	return true;
}

const UPCGExPropertySchemaAsset* FPCGExChoicesEditSession::GetDeclaringAsset() const
{
	if (!bPatch)
	{
		return Cast<UPCGExPropertySchemaAsset>(Owner.Get());
	}
	const FPCGExPropertySchemaCollection* Collection = IsValid() ? ResolveCollection() : nullptr;
	if (!Collection)
	{
		return nullptr;
	}
	TArray<FPCGExPropertyResolved> Resolved;
	Collection->Resolve(Resolved);
	const FPCGExPropertyResolved* Imported = PCGExPropertyChoicesCustomization::FindImported(Resolved, GetPropertyName());
	return Imported ? Imported->OwningAsset : nullptr;
}

bool FPCGExChoicesEditSession::ScratchEqualsReal() const
{
	const void* Raw = GetTargetRaw();
	if (!Raw)
	{
		return true;
	}
	if (!bPatch)
	{
		return *GetScratchChoices() == *static_cast<const FPCGExPropertyChoices*>(Raw);
	}
	const FPCGExPropertyChoicesPatch* Stored = static_cast<const FPCGExPropertyOverrideEntry*>(Raw)->GetChoicesPatch();
	return Stored ? *GetScratchPatch() == *Stored : GetScratchPatch()->IsEmpty();
}

void FPCGExChoicesEditSession::WriteReal(void* Raw) const
{
	if (!bPatch)
	{
		PCGExProperties::CommitDeclaredChoices(Host.Resolve(), *GetScratchChoices());
		return;
	}

	PCGExProperties::CommitChoicesPatch(*static_cast<FPCGExPropertyOverrideEntry*>(Raw), *GetScratchPatch());

	// Before the bracket's post-change notify: the host syncs its consumer rows there, and they resolve through
	// the import rows this patch just changed the list of.
	if (FPCGExPropertySchemaCollection* Collection = ResolveCollection())
	{
		IPCGExPropertyRowHost::FLayerChain Chain;
		PCGExPropertyChoicesCustomization::GetFallbackChain(Owner.Get(), Collection, Chain);
		Collection->SyncImportOverridesInPlace(MakeArrayView(Chain));
	}
}

bool FPCGExChoicesEditSession::Commit(const FText& Description)
{
	if (bCommitting || !IsValid())
	{
		return false;
	}
	// The engine raises finished-changing for edits that changed nothing: writing those would dirty the host and
	// run its whole post-edit path.
	if (ScratchEqualsReal())
	{
		return false;
	}

	TGuardValue<bool> Guard(bCommitting, true);
	// The host's notifies can close the popover, which owns this.
	const TSharedRef<FPCGExChoicesEditSession> KeepAlive = AsShared();

	FPCGExRawPropertyEdit Edit(TargetHandle.ToSharedRef());
	Edit.Commit(Description, [this](void* Raw) { WriteReal(Raw); });

	// The commit settles ids and carriers, and the host may have its own say: what the view shows has to be that.
	if (PullFromReal())
	{
		OnScratchChanged.Broadcast();
	}
	return true;
}

void FPCGExChoicesEditSession::CommitAction(const FText& Description)
{
	Commit(Description);
	OnScratchChanged.Broadcast();
}

bool FPCGExChoicesEditSession::PullFromReal()
{
	const void* Raw = IsValid() ? GetTargetRaw() : nullptr;
	if (!Raw || ScratchEqualsReal())
	{
		return false;
	}
	if (!bPatch)
	{
		*GetScratchChoices() = *static_cast<const FPCGExPropertyChoices*>(Raw);
		return true;
	}
	const FPCGExPropertyChoicesPatch* Stored = static_cast<const FPCGExPropertyOverrideEntry*>(Raw)->GetChoicesPatch();
	*GetScratchPatch() = Stored ? *Stored : FPCGExPropertyChoicesPatch();
	return true;
}

#pragma endregion

#pragma region Customizations

namespace PCGExPropertyChoicesCustomization
{
	/** What the three session-bound customizations share: the session, the rebuild, the actions row, the element rows. */
	class FBase : public IPropertyTypeCustomization
	{
	public:
		explicit FBase(const TSharedRef<FPCGExChoicesEditSession>& InSession)
			: WeakSession(InSession)
		{
		}

	protected:
		/** The session while it can still reach the real data; null afterwards, and every callback then does nothing. */
		TSharedPtr<FPCGExChoicesEditSession> GetSession() const
		{
			const TSharedPtr<FPCGExChoicesEditSession> Session = WeakSession.Pin();
			return Session.IsValid() && Session->IsValid() ? Session : nullptr;
		}

		void RequestRebuild()
		{
			if (const TSharedPtr<IPropertyUtilities> Utils = WeakPropertyUtilities.Pin())
			{
				Utils->RequestForceRefresh();
			}
		}

		/** Full width, right-aligned: [Add Current Value] [Align v]. The header keeps the state and stays readable. */
		void AddActionsRow(IDetailChildrenBuilder& ChildBuilder, const FText& AddToolTip, FOnClicked OnAdd) const
		{
			ChildBuilder.AddCustomRow(LOCTEXT("ChoicesActionsFilter", "Actions"))
			            .WholeRowContent()
				[
					SNew(SBox)
					.Padding(FMargin(0, 2, 0, 2))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0, 0, 4, 0)
						[
							SNew(SButton)
							.Text(LOCTEXT("AddCurrentValue", "Add Current Value"))
							.ToolTipText(AddToolTip)
							.OnClicked(OnAdd)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							MakeAlignButton(WeakSession)
						]
					]
				];
		}

		/** Elements as direct children (no "Items" level): each keeps the engine's insert / delete / duplicate buttons. */
		static void AddElementRows(IDetailChildrenBuilder& ChildBuilder, const TSharedPtr<IPropertyHandle>& ItemsHandle)
		{
			uint32 NumItems = 0;
			ItemsHandle->GetNumChildren(NumItems);
			for (uint32 i = 0; i < NumItems; ++i)
			{
				if (const TSharedPtr<IPropertyHandle> Element = ItemsHandle->GetChildHandle(i))
				{
					ChildBuilder.AddProperty(Element.ToSharedRef());
				}
			}
		}

		TWeakPtr<FPCGExChoicesEditSession> WeakSession;
		TWeakPtr<IPropertyUtilities> WeakPropertyUtilities;
	};

	/** FPCGExPropertyChoices in the popover. Header: [Locked] [N choices]; children: the actions row, then one row per choice. */
	class FChoicesCustomization final : public FBase
	{
	public:
		using FBase::FBase;

		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override
		{
			WeakPropertyUtilities = CustomizationUtils.GetPropertyUtilities();
			ItemsHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExPropertyChoices, Items));
			const TSharedPtr<IPropertyHandle> LockedHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExPropertyChoices, bLocked));
			if (!ItemsHandle.IsValid() || !LockedHandle.IsValid())
			{
				HeaderRow.Visibility(EVisibility::Collapsed);
				return;
			}

			// Rows are added per element below; the engine re-lays-out on a count change but never re-runs this
			// customization, so a shape change needs the deferred full rebuild.
			if (const TSharedPtr<IPropertyHandleArray> ItemsArray = ItemsHandle->AsArray())
			{
				ItemsArray->SetOnNumElementsChanged(FSimpleDelegate::CreateSP(this, &FChoicesCustomization::RequestRebuild));
			}

			HeaderRow
				.NameContent()
				[
					PropertyHandle->CreatePropertyNameWidget()
				]
				.ValueContent()
				.MinDesiredWidth(FPCGExInlineWidgetRegistry::WideValueMinWidth)
				.MaxDesiredWidth(FPCGExInlineWidgetRegistry::WideValueMaxWidth)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0, 0, 4, 0)
					[
						LockedHandle->CreatePropertyValueWidget()
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("LockedLabel", "Locked"))
						.ToolTipText(LockedHandle->GetToolTipText())
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(12, 0, 0, 0)
					[
						SNew(STextBlock)
						.Text(this, &FChoicesCustomization::GetCountText)
						.Font(IDetailLayoutBuilder::GetDetailFontItalic())
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				];
		}

		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override
		{
			if (!ItemsHandle.IsValid())
			{
				return;
			}
			AddActionsRow(
				ChildBuilder,
				LOCTEXT("AddCurrentValueToolTip", "Add the property's current value as a new choice."),
				FOnClicked::CreateSP(this, &FChoicesCustomization::OnAddCurrentValue));
			AddElementRows(ChildBuilder, ItemsHandle);
		}

	private:
		FText GetCountText() const
		{
			uint32 NumItems = 0;
			if (ItemsHandle.IsValid())
			{
				ItemsHandle->GetNumChildren(NumItems);
			}
			return NumItems == 1
				? LOCTEXT("OneChoice", "1 choice")
				: FText::Format(LOCTEXT("ChoiceCount", "{0} choices"), FText::AsNumber(static_cast<int32>(NumItems)));
		}

		FReply OnAddCurrentValue()
		{
			const TSharedPtr<FPCGExChoicesEditSession> Session = GetSession();
			FPCGExPropertyChoices* Choices = Session.IsValid() ? Session->GetScratchChoices() : nullptr;
			if (Choices)
			{
				Choices->Items.Add(PCGExProperties::MakeChoiceFromValue(Session->ResolveHost(), MakeUniqueLabel(Choices->Items)));
				Session->CommitAction(LOCTEXT("AddChoice", "Add Choice"));
			}
			return FReply::Handled();
		}

		TSharedPtr<IPropertyHandle> ItemsHandle;
	};

	/**
	 * One choice as one row: the Label editor as the name, the carrier's own Value editor as the value (Compact
	 * factory, else the default widget). A carrier of a complex type keeps a preview in the header and expands to its
	 * editor. A carrier the host type no longer accepts shows why instead of an editor.
	 */
	class FChoiceCustomization final : public FBase
	{
	public:
		using FBase::FBase;

		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override
		{
			const TSharedPtr<IPropertyHandle> LabelHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExPropertyChoice, Label));
			CarrierHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExPropertyChoice, Value));
			if (!LabelHandle.IsValid() || !CarrierHandle.IsValid())
			{
				HeaderRow.NameContent()[PropertyHandle->CreatePropertyNameWidget()];
				return;
			}

			const TSharedPtr<FPCGExChoicesEditSession> Session = GetSession();
			const FStructView Host = Session.IsValid() ? Session->ResolveHost() : FStructView();
			const FPCGExPropertyChoice* Choice = static_cast<const FPCGExPropertyChoice*>(FPCGExPropertyHostRef::GetSingleRawData(PropertyHandle));
			const UScriptStruct* CarrierType = Choice ? Choice->Value.GetScriptStruct() : nullptr;
			const bool bCompatible = CarrierType && Host.IsValid() && PCGExProperties::IsChoiceCompatible(Host, *Choice);

			// An empty carrier only lives until the commit that follows its insertion gives it one.
			TSharedRef<SWidget> ValueWidget = SNew(STextBlock)
				.Text(CarrierType
					      ? FText::Format(LOCTEXT("IncompatibleCarrier", "No longer matches the property ({0})"), CarrierType->GetDisplayNameText())
					      : LOCTEXT("EmptyCarrier", "Empty choice"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FStyleColors::Warning);

			if (bCompatible)
			{
				// The carrier's fields as real child nodes: edits route through the engine's notify chain like any other row.
				CarrierProvider = MakeShared<FInstancedStructProvider>(CarrierHandle);
				CarrierHandle->RemoveChildren();
				const FProperty* ValueProperty = PCGExProperties::FindValueProperty(CarrierType);
				for (const TSharedPtr<IPropertyHandle>& Field : CarrierHandle->AddChildStructure(CarrierProvider.ToSharedRef()))
				{
					if (Field.IsValid() && Field->GetProperty() == ValueProperty)
					{
						CarrierValueHandle = Field;
						break;
					}
				}

				bInlineCarrier = CarrierType->HasMetaData(TEXT("PCGExInlineValue"));
				if (CarrierValueHandle.IsValid())
				{
					if (bInlineCarrier)
					{
						const FPCGExMakeInlineWidgetFn* Factory = FPCGExInlineWidgetRegistry::Find(CarrierType->GetFName(), EPCGExInlineWidgetMode::Compact);
						ValueWidget = Factory ? (*Factory)(CarrierValueHandle.ToSharedRef()) : CarrierValueHandle->CreatePropertyValueWidget();
					}
					else
					{
						// Complex carriers expand to their editor; the header shows a live preview.
						const TWeakPtr<IPropertyHandle> WeakCarrier = CarrierHandle;
						ValueWidget = SNew(STextBlock)
							.Font(IDetailLayoutBuilder::GetDetailFontItalic())
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							.Text_Lambda([WeakCarrier]() -> FText
							{
								const FInstancedStruct* Carrier = static_cast<const FInstancedStruct*>(FPCGExPropertyHostRef::GetSingleRawData(WeakCarrier.Pin()));
								return Carrier ? PCGExProperties::GetValuePreviewText(*Carrier) : FText::GetEmpty();
							});
					}
				}
			}

			HeaderRow
				.NameContent()
				[
					LabelHandle->CreatePropertyValueWidget()
				]
				.ValueContent()
				.MinDesiredWidth(FPCGExInlineWidgetRegistry::WideValueMinWidth)
				.MaxDesiredWidth(FPCGExInlineWidgetRegistry::WideValueMaxWidth)
				[
					ValueWidget
				];
		}

		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override
		{
			if (!bInlineCarrier && CarrierValueHandle.IsValid())
			{
				ChildBuilder.AddProperty(CarrierValueHandle.ToSharedRef());
			}
		}

	private:
		/** FPCGExPropertyChoice::Value, the carrier FInstancedStruct. */
		TSharedPtr<IPropertyHandle> CarrierHandle;
		/** Keeps the carrier's child-structure nodes alive for the row's lifetime. */
		TSharedPtr<FInstancedStructProvider> CarrierProvider;
		/** The carrier's own "Value" field. */
		TSharedPtr<IPropertyHandle> CarrierValueHandle;
		bool bInlineCarrier = false;
	};

	/**
	 * FPCGExPropertyChoicesPatch in the popover: this importer's rewrite of the choices it inherits. Header: [Mode]
	 * [Lock] [N effective]. Children: the actions row, the inherited choices (read-only, with Override / Remove /
	 * Restore; not shown under Replace), then this patch's own items (none under Inherit). An action on an inherited
	 * choice turns Inherit into Modify within the same commit.
	 */
	class FPatchCustomization final : public FBase
	{
	public:
		using FBase::FBase;

		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override
		{
			WeakPropertyUtilities = CustomizationUtils.GetPropertyUtilities();
			const TSharedPtr<IPropertyHandle> ModeHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExPropertyChoicesPatch, Mode));
			const TSharedPtr<IPropertyHandle> LockHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExPropertyChoicesPatch, Lock));
			ItemsHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExPropertyChoicesPatch, Items));
			if (!ModeHandle.IsValid() || !LockHandle.IsValid() || !ItemsHandle.IsValid())
			{
				HeaderRow.Visibility(EVisibility::Collapsed);
				return;
			}

			// Mode decides which rows exist, and so does the number of items.
			ModeHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FPatchCustomization::RequestRebuild));
			if (const TSharedPtr<IPropertyHandleArray> ItemsArray = ItemsHandle->AsArray())
			{
				ItemsArray->SetOnNumElementsChanged(FSimpleDelegate::CreateSP(this, &FPatchCustomization::RequestRebuild));
			}

			HeaderRow
				.NameContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PatchLabel", "Choices"))
					.ToolTipText(PropertyHandle->GetToolTipText())
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				.ValueContent()
				.MinDesiredWidth(FPCGExInlineWidgetRegistry::WideValueMinWidth)
				.MaxDesiredWidth(FPCGExInlineWidgetRegistry::WideValueMaxWidth)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						ModeHandle->CreatePropertyValueWidget()
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(8, 0, 0, 0)
					[
						LockHandle->CreatePropertyValueWidget()
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(12, 0, 0, 0)
					[
						SNew(STextBlock)
						.Text(this, &FPatchCustomization::GetCountText)
						.Font(IDetailLayoutBuilder::GetDetailFontItalic())
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				];
		}

		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override
		{
			const TSharedPtr<FPCGExChoicesEditSession> Session = GetSession();
			const FPCGExPropertyChoicesPatch* Patch = Session.IsValid() ? Session->GetScratchPatch() : nullptr;
			if (!Patch || !ItemsHandle.IsValid())
			{
				return;
			}

			AddActionsRow(
				ChildBuilder,
				LOCTEXT("PatchAddCurrentValueToolTip", "Add this row's current value as a new choice of this importer."),
				FOnClicked::CreateSP(this, &FPatchCustomization::OnAddCurrentValue));

			// Inherited choices, except those this patch already overrides (their own row stands for them).
			if (Patch->Mode != EPCGExChoicesPatchMode::Replace)
			{
				FPCGExPropertyChoices Inherited;
				Session->GetInheritedChoices(Inherited);
				for (const FPCGExPropertyChoice& Choice : Inherited.Items)
				{
					const FGuid Id = Choice.Id;
					if (!Patch->Items.ContainsByPredicate([&Id](const FPCGExPropertyChoice& Item) { return Item.Id == Id; }))
					{
						AddInheritedRow(ChildBuilder, Choice, Patch->RemovedIds.Contains(Id));
					}
				}
			}

			// This importer's own items: a stored patch holds none under Inherit.
			if (Patch->Mode != EPCGExChoicesPatchMode::Inherit)
			{
				AddElementRows(ChildBuilder, ItemsHandle);
			}
		}

	private:
		void AddInheritedRow(IDetailChildrenBuilder& ChildBuilder, const FPCGExPropertyChoice& Choice, const bool bRemoved)
		{
			const FGuid Id = Choice.Id;
			const FText Label = FPCGExChoiceRowState::LabelOf(Choice);

			const TSharedRef<SHorizontalBox> Actions = SNew(SHorizontalBox);
			if (bRemoved)
			{
				Actions->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("RestoreChoice", "Restore"))
					.ToolTipText(LOCTEXT("RestoreChoiceToolTip", "Stop removing this inherited choice."))
					.OnClicked(this, &FPatchCustomization::OnRestoreInherited, Id)
				];
			}
			else
			{
				Actions->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("OverrideChoice", "Override"))
					.ToolTipText(LOCTEXT("OverrideChoiceToolTip", "Edit this inherited choice here: label and value, same identity."))
					.OnClicked(this, &FPatchCustomization::OnOverrideInherited, Id)
				];
				Actions->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("RemoveChoice", "Remove"))
					.ToolTipText(LOCTEXT("RemoveChoiceToolTip", "Drop this inherited choice for this importer and everything below it."))
					.OnClicked(this, &FPatchCustomization::OnRemoveInherited, Id)
				];
			}

			ChildBuilder.AddCustomRow(Label)
			            .NameContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(Label)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.ColorAndOpacity(bRemoved ? FSlateColor::UseSubduedForeground() : FSlateColor::UseForeground())
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(6, 0, 0, 0)
					[
						SNew(STextBlock)
						.Text(bRemoved ? LOCTEXT("RemovedTag", "removed") : LOCTEXT("InheritedTag", "inherited"))
						.Font(IDetailLayoutBuilder::GetDetailFontItalic())
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]
				.ValueContent()
				.MinDesiredWidth(FPCGExInlineWidgetRegistry::WideValueMinWidth)
				.MaxDesiredWidth(FPCGExInlineWidgetRegistry::WideValueMaxWidth)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(PCGExProperties::GetValuePreviewText(FConstStructView(Choice.Value)))
						.Font(IDetailLayoutBuilder::GetDetailFontItalic())
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						Actions
					]
				];
		}

		FText GetCountText() const
		{
			// The row's own mirror: the effective list, as every consumer of this importer sees it.
			const TSharedPtr<FPCGExChoicesEditSession> Session = GetSession();
			const FPCGExProperty* Property = Session.IsValid() ? Session->ResolveHost().GetPtr<FPCGExProperty>() : nullptr;
			const int32 Num = Property ? Property->Choices.Items.Num() : 0;
			return Num == 1
				? LOCTEXT("OneEffectiveChoice", "1 effective")
				: FText::Format(LOCTEXT("EffectiveChoiceCount", "{0} effective"), FText::AsNumber(Num));
		}

		/** One edit of the scratch patch, committed once. Inherit rewrites nothing, so any edit of the list leaves it. */
		template <typename TEdit>
		FReply EditPatch(const FText& Description, TEdit&& Edit)
		{
			const TSharedPtr<FPCGExChoicesEditSession> Session = GetSession();
			if (FPCGExPropertyChoicesPatch* Patch = Session.IsValid() ? Session->GetScratchPatch() : nullptr)
			{
				if (Edit(*Session, *Patch))
				{
					if (Patch->Mode == EPCGExChoicesPatchMode::Inherit)
					{
						Patch->Mode = EPCGExChoicesPatchMode::Modify;
					}
					Session->CommitAction(Description);
				}
			}
			return FReply::Handled();
		}

		FReply OnAddCurrentValue()
		{
			return EditPatch(LOCTEXT("PatchAddChoice", "Add Choice"), [](const FPCGExChoicesEditSession& Session, FPCGExPropertyChoicesPatch& Patch)
			{
				FPCGExPropertyChoices Inherited;
				Session.GetInheritedChoices(Inherited);
				Patch.Items.Add(PCGExProperties::MakeChoiceFromValue(Session.ResolveHost(), MakeUniqueLabel(Patch.Items, &Inherited.Items)));
				return true;
			});
		}

		FReply OnOverrideInherited(const FGuid ChoiceId)
		{
			return EditPatch(LOCTEXT("PatchOverrideChoice", "Override Choice"), [&ChoiceId](const FPCGExChoicesEditSession& Session, FPCGExPropertyChoicesPatch& Patch)
			{
				FPCGExPropertyChoices Inherited;
				Session.GetInheritedChoices(Inherited);
				const int32 Index = PCGExProperties::FindChoiceById(Inherited, ChoiceId);
				if (Index == INDEX_NONE)
				{
					return false;
				}
				// Same Id: the item replaces the inherited choice rather than sitting beside it.
				Patch.Items.Add(Inherited.Items[Index]);
				return true;
			});
		}

		FReply OnRemoveInherited(const FGuid ChoiceId)
		{
			return EditPatch(LOCTEXT("PatchRemoveChoice", "Remove Choice"), [&ChoiceId](const FPCGExChoicesEditSession&, FPCGExPropertyChoicesPatch& Patch)
			{
				if (Patch.RemovedIds.Contains(ChoiceId))
				{
					return false;
				}
				Patch.RemovedIds.Add(ChoiceId);
				return true;
			});
		}

		FReply OnRestoreInherited(const FGuid ChoiceId)
		{
			return EditPatch(LOCTEXT("PatchRestoreChoice", "Restore Choice"), [&ChoiceId](const FPCGExChoicesEditSession&, FPCGExPropertyChoicesPatch& Patch)
			{
				return Patch.RemovedIds.Remove(ChoiceId) > 0;
			});
		}

		TSharedPtr<IPropertyHandle> ItemsHandle;
	};

	/** No row at all: outside the popover the choices structs are data, not something to edit in place. */
	class FHiddenCustomization final : public IPropertyTypeCustomization
	{
	public:
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override
		{
			HeaderRow.Visibility(EVisibility::Collapsed);
		}

		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override
		{
		}
	};
}

#pragma endregion

#pragma region PCGExPropertyChoicesUI

bool PCGExPropertyChoicesUI::IsUnderReadOnlySchema(const TSharedRef<IPropertyHandle>& Handle)
{
	// HasMetaData covers both compile-time meta=(ReadOnlySchema) and the per-instance flag set through
	// SetInstanceMetaData, which is how a values-only host locks its schema.
	for (TSharedPtr<IPropertyHandle> Parent = Handle->GetParentHandle(); Parent.IsValid(); Parent = Parent->GetParentHandle())
	{
		if (Parent->HasMetaData(TEXT("ReadOnlySchema")))
		{
			return true;
		}
	}
	return false;
}

FOnGetContent PCGExPropertyChoicesUI::MakeDeclarationEditor(const TSharedPtr<IPropertyHandle>& ChoicesHandle, const FPCGExPropertyHostRef& Host)
{
	if (!ChoicesHandle.IsValid() || !Host.IsSet())
	{
		return FOnGetContent();
	}
	return FOnGetContent::CreateLambda([ChoicesHandle, Host]() -> TSharedRef<SWidget>
	{
		return SNew(SPCGExPropertyChoicesEditor).Session(FPCGExChoicesEditSession::MakeForDeclaration(ChoicesHandle.ToSharedRef(), Host));
	});
}

FOnGetContent PCGExPropertyChoicesUI::MakePatchEditor(const TSharedPtr<IPropertyHandle>& EntryHandle, const FPCGExPropertyHostRef& Host)
{
	if (!EntryHandle.IsValid() || !Host.IsSet())
	{
		return FOnGetContent();
	}
	return FOnGetContent::CreateLambda([EntryHandle, Host]() -> TSharedRef<SWidget>
	{
		return SNew(SPCGExPropertyChoicesEditor).Session(FPCGExChoicesEditSession::MakeForPatch(EntryHandle.ToSharedRef(), Host));
	});
}

void PCGExPropertyChoicesUI::RegisterSessionLayouts(IDetailsView& View, const TSharedRef<FPCGExChoicesEditSession>& Session)
{
	using namespace PCGExPropertyChoicesCustomization;

	View.RegisterInstancedCustomPropertyTypeLayout(
		FPCGExPropertyChoices::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateLambda([Session]() -> TSharedRef<IPropertyTypeCustomization> { return MakeShared<FChoicesCustomization>(Session); }));
	View.RegisterInstancedCustomPropertyTypeLayout(
		FPCGExPropertyChoice::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateLambda([Session]() -> TSharedRef<IPropertyTypeCustomization> { return MakeShared<FChoiceCustomization>(Session); }));
	View.RegisterInstancedCustomPropertyTypeLayout(
		FPCGExPropertyChoicesPatch::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateLambda([Session]() -> TSharedRef<IPropertyTypeCustomization> { return MakeShared<FPatchCustomization>(Session); }));
}

TSharedRef<IPropertyTypeCustomization> PCGExPropertyChoicesUI::MakeHiddenCustomization()
{
	return MakeShared<PCGExPropertyChoicesCustomization::FHiddenCustomization>();
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
