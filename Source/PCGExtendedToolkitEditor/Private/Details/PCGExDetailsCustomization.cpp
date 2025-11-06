// Copyright (c) Nebukam


#include "Details/PCGExDetailsCustomization.h"

#include "AssetToolsModule.h"
#include "Details/Collections/PCGExActorCollectionActions.h"
#include "Details/Actions/PCGExActorDataPackerActions.h"
#include "Details/Collections/PCGExMeshCollectionActions.h"
#include "Details/Enums/PCGExInlineEnumCustomization.h"
#include "Details/Tuple/PCGExTupleBodyCustomization.h"


namespace PCGExDetailsCustomization
{
	void RegisterDetailsCustomization(const TSharedPtr<FSlateStyleSet>& Style)
	{
		// I know this is cursed
		FSlateStyleSet& AppStyle = const_cast<FSlateStyleSet&>(static_cast<const FSlateStyleSet&>(FAppStyle::Get()));

#define PCGEX_ADD_ACTION_ICON(_NAME, _SIZE) AppStyle.Set("PCGEx.ActionIcon." # _NAME, new FSlateVectorImageBrush(Style->RootToContentDir(TEXT( "PCGEx_Editor_" #_NAME), TEXT(".svg")), _SIZE));

		const FVector2D AIS_VerySmall = FVector2D(16.0f, 16.0f);
		const FVector2D AIS_Small = FVector2D(18.0f, 18.0f);
		const FVector2D AIS_Med = FVector2D(22.0f, 22.0f);
		const FVector2D AIS_Wide = FVector2D(44.0f, 22.0f);
		const FVector2D AIS_Big = FVector2D(44.0f, 44.0f);

		PCGEX_ADD_ACTION_ICON(Constant, AIS_VerySmall)
		PCGEX_ADD_ACTION_ICON(Attribute, AIS_VerySmall)
		PCGEX_ADD_ACTION_ICON(DataAttribute, AIS_VerySmall)
		PCGEX_ADD_ACTION_ICON(Default, AIS_VerySmall)
		PCGEX_ADD_ACTION_ICON(Enabled, AIS_VerySmall)
		PCGEX_ADD_ACTION_ICON(Disabled, AIS_VerySmall)
		PCGEX_ADD_ACTION_ICON(ScaledBounds, AIS_Med)
		PCGEX_ADD_ACTION_ICON(DensityBounds, AIS_Med)
		PCGEX_ADD_ACTION_ICON(Bounds, AIS_Med)
		PCGEX_ADD_ACTION_ICON(Center, AIS_Med)
		PCGEX_ADD_ACTION_ICON(X, AIS_Small)
		PCGEX_ADD_ACTION_ICON(Y, AIS_Small)
		PCGEX_ADD_ACTION_ICON(Z, AIS_Small)
		PCGEX_ADD_ACTION_ICON(Dist_Center, AIS_Small)
		PCGEX_ADD_ACTION_ICON(Dist_SphereBounds, AIS_Small)
		PCGEX_ADD_ACTION_ICON(Dist_BoxBounds, AIS_Small)
		PCGEX_ADD_ACTION_ICON(All, AIS_Small)
		PCGEX_ADD_ACTION_ICON(Include, AIS_Small)
		PCGEX_ADD_ACTION_ICON(Exclude, AIS_Small)
		PCGEX_ADD_ACTION_ICON(Vtx, AIS_Med)
		PCGEX_ADD_ACTION_ICON(Edges, AIS_Med)

		PCGEX_ADD_ACTION_ICON(STF_None, AIS_Med)
		PCGEX_ADD_ACTION_ICON(STF_Uniform, AIS_Wide)
		PCGEX_ADD_ACTION_ICON(STF_Individual, AIS_Wide)

		PCGEX_ADD_ACTION_ICON(MissingData_Error, AIS_VerySmall)
		PCGEX_ADD_ACTION_ICON(MissingData_Pass, AIS_VerySmall)
		PCGEX_ADD_ACTION_ICON(MissingData_Fail, AIS_VerySmall)

		PCGEX_ADD_ACTION_ICON(Fit_None, AIS_Med)
		PCGEX_ADD_ACTION_ICON(Fit_Fill, AIS_Med)
		PCGEX_ADD_ACTION_ICON(Fit_Min, AIS_Med)
		PCGEX_ADD_ACTION_ICON(Fit_Max, AIS_Med)
		PCGEX_ADD_ACTION_ICON(Fit_Average, AIS_Med)

		PCGEX_ADD_ACTION_ICON(From_Min, AIS_Med)
		PCGEX_ADD_ACTION_ICON(From_Center, AIS_Med)
		PCGEX_ADD_ACTION_ICON(From_Max, AIS_Med)
		PCGEX_ADD_ACTION_ICON(From_Pivot, AIS_Med)
		PCGEX_ADD_ACTION_ICON(From_Custom, AIS_Med)

		PCGEX_ADD_ACTION_ICON(To_Same, AIS_Med)
		PCGEX_ADD_ACTION_ICON(To_Min, AIS_Med)
		PCGEX_ADD_ACTION_ICON(To_Center, AIS_Med)
		PCGEX_ADD_ACTION_ICON(To_Max, AIS_Med)
		PCGEX_ADD_ACTION_ICON(To_Pivot, AIS_Med)
		PCGEX_ADD_ACTION_ICON(To_Custom, AIS_Med)

		PCGEX_ADD_ACTION_ICON(Numeric, AIS_Wide)
		PCGEX_ADD_ACTION_ICON(Text, AIS_Wide)

		
		PCGEX_ADD_ACTION_ICON(RebuildStaging, AIS_Big)
		PCGEX_ADD_ACTION_ICON(RebuildStagingRecursive, AIS_Big)
		PCGEX_ADD_ACTION_ICON(RebuildStagingProject, AIS_Big)
		
		PCGEX_ADD_ACTION_ICON(AddContentBrowserSelection, AIS_Med)
		PCGEX_ADD_ACTION_ICON(NormalizeWeight, AIS_Med)
		
		PCGEX_ADD_ACTION_ICON(Entries, AIS_Med)
		PCGEX_ADD_ACTION_ICON(Settings, AIS_Med)

		FButtonStyle ActionIconButton = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton");

		FSlateBrush Brush = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton").Pressed;
		Brush.Margin = FMargin(2, 2);

		Brush.TintColor = FLinearColor(0.1, 0.1, 0.1, 0.5f);
		ActionIconButton.SetNormal(Brush);

		Brush.TintColor = FLinearColor(0.1, 0.1, 0.1, 0.5f);
		ActionIconButton.SetHovered(Brush);

		Brush.TintColor = FLinearColor(0.1, 0.1, 0.1, 0.8f);
		ActionIconButton.SetPressed(Brush);

		AppStyle.Set("PCGEx.ActionIcon", ActionIconButton);

#undef PCGEX_ADD_ACTION_ICON
#undef PCGEX_ADD_ACTION_ICON_WIDE

		/////////

		FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(MakeShared<FPCGExMeshCollectionActions>());
		FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(MakeShared<FPCGExActorCollectionActions>());
		FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(MakeShared<FPCGExActorDataPackerActions>());
				
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyModule.RegisterCustomPropertyTypeLayout(
			"PCGExTupleBody",
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGExTupleBodyCustomization::MakeInstance));

#define PCGEX_FOREACH_INLINE_ENUM(MACRO)\
MACRO(EPCGExInputValueType)\
MACRO(EPCGExApplySampledComponentFlags)\
MACRO(EPCGExOptionState)\
MACRO(EPCGExFilterFallback)\
MACRO(EPCGExFilterNoDataFallback)\
MACRO(EPCGExPointBoundsSource)\
MACRO(EPCGExDistance)\
MACRO(EPCGExClusterElement)\
MACRO(EPCGExAttributeFilter)\
MACRO(EPCGExComparisonDataType)\
MACRO(EPCGExScaleToFit)\
MACRO(EPCGExJustifyFrom)\
MACRO(EPCGExJustifyTo)\
MACRO(EPCGExFitMode)

#define PCGEX_DECL_INLINE_ENUM(_ENUM)\
class FPCGExInline##_ENUM final : public FPCGExInlineEnumCustomization{\
public:	explicit FPCGExInline##_ENUM(const FString& InEnumName) : FPCGExInlineEnumCustomization(InEnumName){}\
static TSharedRef<IPropertyTypeCustomization> MakeInstance(){ return MakeShareable(new FPCGExInline##_ENUM(#_ENUM));}};\
PropertyModule.RegisterCustomPropertyTypeLayout(#_ENUM,FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGExInline##_ENUM::MakeInstance));

		// This is grotesque but it works （。＾▽＾）
		PCGEX_FOREACH_INLINE_ENUM(PCGEX_DECL_INLINE_ENUM)

#undef PCGEX_DECL_INLINE_ENUM
#undef PCGEX_FOREACH_INLINE_ENUM
	}
}
