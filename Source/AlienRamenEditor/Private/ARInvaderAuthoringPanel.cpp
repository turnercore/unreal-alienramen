#include "ARInvaderAuthoringPanel.h"

#include "ARInvaderAuthoringEditorProxies.h"
#include "ARInvaderAuthoringEditorSettings.h"
#include "ARInvaderToolingSettings.h"
#include "AREnemyBase.h"
#include "ARInvaderDirectorSettings.h"
#include "ARLog.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Editor/UnrealEdEngine.h"
#include "FileHelpers.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PlayInEditorDataTypes.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Styling/AppStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/SoftObjectPath.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Misc/PackageName.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	static constexpr float LayerMatchTolerance = 0.0001f;
	static constexpr float PIEBootstrapPollInterval = 0.2f;
	static constexpr float PIEBootstrapFallbackDelaySeconds = 0.75f;
	static constexpr float PIEBootstrapTimeoutSeconds = 20.0f;

	class FInvaderSpawnDragDropOp : public FDecoratedDragDropOp
	{
	public:
		DRAG_DROP_OPERATOR_TYPE(FInvaderSpawnDragDropOp, FDecoratedDragDropOp)

		int32 SourceSpawnIndex = INDEX_NONE;

		static TSharedRef<FInvaderSpawnDragDropOp> New(int32 InSourceSpawnIndex, const FString& Label)
		{
			TSharedRef<FInvaderSpawnDragDropOp> Op = MakeShared<FInvaderSpawnDragDropOp>();
			Op->SourceSpawnIndex = InSourceSpawnIndex;
			Op->DefaultHoverText = FText::FromString(Label);
			Op->CurrentHoverText = Op->DefaultHoverText;
			Op->Construct();
			return Op;
		}
	};

	static FLinearColor GetEnemyColor(EAREnemyColor Color)
	{
		switch (Color)
		{
		case EAREnemyColor::Red:
			return FLinearColor(0.95f, 0.2f, 0.2f, 1.f);
		case EAREnemyColor::Blue:
			return FLinearColor(0.2f, 0.35f, 0.95f, 1.f);
		case EAREnemyColor::White:
		default:
			return FLinearColor(0.9f, 0.9f, 0.9f, 1.f);
		}
	}

	static FString EnemyColorToName(EAREnemyColor Color)
	{
		switch (Color)
		{
		case EAREnemyColor::Red:
			return TEXT("Red");
		case EAREnemyColor::Blue:
			return TEXT("Blue");
		case EAREnemyColor::White:
		default:
			return TEXT("White");
		}
	}

	enum class EPaletteShape : uint8
	{
		Square = 0,
		Circle,
		Triangle,
		Diamond,
		Count,
		Star
	};

	static EPaletteShape NormalizePaletteShape(int32 ShapeCycle)
	{
		const int32 Count = static_cast<int32>(EPaletteShape::Count);
		const int32 Normalized = ((ShapeCycle % Count) + Count) % Count;
		return static_cast<EPaletteShape>(Normalized);
	}

	static FString PaletteShapeShortName(int32 ShapeCycle)
	{
		switch (NormalizePaletteShape(ShapeCycle))
		{
		case EPaletteShape::Circle:
			return TEXT("CI");
		case EPaletteShape::Triangle:
			return TEXT("TR");
		case EPaletteShape::Diamond:
			return TEXT("DI");
		case EPaletteShape::Square:
		default:
			return TEXT("SQ");
		}
	}

	static FString PaletteShapeGlyph(int32 ShapeCycle)
	{
		switch (NormalizePaletteShape(ShapeCycle))
		{
		case EPaletteShape::Circle:
			return TEXT("●");
		case EPaletteShape::Triangle:
			return TEXT("▲");
		case EPaletteShape::Diamond:
			return TEXT("◆");
		case EPaletteShape::Star:
			return TEXT("★");
		case EPaletteShape::Square:
		default:
			return TEXT("■");
		}
	}

	static int32 GetPaletteShapeCycleForClass(const UClass* EnemyClass)
	{
		if (!EnemyClass)
		{
			return 0;
		}

		const UARInvaderAuthoringEditorSettings* Settings = GetDefault<UARInvaderAuthoringEditorSettings>();
		if (!Settings)
		{
			return 0;
		}

		const FSoftClassPath ClassPath(EnemyClass);
		if (const int32* SavedShape = Settings->EnemyClassShapeCycles.Find(ClassPath))
		{
			return *SavedShape;
		}
		return 0;
	}

	static int32 FindClosestSpawn(
		const FARWaveDefRow* WaveRow,
		const FVector2D& LocalPoint,
		const FVector2D& Size,
		const FVector2D& BoundsMin,
		const FVector2D& BoundsMax,
		float SelectedLayerDelay,
		bool bHideOtherLayers)
	{
		if (!WaveRow)
		{
			return INDEX_NONE;
		}

		auto OffsetToLocal = [&Size, &BoundsMin, &BoundsMax](const FVector2D& Offset)
		{
			const float VerticalRange = FMath::Max(1.f, BoundsMax.X - BoundsMin.X);
			const float HorizontalRange = FMath::Max(1.f, BoundsMax.Y - BoundsMin.Y);
			const float HorizontalAlpha = (Offset.Y - BoundsMin.Y) / HorizontalRange;
			const float VerticalAlpha = (Offset.X - BoundsMin.X) / VerticalRange;
			return FVector2D(HorizontalAlpha * Size.X, Size.Y - (VerticalAlpha * Size.Y));
		};

		int32 BestIndex = INDEX_NONE;
		float BestDistSq = TNumericLimits<float>::Max();
		for (int32 i = 0; i < WaveRow->EnemySpawns.Num(); ++i)
		{
			const FARWaveEnemySpawnDef& Spawn = WaveRow->EnemySpawns[i];
			if (bHideOtherLayers && !FMath::IsNearlyEqual(Spawn.SpawnDelay, SelectedLayerDelay, LayerMatchTolerance))
			{
				continue;
			}

			const FVector2D Pt = OffsetToLocal(Spawn.AuthoredScreenOffset);
			const float DistSq = FVector2D::DistSquared(LocalPoint, Pt);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				BestIndex = i;
			}
		}

		return BestDistSq <= FMath::Square(14.f) ? BestIndex : INDEX_NONE;
	}

	static bool IsPaletteClassSupported(const UClass* Class)
	{
		if (!Class)
		{
			return false;
		}

		if (!Class->IsChildOf(AAREnemyBase::StaticClass()) || Class == AAREnemyBase::StaticClass())
		{
			return false;
		}

		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			return false;
		}

		const FString Name = Class->GetName();
		if (Name.StartsWith(TEXT("SKEL_")) || Name.StartsWith(TEXT("REINST_")))
		{
			return false;
		}

		return true;
	}

	static FString MakePaletteClassDisplayName(const FSoftClassPath& ClassPath)
	{
		FString Name = FPackageName::ObjectPathToObjectName(ClassPath.ToString());

		if (Name.RemoveFromEnd(TEXT("_C")))
		{
			// already stripped class suffix
		}

		if (Name.StartsWith(TEXT("SKEL_")))
		{
			Name.RightChopInline(5, EAllowShrinking::No);
		}
		if (Name.StartsWith(TEXT("REINST_")))
		{
			Name.RightChopInline(7, EAllowShrinking::No);
		}

		if (Name.StartsWith(TEXT("BP_EnemyBase_")))
		{
			Name.RightChopInline(13, EAllowShrinking::No);
		}
		else if (Name.StartsWith(TEXT("BP_EnermyBase_")))
		{
			Name.RightChopInline(14, EAllowShrinking::No);
		}
		else if (Name.StartsWith(TEXT("BP_")))
		{
			Name.RightChopInline(3, EAllowShrinking::No);
		}

		Name.ReplaceInline(TEXT("_"), TEXT(" "));
		Name.TrimStartAndEndInline();
		return Name.IsEmpty() ? FPackageName::ObjectPathToObjectName(ClassPath.ToString()) : Name;
	}

	static bool TryReadBoolPropertyByName(const UObject* Object, const FName PropertyName, bool& bOutValue)
	{
		if (!Object || PropertyName.IsNone())
		{
			return false;
		}

		const FBoolProperty* BoolProp = FindFProperty<FBoolProperty>(Object->GetClass(), PropertyName);
		if (!BoolProp)
		{
			return false;
		}

		bOutValue = BoolProp->GetPropertyValue_InContainer(Object);
		return true;
	}

	static bool TryCallNoArgBoolFunctionByName(UObject* Object, const FName FunctionName, bool& bOutValue)
	{
		if (!Object || FunctionName.IsNone())
		{
			return false;
		}

		UFunction* Fn = Object->FindFunction(FunctionName);
		if (!Fn)
		{
			return false;
		}

		if (Fn->NumParms != 1 || !Fn->GetReturnProperty())
		{
			return false;
		}

		const FBoolProperty* ReturnBool = CastField<FBoolProperty>(Fn->GetReturnProperty());
		if (!ReturnBool)
		{
			return false;
		}

		TArray<uint8> Buffer;
		Buffer.SetNumZeroed(Fn->ParmsSize);
		Object->ProcessEvent(Fn, Buffer.GetData());
		bOutValue = ReturnBool->GetPropertyValue(Buffer.GetData() + ReturnBool->GetOffset_ForUFunction());
		return true;
	}

	static bool IsPIESaveLoadComplete(UObject* GameInstance, bool& bOutHadSignal)
	{
		bOutHadSignal = false;

		bool bValue = false;
		static const FName BoolFunctionCandidates[] =
		{
			TEXT("IsGameLoaded"),
			TEXT("HasGameLoaded"),
			TEXT("IsSaveLoaded"),
			TEXT("HasSaveLoaded"),
			TEXT("IsLoadCompleted"),
			TEXT("HasLoadCompleted"),
		};
		for (const FName& Name : BoolFunctionCandidates)
		{
			if (TryCallNoArgBoolFunctionByName(GameInstance, Name, bValue))
			{
				bOutHadSignal = true;
				return bValue;
			}
		}

		static const FName BoolPropertyCandidates[] =
		{
			TEXT("bGameLoaded"),
			TEXT("bIsGameLoaded"),
			TEXT("bSaveLoaded"),
			TEXT("bIsSaveLoaded"),
			TEXT("bLoadCompleted"),
		};
		for (const FName& Name : BoolPropertyCandidates)
		{
			if (TryReadBoolPropertyByName(GameInstance, Name, bValue))
			{
				bOutHadSignal = true;
				return bValue;
			}
		}

		return false;
	}
}

class SInvaderWaveCanvas final : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SInvaderWaveCanvas) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&)
		{
		}

		void SetDataAccessors(
			TFunction<const FARWaveDefRow*()> InGetWaveRow,
			TFunction<float()> InGetSelectedLayerDelay,
			TFunction<bool()> InGetHideOtherLayers,
			TFunction<TSet<int32>()> InGetSelectedSpawnIndices,
			TFunction<float()> InGetPreviewTime,
			TFunction<void(int32, bool, bool)> InOnSpawnSelected,
			TFunction<void()> InOnClearSelection,
			TFunction<void(int32, const FVector2D&)> InOnOpenSpawnContextMenu,
			TFunction<void()> InOnBeginDragSpawn,
			TFunction<void()> InOnEndDragSpawn,
			TFunction<void(const FVector2D&)> InOnMoveSelectedSpawnsByDelta,
			TFunction<void(const TArray<int32>&, bool)> InOnSelectionRectChanged,
			TFunction<void(const FVector2D&)> InOnAddSpawnAt)
		{
			GetWaveRow = MoveTemp(InGetWaveRow);
			GetSelectedLayerDelay = MoveTemp(InGetSelectedLayerDelay);
			GetHideOtherLayers = MoveTemp(InGetHideOtherLayers);
			GetSelectedSpawnIndices = MoveTemp(InGetSelectedSpawnIndices);
			GetPreviewTime = MoveTemp(InGetPreviewTime);
			OnSpawnSelected = MoveTemp(InOnSpawnSelected);
			OnClearSelection = MoveTemp(InOnClearSelection);
			OnOpenSpawnContextMenu = MoveTemp(InOnOpenSpawnContextMenu);
			OnBeginDragSpawn = MoveTemp(InOnBeginDragSpawn);
			OnEndDragSpawn = MoveTemp(InOnEndDragSpawn);
			OnMoveSelectedSpawnsByDelta = MoveTemp(InOnMoveSelectedSpawnsByDelta);
			OnSelectionRectChanged = MoveTemp(InOnSelectionRectChanged);
			OnAddSpawnAt = MoveTemp(InOnAddSpawnAt);
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			return FVector2D(640.f, 400.f);
		}

		virtual int32 OnPaint(
			const FPaintArgs& Args,
			const FGeometry& AllottedGeometry,
			const FSlateRect& MyCullingRect,
			FSlateWindowElementList& OutDrawElements,
			int32 LayerId,
			const FWidgetStyle& InWidgetStyle,
			bool bParentEnabled) const override
		{
			const FVector2D Size = AllottedGeometry.GetLocalSize();
			const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
			const FVector2D BoundsMin = Settings->GameplayBoundsMin;
			const FVector2D BoundsMax = Settings->GameplayBoundsMax;

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(),
				FAppStyle::GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				FLinearColor(0.03f, 0.03f, 0.04f, 1.f));

			for (int32 Line = 1; Line < 8; ++Line)
			{
				const float X = (Size.X / 8.f) * Line;
				const float Y = (Size.Y / 8.f) * Line;
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), { FVector2D(X, 0.f), FVector2D(X, Size.Y) }, ESlateDrawEffect::None, FLinearColor(0.09f, 0.09f, 0.11f, 1.f), true, 1.f);
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), { FVector2D(0.f, Y), FVector2D(Size.X, Y) }, ESlateDrawEffect::None, FLinearColor(0.09f, 0.09f, 0.11f, 1.f), true, 1.f);
			}

			auto OffsetToLocal = [&Size, &BoundsMin, &BoundsMax](const FVector2D& Offset)
			{
				const float VerticalRange = FMath::Max(1.f, BoundsMax.X - BoundsMin.X);
				const float HorizontalRange = FMath::Max(1.f, BoundsMax.Y - BoundsMin.Y);
				const float HorizontalAlpha = (Offset.Y - BoundsMin.Y) / HorizontalRange;
				const float VerticalAlpha = (Offset.X - BoundsMin.X) / VerticalRange;
				return FVector2D(HorizontalAlpha * Size.X, Size.Y - (VerticalAlpha * Size.Y));
			};

			const FARWaveDefRow* WaveRow = GetWaveRow ? GetWaveRow() : nullptr;
			if (!WaveRow)
			{
				return LayerId + 2;
			}

			const bool bHideOtherLayers = GetHideOtherLayers ? GetHideOtherLayers() : false;
			const float SelectedLayerDelay = GetSelectedLayerDelay ? GetSelectedLayerDelay() : 0.f;
			const TSet<int32> SelectedIndices = GetSelectedSpawnIndices ? GetSelectedSpawnIndices() : TSet<int32>();
			const float InPreviewTime = GetPreviewTime ? GetPreviewTime() : 0.f;

			for (int32 Index = 0; Index < WaveRow->EnemySpawns.Num(); ++Index)
			{
				const FARWaveEnemySpawnDef& Spawn = WaveRow->EnemySpawns[Index];
				if (bHideOtherLayers && !FMath::IsNearlyEqual(Spawn.SpawnDelay, SelectedLayerDelay, LayerMatchTolerance))
				{
					continue;
				}

				const FVector2D Local = OffsetToLocal(Spawn.AuthoredScreenOffset);
				FLinearColor DotColor = GetEnemyColor(Spawn.EnemyColor);
				if (InPreviewTime < Spawn.SpawnDelay)
				{
					DotColor.A = 0.20f;
				}

				const int32 ShapeCycle = GetPaletteShapeCycleForClass(Spawn.EnemyClass);
				DrawShape(
					OutDrawElements,
					LayerId + 2,
					AllottedGeometry,
					Local,
					DotColor,
					ShapeCycle,
					12.f,
					false);

				if (SelectedIndices.Contains(Index))
				{
					DrawShape(
						OutDrawElements,
						LayerId + 3,
						AllottedGeometry,
						Local,
						FLinearColor(1.f, 1.f, 1.f, 0.3f),
						ShapeCycle,
						18.f,
						true);
				}
			}

			if (bSelectionRectActive)
			{
				const FVector2D Min(FMath::Min(SelectionRectStart.X, SelectionRectEnd.X), FMath::Min(SelectionRectStart.Y, SelectionRectEnd.Y));
				const FVector2D Max(FMath::Max(SelectionRectStart.X, SelectionRectEnd.X), FMath::Max(SelectionRectStart.Y, SelectionRectEnd.Y));
				const FVector2D RectSize = Max - Min;
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId + 4,
					AllottedGeometry.ToPaintGeometry(RectSize, FSlateLayoutTransform(Min)),
					FAppStyle::GetBrush("WhiteBrush"),
					ESlateDrawEffect::None,
					FLinearColor(0.2f, 0.6f, 1.f, 0.15f));
				FSlateDrawElement::MakeLines(
					OutDrawElements,
					LayerId + 5,
					AllottedGeometry.ToPaintGeometry(),
					{ Min, FVector2D(Max.X, Min.Y), Max, FVector2D(Min.X, Max.Y), Min },
					ESlateDrawEffect::None,
					FLinearColor(0.35f, 0.75f, 1.f, 0.9f),
					true,
					1.5f);
			}

			return LayerId + 6;
		}

		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (!GetWaveRow)
			{
				return FReply::Unhandled();
			}

			const FARWaveDefRow* WaveRow = GetWaveRow();
			if (!WaveRow)
			{
				return FReply::Unhandled();
			}

			const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			const int32 Closest = FindSpawnAtLocalPoint(WaveRow, MyGeometry, Local);

			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
			{
				if (Closest != INDEX_NONE && OnSpawnSelected)
				{
					const bool bToggle = MouseEvent.IsControlDown();
					const bool bRangeSelect = MouseEvent.IsShiftDown();
					OnSpawnSelected(Closest, bToggle, bRangeSelect);
					bStartedDragTransaction = false;
					DraggedSpawnIndex = Closest;
					DragStartOffset = LocalToOffset(Local, MyGeometry.GetLocalSize());
					bSelectionRectActive = false;
					return FReply::Handled().CaptureMouse(AsShared());
				}

				if (Closest == INDEX_NONE)
				{
					bSelectionRectActive = true;
					SelectionRectStart = Local;
					SelectionRectEnd = Local;
					bSelectionRectAppend = MouseEvent.IsControlDown() || MouseEvent.IsShiftDown();
					if (!bSelectionRectAppend && OnClearSelection)
					{
						OnClearSelection();
					}
					return FReply::Handled().CaptureMouse(AsShared());
				}
			}
			else if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
			{
				if (Closest != INDEX_NONE)
				{
					if (OnSpawnSelected)
					{
						OnSpawnSelected(Closest, false, false);
					}
					if (OnOpenSpawnContextMenu)
					{
						OnOpenSpawnContextMenu(Closest, MouseEvent.GetScreenSpacePosition());
						return FReply::Handled();
					}
				}

				if (OnAddSpawnAt)
				{
					OnAddSpawnAt(LocalToOffset(Local, MyGeometry.GetLocalSize()));
					return FReply::Handled();
				}
			}

			return FReply::Unhandled();
		}

		virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (GetWaveRow)
			{
				const FARWaveDefRow* WaveRow = GetWaveRow();
				const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
				const int32 Closest = FindSpawnAtLocalPoint(WaveRow, MyGeometry, Local);
				if (Closest != HoveredSpawnIndex)
				{
					HoveredSpawnIndex = Closest;
					if (WaveRow && WaveRow->EnemySpawns.IsValidIndex(HoveredSpawnIndex) && WaveRow->EnemySpawns[HoveredSpawnIndex].EnemyClass)
					{
						const FString HoverName = MakePaletteClassDisplayName(FSoftClassPath(WaveRow->EnemySpawns[HoveredSpawnIndex].EnemyClass));
						SetToolTipText(FText::FromString(HoverName));
					}
					else
					{
						SetToolTipText(FText::GetEmpty());
					}
				}
			}

			if (!MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) || DraggedSpawnIndex == INDEX_NONE)
			{
				if (!MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) || !bSelectionRectActive || !HasMouseCapture())
				{
					return FReply::Unhandled();
				}

				const FARWaveDefRow* WaveRow = GetWaveRow ? GetWaveRow() : nullptr;
				SelectionRectEnd = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
				if (OnSelectionRectChanged)
				{
					OnSelectionRectChanged(FindSpawnsInRect(WaveRow, MyGeometry, SelectionRectStart, SelectionRectEnd), bSelectionRectAppend);
				}
				return FReply::Handled();
			}

			if (OnMoveSelectedSpawnsByDelta)
			{
				if (!bStartedDragTransaction && OnBeginDragSpawn)
				{
					OnBeginDragSpawn();
					bStartedDragTransaction = true;
				}
				const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
				const FVector2D CurrentOffset = LocalToOffset(Local, MyGeometry.GetLocalSize());
				OnMoveSelectedSpawnsByDelta(CurrentOffset - DragStartOffset);
				return FReply::Handled();
			}

			return FReply::Unhandled();
		}

		virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
			{
				if (bStartedDragTransaction && OnEndDragSpawn)
				{
					OnEndDragSpawn();
				}
				DraggedSpawnIndex = INDEX_NONE;
				bStartedDragTransaction = false;
				bSelectionRectActive = false;
				if (HasMouseCapture())
				{
					return FReply::Handled().ReleaseMouseCapture();
				}
			}
			return FReply::Unhandled();
		}

		virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override
		{
			(void)CaptureLostEvent;
			if (bStartedDragTransaction && OnEndDragSpawn)
			{
				OnEndDragSpawn();
			}
			DraggedSpawnIndex = INDEX_NONE;
			bStartedDragTransaction = false;
			bSelectionRectActive = false;
		}

		virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
		{
			SLeafWidget::OnMouseLeave(MouseEvent);
			HoveredSpawnIndex = INDEX_NONE;
			SetToolTipText(FText::GetEmpty());
		}

	private:
		void DrawShape(
			FSlateWindowElementList& OutDrawElements,
			int32 Layer,
			const FGeometry& Geometry,
			const FVector2D& Center,
			const FLinearColor& Color,
			int32 ShapeCycle,
			float Size,
			bool bOutlineOnly) const
		{
			const EPaletteShape Shape = NormalizePaletteShape(ShapeCycle);
			const float Half = Size * 0.5f;
			switch (Shape)
			{
			case EPaletteShape::Circle:
			{
				TArray<FVector2D> CirclePoints;
				CirclePoints.Reserve(17);
				for (int32 i = 0; i <= 16; ++i)
				{
					const float Angle = (2.f * PI * static_cast<float>(i)) / 16.f;
					CirclePoints.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Half);
				}
				if (!bOutlineOnly)
				{
					const FVector2D FillSize(Half * 1.2f, Half * 1.2f);
					const FVector2D FillPos = Center - (FillSize * 0.5f);
					FSlateDrawElement::MakeBox(
						OutDrawElements,
						Layer,
						Geometry.ToPaintGeometry(FillSize, FSlateLayoutTransform(FillPos)),
						FAppStyle::GetBrush("WhiteBrush"),
						ESlateDrawEffect::None,
						Color);
				}
				FSlateDrawElement::MakeLines(
					OutDrawElements,
					Layer,
					Geometry.ToPaintGeometry(),
					CirclePoints,
					ESlateDrawEffect::None,
					Color,
					true,
					bOutlineOnly ? 2.f : 3.f);
				break;
			}
			case EPaletteShape::Triangle:
			{
				const FVector2D A = Center + FVector2D(0.f, -Half);
				const FVector2D B = Center + FVector2D(Half, Half);
				const FVector2D C = Center + FVector2D(-Half, Half);
				if (!bOutlineOnly)
				{
					const FVector2D FillSize(Half * 1.2f, Half * 1.2f);
					const FVector2D FillPos = Center - (FillSize * 0.5f);
					FSlateDrawElement::MakeBox(
						OutDrawElements,
						Layer,
						Geometry.ToPaintGeometry(FillSize, FSlateLayoutTransform(FillPos)),
						FAppStyle::GetBrush("WhiteBrush"),
						ESlateDrawEffect::None,
						Color);
				}
				FSlateDrawElement::MakeLines(
					OutDrawElements,
					Layer,
					Geometry.ToPaintGeometry(),
					{ A, B, C, A },
					ESlateDrawEffect::None,
					Color,
					true,
					bOutlineOnly ? 2.f : 3.f);
				break;
			}
			case EPaletteShape::Diamond:
			{
				const FVector2D A = Center + FVector2D(0.f, -Half);
				const FVector2D B = Center + FVector2D(Half, 0.f);
				const FVector2D C = Center + FVector2D(0.f, Half);
				const FVector2D D = Center + FVector2D(-Half, 0.f);
				if (!bOutlineOnly)
				{
					const FVector2D FillSize(Half * 1.15f, Half * 1.15f);
					const FVector2D FillPos = Center - (FillSize * 0.5f);
					FSlateDrawElement::MakeBox(
						OutDrawElements,
						Layer,
						Geometry.ToPaintGeometry(FillSize, FSlateLayoutTransform(FillPos)),
						FAppStyle::GetBrush("WhiteBrush"),
						ESlateDrawEffect::None,
						Color);
				}
				FSlateDrawElement::MakeLines(
					OutDrawElements,
					Layer,
					Geometry.ToPaintGeometry(),
					{ A, B, C, D, A },
					ESlateDrawEffect::None,
					Color,
					true,
					bOutlineOnly ? 2.f : 3.f);
				break;
			}
			case EPaletteShape::Square:
			default:
			{
				const FVector2D DotSize(Size, Size);
				const FVector2D DotPos = Center - (DotSize * 0.5f);
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					Layer,
					Geometry.ToPaintGeometry(DotSize, FSlateLayoutTransform(DotPos)),
					FAppStyle::GetBrush("WhiteBrush"),
					ESlateDrawEffect::None,
					Color);
				break;
			}
			}
		}

		int32 FindSpawnAtLocalPoint(const FARWaveDefRow* WaveRow, const FGeometry& MyGeometry, const FVector2D& Local) const
		{
			if (!WaveRow)
			{
				return INDEX_NONE;
			}

			const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
			return FindClosestSpawn(
				WaveRow,
				Local,
				MyGeometry.GetLocalSize(),
				Settings->GameplayBoundsMin,
				Settings->GameplayBoundsMax,
				GetSelectedLayerDelay ? GetSelectedLayerDelay() : 0.f,
				GetHideOtherLayers ? GetHideOtherLayers() : false);
		}

		TArray<int32> FindSpawnsInRect(const FARWaveDefRow* WaveRow, const FGeometry& MyGeometry, const FVector2D& A, const FVector2D& B) const
		{
			TArray<int32> Result;
			if (!WaveRow)
			{
				return Result;
			}

			const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
			const FVector2D BoundsMin = Settings->GameplayBoundsMin;
			const FVector2D BoundsMax = Settings->GameplayBoundsMax;
			const FVector2D Size = MyGeometry.GetLocalSize();
			const bool bHideOtherLayers = GetHideOtherLayers ? GetHideOtherLayers() : false;
			const float SelectedLayerDelay = GetSelectedLayerDelay ? GetSelectedLayerDelay() : 0.f;

			const FVector2D Min(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
			const FVector2D Max(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y));

			auto OffsetToLocal = [&Size, &BoundsMin, &BoundsMax](const FVector2D& Offset)
			{
				const float VerticalRange = FMath::Max(1.f, BoundsMax.X - BoundsMin.X);
				const float HorizontalRange = FMath::Max(1.f, BoundsMax.Y - BoundsMin.Y);
				const float HorizontalAlpha = (Offset.Y - BoundsMin.Y) / HorizontalRange;
				const float VerticalAlpha = (Offset.X - BoundsMin.X) / VerticalRange;
				return FVector2D(HorizontalAlpha * Size.X, Size.Y - (VerticalAlpha * Size.Y));
			};

			for (int32 Index = 0; Index < WaveRow->EnemySpawns.Num(); ++Index)
			{
				const FARWaveEnemySpawnDef& Spawn = WaveRow->EnemySpawns[Index];
				if (bHideOtherLayers && !FMath::IsNearlyEqual(Spawn.SpawnDelay, SelectedLayerDelay, LayerMatchTolerance))
				{
					continue;
				}

				const FVector2D Local = OffsetToLocal(Spawn.AuthoredScreenOffset);
				if (Local.X >= Min.X && Local.X <= Max.X && Local.Y >= Min.Y && Local.Y <= Max.Y)
				{
					Result.Add(Index);
				}
			}

			return Result;
		}

		FVector2D LocalToOffset(const FVector2D& Local, const FVector2D& Size) const
		{
			const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
			const FVector2D BoundsMin = Settings->GameplayBoundsMin;
			const FVector2D BoundsMax = Settings->GameplayBoundsMax;
			const float VerticalRange = FMath::Max(1.f, BoundsMax.X - BoundsMin.X);
			const float HorizontalRange = FMath::Max(1.f, BoundsMax.Y - BoundsMin.Y);
			const float HorizontalAlpha = FMath::Clamp(Local.X / FMath::Max(1.f, Size.X), 0.f, 1.f);
			const float VerticalAlpha = FMath::Clamp(1.f - (Local.Y / FMath::Max(1.f, Size.Y)), 0.f, 1.f);
			return FVector2D(
				BoundsMin.X + (VerticalRange * VerticalAlpha),
				BoundsMin.Y + (HorizontalRange * HorizontalAlpha));
		}

	private:
		TFunction<const FARWaveDefRow*()> GetWaveRow;
		TFunction<float()> GetSelectedLayerDelay;
		TFunction<bool()> GetHideOtherLayers;
		TFunction<TSet<int32>()> GetSelectedSpawnIndices;
		TFunction<float()> GetPreviewTime;
		TFunction<void(int32, bool, bool)> OnSpawnSelected;
		TFunction<void()> OnClearSelection;
		TFunction<void(int32, const FVector2D&)> OnOpenSpawnContextMenu;
		TFunction<void()> OnBeginDragSpawn;
		TFunction<void()> OnEndDragSpawn;
		TFunction<void(const FVector2D&)> OnMoveSelectedSpawnsByDelta;
		TFunction<void(const TArray<int32>&, bool)> OnSelectionRectChanged;
		TFunction<void(const FVector2D&)> OnAddSpawnAt;

		int32 DraggedSpawnIndex = INDEX_NONE;
		int32 HoveredSpawnIndex = INDEX_NONE;
		bool bStartedDragTransaction = false;
		bool bSelectionRectActive = false;
		bool bSelectionRectAppend = false;
		FVector2D DragStartOffset = FVector2D::ZeroVector;
		FVector2D SelectionRectStart = FVector2D::ZeroVector;
		FVector2D SelectionRectEnd = FVector2D::ZeroVector;
	};

const FName ARInvaderAuthoringEditor::TabName(TEXT("AR_InvaderAuthoringTool"));

TSharedRef<SDockTab> ARInvaderAuthoringEditor::SpawnTab(const FSpawnTabArgs& Args)
{
	(void)Args;
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SInvaderAuthoringPanel)
		];
}

void SInvaderAuthoringPanel::Construct(const FArguments& InArgs)
{
	(void)InArgs;

	WaveProxy.Reset(NewObject<UARInvaderWaveRowProxy>(GetTransientPackage()));
	StageProxy.Reset(NewObject<UARInvaderStageRowProxy>(GetTransientPackage()));
	SpawnProxy.Reset(NewObject<UARInvaderSpawnProxy>(GetTransientPackage()));

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs RowDetailsArgs;
	RowDetailsArgs.bAllowSearch = true;
	RowDetailsArgs.bHideSelectionTip = true;
	RowDetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	RowDetailsArgs.bUpdatesFromSelection = false;

	FDetailsViewArgs SpawnDetailsArgs = RowDetailsArgs;
	SpawnDetailsArgs.bAllowSearch = false;

	RowDetailsView = PropertyEditorModule.CreateDetailView(RowDetailsArgs);
	SpawnDetailsView = PropertyEditorModule.CreateDetailView(SpawnDetailsArgs);
	RowDetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SInvaderAuthoringPanel::ShouldShowWaveDetailProperty));
	SpawnDetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SInvaderAuthoringPanel::ShouldShowSpawnDetailProperty));
	RowDetailsView->OnFinishedChangingProperties().AddSP(SharedThis(this), &SInvaderAuthoringPanel::HandleWaveRowPropertiesChanged);
	SpawnDetailsView->OnFinishedChangingProperties().AddSP(SharedThis(this), &SInvaderAuthoringPanel::HandleSpawnPropertiesChanged);

	BuildLayout();
	RefreshTables();
	{
		const TWeakPtr<SInvaderAuthoringPanel> WeakPanel = SharedThis(this);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPanel](float)
		{
			if (const TSharedPtr<SInvaderAuthoringPanel> Pinned = WeakPanel.Pin())
			{
				Pinned->EnsureInitialTableBackups();
			}
			return false;
		}), 1.0f);
	}
	RefreshPalette();
	RefreshRowItems();
	RefreshDetailsObjects();
	RefreshIssues();
	ObjectTransactedHandle = FCoreUObjectDelegates::OnObjectTransacted.AddSP(SharedThis(this), &SInvaderAuthoringPanel::HandleObjectTransacted);
	SetStatus(TEXT("Invader authoring tool ready."));
}

SInvaderAuthoringPanel::~SInvaderAuthoringPanel()
{
	HandleCanvasEndSpawnDrag();

	if (RowDetailsView.IsValid())
	{
		RowDetailsView->OnFinishedChangingProperties().RemoveAll(this);
	}
	if (SpawnDetailsView.IsValid())
	{
		SpawnDetailsView->OnFinishedChangingProperties().RemoveAll(this);
	}
	if (ObjectTransactedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectTransacted.Remove(ObjectTransactedHandle);
		ObjectTransactedHandle.Reset();
	}
}

FReply SInvaderAuthoringPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	(void)MyGeometry;
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Delete || Key == EKeys::BackSpace)
	{
		if (Mode == EAuthoringMode::Waves)
		{
			if (!SelectedSpawnIndices.IsEmpty())
			{
				return OnDeleteSelectedSpawn();
			}
			SetStatus(TEXT("No spawn selected. Use the row Delete button to remove a wave row."));
			return FReply::Handled();
		}

		if (!SelectedStageRow.IsNone())
		{
			return OnDeleteRow();
		}
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SInvaderAuthoringPanel::BuildLayout()
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked(this, &SInvaderAuthoringPanel::IsModeChecked, EAuthoringMode::Waves)
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
				{
					if (State == ECheckBoxState::Checked)
					{
						SetMode(EAuthoringMode::Waves);
					}
				})
				[
					SNew(STextBlock).Text(FText::FromString("Waves"))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked(this, &SInvaderAuthoringPanel::IsModeChecked, EAuthoringMode::Stages)
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
				{
					if (State == ECheckBoxState::Checked)
					{
						SetMode(EAuthoringMode::Stages);
					}
				})
				[
					SNew(STextBlock).Text(FText::FromString("Stages"))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SSpacer)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SButton)
				.Text(FText::FromString("Reload Tables"))
				.ToolTipText(FText::FromString("Reload wave and stage DataTables from disk."))
				.OnClicked_Lambda([this]()
				{
					RefreshTables();
					RefreshRowItems();
					RefreshDetailsObjects();
					RefreshIssues();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SButton)
				.Text(FText::FromString("Validate Selected"))
				.ToolTipText(FText::FromString("Validate the currently selected wave or stage row."))
				.OnClicked(this, &SInvaderAuthoringPanel::OnValidateSelected)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString("Validate All"))
				.ToolTipText(FText::FromString("Validate all wave and stage rows in loaded tables."))
				.OnClicked(this, &SInvaderAuthoringPanel::OnValidateAll)
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(6.f)
		[
			SNew(SSplitter)
			+ SSplitter::Slot().Value(0.2f)
			[
				SNew(SBorder)
				.Padding(6.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
						[
							SNew(SButton).Text(FText::FromString("New")).OnClicked(this, &SInvaderAuthoringPanel::OnCreateRow)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 0.f, 0.f, 0.f)
						[
							SNew(SButton).Text(FText::FromString("Duplicate")).OnClicked(this, &SInvaderAuthoringPanel::OnDuplicateRow)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SAssignNew(RenameTextBox, SEditableTextBox)
						.HintText_Lambda([this]()
						{
							return Mode == EAuthoringMode::Waves
								? FText::FromString("Name Wave")
								: FText::FromString("Name Stage");
						})
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
						[
							SNew(SButton).Text(FText::FromString("Rename")).OnClicked(this, &SInvaderAuthoringPanel::OnRenameRow)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 0.f, 0.f, 0.f)
						[
							SNew(SButton).Text(FText::FromString("Delete")).OnClicked(this, &SInvaderAuthoringPanel::OnDeleteRow)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
					[
						SNew(SButton).Text(FText::FromString("Save Table")).OnClicked(this, &SInvaderAuthoringPanel::OnSaveTable)
					]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SAssignNew(RowListView, SListView<TSharedPtr<FRowNameItem>>)
						.ListItemsSource(&RowItems)
						.SelectionMode(ESelectionMode::Multi)
						.OnGenerateRow(this, &SInvaderAuthoringPanel::OnGenerateRowNameRow)
						.OnContextMenuOpening(this, &SInvaderAuthoringPanel::OnOpenRowContextMenu)
						.OnSelectionChanged(this, &SInvaderAuthoringPanel::HandleRowListSelectionChanged)
					]
				]
			]
			+ SSplitter::Slot().Value(0.46f)
			[
				SNew(SBorder)
				.Padding(6.f)
				[
					SNew(SWidgetSwitcher)
					.WidgetIndex_Lambda([this]() { return Mode == EAuthoringMode::Waves ? 0 : 1; })
					+ SWidgetSwitcher::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
						[
							SAssignNew(PreviewSlider, SSlider)
							.Value_Lambda([this]()
							{
								const float MaxTime = FMath::Max(0.01f, GetMaxPreviewTime());
								return FMath::Clamp(GetPreviewTime() / MaxTime, 0.f, 1.f);
							})
							.OnValueChanged_Lambda([this](float NewValue)
							{
								SetPreviewTime(NewValue * FMath::Max(0.01f, GetMaxPreviewTime()));
							})
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
						[
							SNew(STextBlock)
							.Text(this, &SInvaderAuthoringPanel::GetPhaseSummaryText)
						]
						+ SVerticalBox::Slot().FillHeight(0.62f).Padding(0.f, 0.f, 0.f, 6.f)
						[
							SAssignNew(WaveCanvas, SInvaderWaveCanvas)
						]
						+ SVerticalBox::Slot().FillHeight(0.38f)
						[
							SNew(SSplitter)
							+ SSplitter::Slot().Value(0.45f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().FillWidth(1.f)
									[
										SNew(STextBlock).Text(FText::FromString("Wave Layers"))
									]
									+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
									[
										SNew(SButton)
										.Text(FText::FromString("+"))
										.ToolTipText(FText::FromString("Add a new spawn delay layer."))
										.OnClicked(this, &SInvaderAuthoringPanel::OnAddLayer)
									]
									+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
									[
										SNew(SCheckBox)
										.IsChecked_Lambda([this]()
										{
											return GetDefault<UARInvaderAuthoringEditorSettings>()->bHideOtherLayersInWavePreview ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
										})
										.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
										{
											UARInvaderAuthoringEditorSettings* Settings = GetMutableDefault<UARInvaderAuthoringEditorSettings>();
											Settings->bHideOtherLayersInWavePreview = (NewState == ECheckBoxState::Checked);
											Settings->SaveConfig();
											if (WaveCanvas.IsValid())
											{
												WaveCanvas->Invalidate(EInvalidateWidget::LayoutAndVolatility);
											}
										})
										[
											SNew(STextBlock).Text(FText::FromString("Hide Other Layers"))
										]
									]
								]
								+ SVerticalBox::Slot().FillHeight(1.f)
								[
									SAssignNew(LayerListView, SListView<TSharedPtr<FLayerItem>>)
									.ListItemsSource(&LayerItems)
									.OnGenerateRow(this, &SInvaderAuthoringPanel::OnGenerateLayerRow)
									.OnSelectionChanged(this, &SInvaderAuthoringPanel::HandleLayerSelectionChanged)
								]
							]
							+ SSplitter::Slot().Value(0.55f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().FillWidth(1.f)
									[
										SNew(STextBlock).Text(FText::FromString("Spawns"))
									]
									+ SHorizontalBox::Slot().AutoWidth().Padding(6.f, 0.f, 0.f, 0.f)
									[
										SNew(SButton)
										.Text(FText::FromString("Delete"))
										.ToolTipText(FText::FromString("Delete selected spawns in this layer list."))
										.OnClicked(this, &SInvaderAuthoringPanel::OnDeleteSelectedSpawn)
									]
								]
								+ SVerticalBox::Slot().FillHeight(1.f)
								[
									SAssignNew(SpawnListView, SListView<TSharedPtr<FSpawnItem>>)
									.ListItemsSource(&SpawnItems)
									.SelectionMode(ESelectionMode::Multi)
									.OnGenerateRow(this, &SInvaderAuthoringPanel::OnGenerateSpawnRow)
									.OnContextMenuOpening(this, &SInvaderAuthoringPanel::OnOpenSpawnContextMenu)
									.OnSelectionChanged(this, &SInvaderAuthoringPanel::HandleSpawnSelectionChanged)
								]
							]
						]
					]
					+ SWidgetSwitcher::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
						[
							SNew(STextBlock)
							.Text(FText::FromString("Stages use structured form editing in the details panel. No stage graph in v1."))
						]
						+ SVerticalBox::Slot().FillHeight(1.f)
						[
							SNew(SBorder)
							.Padding(8.f)
							[
								SNew(STextBlock)
								.Text(FText::FromString("Select a stage row on the left to edit stage parameters."))
							]
						]
					]
				]
			]
			+ SSplitter::Slot().Value(0.34f)
			[
				SNew(SBorder)
				.Padding(6.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
					[
						SNew(STextBlock)
						.Visibility_Lambda([this]() { return Mode == EAuthoringMode::Waves ? EVisibility::Visible : EVisibility::Collapsed; })
						.Text(FText::FromString("Enemy Palette"))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
					[
						SAssignNew(PaletteHost, SBorder)
						.Visibility_Lambda([this]() { return Mode == EAuthoringMode::Waves ? EVisibility::Visible : EVisibility::Collapsed; })
						[
							BuildPaletteWidget()
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(STextBlock).Text(FText::FromString("Selected Row Details"))
					]
					+ SVerticalBox::Slot().FillHeight(0.45f).Padding(0.f, 0.f, 0.f, 6.f)
					[
						RowDetailsView.ToSharedRef()
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(STextBlock)
						.Visibility_Lambda([this]() { return Mode == EAuthoringMode::Waves ? EVisibility::Visible : EVisibility::Collapsed; })
						.Text(FText::FromString("Selected Spawn Details"))
					]
					+ SVerticalBox::Slot().FillHeight(0.28f).Padding(0.f, 0.f, 0.f, 6.f)
					[
						SNew(SBox)
						.Visibility_Lambda([this]() { return Mode == EAuthoringMode::Waves ? EVisibility::Visible : EVisibility::Collapsed; })
						[
							SpawnDetailsView.ToSharedRef()
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(ValidationIssuesArea, SExpandableArea)
						.HeaderContent()
						[
							SNew(STextBlock).Text(FText::FromString("Validation Issues"))
						]
						.BodyContent()
						[
							SNew(SBox)
							.MinDesiredHeight(120.f)
							.MaxDesiredHeight(220.f)
							[
								SAssignNew(IssueListView, SListView<TSharedPtr<FInvaderAuthoringIssue>>)
								.ListItemsSource(&IssueItems)
								.OnGenerateRow(this, &SInvaderAuthoringPanel::OnGenerateIssueRow)
								.OnSelectionChanged(this, &SInvaderAuthoringPanel::HandleIssueSelectionChanged)
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(STextBlock).Text(FText::FromString("PIE Test Harness"))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
						[
							SNew(SButton).Text(FText::FromString("Start/Attach PIE")).OnClicked(this, &SInvaderAuthoringPanel::OnStartOrAttachPIE)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 0.f, 0.f, 0.f)
						[
							SNew(SButton).Text(FText::FromString("Stop PIE")).OnClicked(this, &SInvaderAuthoringPanel::OnStopPIE)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
						[
							SNew(SButton).Text(FText::FromString("Start Run")).OnClicked(this, &SInvaderAuthoringPanel::OnStartRun)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 0.f, 0.f, 0.f)
						[
							SNew(SButton).Text(FText::FromString("Stop Run")).OnClicked(this, &SInvaderAuthoringPanel::OnStopRun)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
						[
							SNew(SButton).Text(FText::FromString("Force Stage")).OnClicked(this, &SInvaderAuthoringPanel::OnForceStage)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 0.f, 0.f, 0.f)
						[
							SNew(SButton).Text(FText::FromString("Force Wave")).OnClicked(this, &SInvaderAuthoringPanel::OnForceWave)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
						[
							SAssignNew(ThreatTextBox, SEditableTextBox)
							.HintText(FText::FromString("Threat value"))
							.Text(FText::FromString("0"))
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton).Text(FText::FromString("Force Threat")).OnClicked(this, &SInvaderAuthoringPanel::OnForceThreat)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(SButton).Text(FText::FromString("Dump Runtime State")).OnClicked(this, &SInvaderAuthoringPanel::OnDumpState)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(StatusText, STextBlock).Text(FText::FromString("Ready"))
					]
				]
			]
		]
	];

	if (WaveCanvas.IsValid())
	{
		WaveCanvas->SetDataAccessors(
			[this]() { return GetSelectedWaveRowConst(); },
			[this]() { return SelectedLayerDelay; },
			[]() { return GetDefault<UARInvaderAuthoringEditorSettings>()->bHideOtherLayersInWavePreview; },
			[this]() { return SelectedSpawnIndices; },
			[this]() { return PreviewTime; },
			[this](int32 SpawnIndex, bool bToggle, bool bRangeSelect) { HandleCanvasSpawnSelected(SpawnIndex, bToggle, bRangeSelect); },
			[this]() { HandleCanvasClearSpawnSelection(); },
			[this](int32 SpawnIndex, const FVector2D& ScreenPos) { HandleCanvasOpenSpawnContextMenu(SpawnIndex, ScreenPos); },
			[this]() { HandleCanvasBeginSpawnDrag(); },
			[this]() { HandleCanvasEndSpawnDrag(); },
			[this](const FVector2D& OffsetDelta) { HandleCanvasSelectedSpawnsMoved(OffsetDelta); },
			[this](const TArray<int32>& RectSelection, bool bAppendToSelection) { HandleCanvasSelectionRectChanged(RectSelection, bAppendToSelection); },
			[this](const FVector2D& NewOffset) { HandleCanvasAddSpawnAt(NewOffset); });
	}
}

void SInvaderAuthoringPanel::RefreshTables()
{
	WaveTable = nullptr;
	StageTable = nullptr;

	const UARInvaderToolingSettings* ToolingSettings = GetDefault<UARInvaderToolingSettings>();
	if (!ToolingSettings)
	{
		SetStatus(TEXT("Missing UARInvaderToolingSettings."));
		return;
	}

	WaveTable = ToolingSettings->WaveDataTable.LoadSynchronous();
	StageTable = ToolingSettings->StageDataTable.LoadSynchronous();
	if (!WaveTable || !StageTable)
	{
		const UARInvaderDirectorSettings* RuntimeSettings = GetDefault<UARInvaderDirectorSettings>();
		if (RuntimeSettings)
		{
			if (!WaveTable)
			{
				WaveTable = RuntimeSettings->WaveDataTable.LoadSynchronous();
			}
			if (!StageTable)
			{
				StageTable = RuntimeSettings->StageDataTable.LoadSynchronous();
			}
		}
	}

	if (!WaveTable || !StageTable)
	{
		SetStatus(FString::Printf(
			TEXT("Failed loading tables. Tooling Wave='%s' Stage='%s'."),
			*ToolingSettings->WaveDataTable.ToString(),
			*ToolingSettings->StageDataTable.ToString()));
	}
}

UDataTable* SInvaderAuthoringPanel::GetActiveTable() const
{
	return Mode == EAuthoringMode::Waves ? WaveTable : StageTable;
}

const FARWaveDefRow* SInvaderAuthoringPanel::GetWaveRow(FName RowName) const
{
	if (!WaveTable || RowName.IsNone())
	{
		return nullptr;
	}
	return WaveTable->FindRow<FARWaveDefRow>(RowName, TEXT("InvaderAuthoring"), false);
}

const FARStageDefRow* SInvaderAuthoringPanel::GetStageRow(FName RowName) const
{
	if (!StageTable || RowName.IsNone())
	{
		return nullptr;
	}
	return StageTable->FindRow<FARStageDefRow>(RowName, TEXT("InvaderAuthoring"), false);
}

FARWaveDefRow* SInvaderAuthoringPanel::GetMutableWaveRow(FName RowName)
{
	if (!WaveTable || RowName.IsNone())
	{
		return nullptr;
	}
	return WaveTable->FindRow<FARWaveDefRow>(RowName, TEXT("InvaderAuthoring"), false);
}

FARStageDefRow* SInvaderAuthoringPanel::GetMutableStageRow(FName RowName)
{
	if (!StageTable || RowName.IsNone())
	{
		return nullptr;
	}
	return StageTable->FindRow<FARStageDefRow>(RowName, TEXT("InvaderAuthoring"), false);
}

FARWaveDefRow* SInvaderAuthoringPanel::GetSelectedWaveRow()
{
	return GetMutableWaveRow(SelectedWaveRow);
}

FARStageDefRow* SInvaderAuthoringPanel::GetSelectedStageRow()
{
	return GetMutableStageRow(SelectedStageRow);
}

const FARWaveDefRow* SInvaderAuthoringPanel::GetSelectedWaveRowConst() const
{
	return GetWaveRow(SelectedWaveRow);
}

const FARStageDefRow* SInvaderAuthoringPanel::GetSelectedStageRowConst() const
{
	return GetStageRow(SelectedStageRow);
}

FARWaveEnemySpawnDef* SInvaderAuthoringPanel::GetSelectedSpawn()
{
	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row)
	{
		return nullptr;
	}

	if (!Row->EnemySpawns.IsValidIndex(SelectedSpawnIndex))
	{
		for (int32 Index : SelectedSpawnIndices)
		{
			if (Row->EnemySpawns.IsValidIndex(Index))
			{
				SelectedSpawnIndex = Index;
				break;
			}
		}
		if (!Row->EnemySpawns.IsValidIndex(SelectedSpawnIndex))
		{
			return nullptr;
		}
	}
	return &Row->EnemySpawns[SelectedSpawnIndex];
}

void SInvaderAuthoringPanel::RefreshRowItems()
{
	RowItems.Reset();
	UDataTable* Table = GetActiveTable();
	if (!Table)
	{
		if (RowListView.IsValid())
		{
			RowListView->RequestListRefresh();
		}
		return;
	}

	TArray<FName> Names;
	Table->GetRowMap().GenerateKeyArray(Names);
	Names.Sort([](const FName& A, const FName& B)
	{
		return A.LexicalLess(B);
	});

	for (const FName& Name : Names)
	{
		TSharedPtr<FRowNameItem> Item = MakeShared<FRowNameItem>();
		Item->RowName = Name;
		RowItems.Add(Item);
	}

	if (Mode == EAuthoringMode::Waves)
	{
		if (SelectedWaveRow.IsNone() || !Table->GetRowMap().Contains(SelectedWaveRow))
		{
			SelectedWaveRow = Names.IsEmpty() ? NAME_None : Names[0];
		}
	}
	else
	{
		if (SelectedStageRow.IsNone() || !Table->GetRowMap().Contains(SelectedStageRow))
		{
			SelectedStageRow = Names.IsEmpty() ? NAME_None : Names[0];
		}
	}

	if (RowListView.IsValid())
	{
		RowListView->RequestListRefresh();
		for (const TSharedPtr<FRowNameItem>& Item : RowItems)
		{
			if (!Item.IsValid())
			{
				continue;
			}
			const FName Target = Mode == EAuthoringMode::Waves ? SelectedWaveRow : SelectedStageRow;
			if (Item->RowName == Target)
			{
				RowListView->SetSelection(Item);
				break;
			}
		}
	}

	RefreshLayerItems();
	RefreshSpawnItems();
}

void SInvaderAuthoringPanel::RefreshLayerItems()
{
	LayerInfos.Reset();
	LayerItems.Reset();
	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row)
	{
		if (LayerListView.IsValid())
		{
			LayerListView->RequestListRefresh();
		}
		return;
	}

	TArray<float> UniqueDelays;
	for (const FARWaveEnemySpawnDef& Spawn : Row->EnemySpawns)
	{
		if (!UniqueDelays.ContainsByPredicate([&Spawn](float Existing)
		{
			return FMath::IsNearlyEqual(Existing, Spawn.SpawnDelay, LayerMatchTolerance);
		}))
		{
			UniqueDelays.Add(Spawn.SpawnDelay);
		}
	}

	UniqueDelays.Sort([](float A, float B)
	{
		return A < B;
	});

	for (float Delay : UniqueDelays)
	{
		FInvaderLayerInfo Layer;
		Layer.Delay = Delay;
		for (int32 Index = 0; Index < Row->EnemySpawns.Num(); ++Index)
		{
			if (FMath::IsNearlyEqual(Row->EnemySpawns[Index].SpawnDelay, Delay, LayerMatchTolerance))
			{
				Layer.SpawnIndices.Add(Index);
			}
		}
		LayerInfos.Add(Layer);

		TSharedPtr<FLayerItem> Item = MakeShared<FLayerItem>();
		Item->Delay = Delay;
		Item->Count = Layer.SpawnIndices.Num();
		LayerItems.Add(Item);
	}

	if (LayerInfos.IsEmpty())
	{
		SelectedLayerDelay = 0.f;
	}
	else if (!LayerInfos.ContainsByPredicate([this](const FInvaderLayerInfo& Layer)
	{
		return FMath::IsNearlyEqual(Layer.Delay, SelectedLayerDelay, LayerMatchTolerance);
	}))
	{
		SelectedLayerDelay = LayerInfos[0].Delay;
	}

	if (LayerListView.IsValid())
	{
		LayerListView->RequestListRefresh();
		for (const TSharedPtr<FLayerItem>& Item : LayerItems)
		{
			if (Item.IsValid() && FMath::IsNearlyEqual(Item->Delay, SelectedLayerDelay, LayerMatchTolerance))
			{
				LayerListView->SetSelection(Item);
				break;
			}
		}
	}
}

void SInvaderAuthoringPanel::RefreshSpawnItems()
{
	SpawnItems.Reset();
	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row)
	{
		SelectedSpawnIndex = INDEX_NONE;
		if (SpawnListView.IsValid())
		{
			SpawnListView->RequestListRefresh();
		}
		return;
	}

	for (const FInvaderLayerInfo& Layer : LayerInfos)
	{
		if (!FMath::IsNearlyEqual(Layer.Delay, SelectedLayerDelay, LayerMatchTolerance))
		{
			continue;
		}

		for (const int32 SpawnIndex : Layer.SpawnIndices)
		{
			if (!Row->EnemySpawns.IsValidIndex(SpawnIndex))
			{
				continue;
			}

			const FARWaveEnemySpawnDef& Spawn = Row->EnemySpawns[SpawnIndex];
			TSharedPtr<FSpawnItem> Item = MakeShared<FSpawnItem>();
			Item->SpawnIndex = SpawnIndex;
			Item->Delay = Spawn.SpawnDelay;
			Item->Color = Spawn.EnemyColor;
			Item->EnemyClassName = Spawn.EnemyClass
				? MakePaletteClassDisplayName(FSoftClassPath(Spawn.EnemyClass))
				: TEXT("<None>");
			SpawnItems.Add(Item);
		}
		break;
	}

	TSet<int32> VisibleIndices;
	for (const TSharedPtr<FSpawnItem>& Item : SpawnItems)
	{
		if (Item.IsValid())
		{
			VisibleIndices.Add(Item->SpawnIndex);
		}
	}
	for (TSet<int32>::TIterator It(SelectedSpawnIndices); It; ++It)
	{
		if (!VisibleIndices.Contains(*It))
		{
			It.RemoveCurrent();
		}
	}
	if (SelectedSpawnIndex != INDEX_NONE && !SelectedSpawnIndices.Contains(SelectedSpawnIndex))
	{
		SelectedSpawnIndex = INDEX_NONE;
	}
	if (SelectedSpawnIndex == INDEX_NONE && SelectedSpawnIndices.Num() > 0)
	{
		SelectedSpawnIndex = *SelectedSpawnIndices.CreateConstIterator();
	}

	if (SpawnListView.IsValid())
	{
		SpawnListView->RequestListRefresh();
		SyncSpawnListSelectionFromState();
	}

	RefreshDetailsObjects();
}

void SInvaderAuthoringPanel::RefreshDetailsObjects()
{
	bApplyingProxyToModel = true;
	if (Mode == EAuthoringMode::Waves)
	{
		FARWaveDefRow* Row = GetSelectedWaveRow();
		if (Row && WaveProxy)
		{
			WaveProxy->Row = *Row;
			RowDetailsView->SetObject(WaveProxy.Get());
		}
		else
		{
			RowDetailsView->SetObject(nullptr);
		}
	}
	else
	{
		FARStageDefRow* Row = GetSelectedStageRow();
		if (Row && StageProxy)
		{
			StageProxy->Row = *Row;
			RowDetailsView->SetObject(StageProxy.Get());
		}
		else
		{
			RowDetailsView->SetObject(nullptr);
		}
	}

	FARWaveEnemySpawnDef* Spawn = GetSelectedSpawn();
	if (Spawn && SpawnProxy)
	{
		SpawnProxy->Spawn = *Spawn;
		SpawnDetailsView->SetObject(SpawnProxy.Get());
	}
	else
	{
		SpawnDetailsView->SetObject(nullptr);
	}
	bApplyingProxyToModel = false;

	if (WaveCanvas.IsValid())
	{
		WaveCanvas->Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}
}

FName SInvaderAuthoringPanel::MakeUniqueRowName(UDataTable* Table, const FString& BaseName) const
{
	if (!Table)
	{
		return FName(*BaseName);
	}

	FString Candidate = BaseName;
	int32 Suffix = 1;
	while (Table->GetRowMap().Contains(FName(*Candidate)))
	{
		Candidate = FString::Printf(TEXT("%s_%d"), *BaseName, Suffix++);
	}
	return FName(*Candidate);
}

void SInvaderAuthoringPanel::MarkTableDirty(UDataTable* Table)
{
	if (!Table)
	{
		return;
	}
	Table->MarkPackageDirty();

	const UARInvaderToolingSettings* ToolingSettings = GetDefault<UARInvaderToolingSettings>();
	if (ToolingSettings && ToolingSettings->bAutoSaveTablesOnEdit)
	{
		SaveTable(Table, false);
	}
}

void SInvaderAuthoringPanel::SaveTable(UDataTable* Table, bool bPromptForCheckout)
{
	if (!Table)
	{
		return;
	}

	if (bPromptForCheckout)
	{
		TArray<UPackage*> Packages;
		Packages.Add(Table->GetOutermost());
		FEditorFileUtils::PromptForCheckoutAndSave(Packages, false, false);
		return;
	}

	if (!SaveTablePackage(Table))
	{
		SetStatus(FString::Printf(TEXT("Auto-save failed for '%s'."), *Table->GetName()));
	}
}

void SInvaderAuthoringPanel::SaveActiveTable(bool bPromptForCheckout)
{
	SaveTable(GetActiveTable(), bPromptForCheckout);
}

bool SInvaderAuthoringPanel::SaveTablePackage(UDataTable* Table) const
{
	if (!Table || !Table->GetOutermost())
	{
		return false;
	}

	const FString PackageName = Table->GetOutermost()->GetName();
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GError;
	SaveArgs.SaveFlags = SAVE_NoError;
	return UPackage::SavePackage(Table->GetOutermost(), Table, *PackageFilename, SaveArgs);
}

void SInvaderAuthoringPanel::EnsureInitialTableBackups()
{
	if (bInitialBackupsCreated)
	{
		return;
	}

	const UARInvaderToolingSettings* ToolingSettings = GetDefault<UARInvaderToolingSettings>();
	if (!ToolingSettings || !ToolingSettings->bCreateBackupOnToolOpen)
	{
		bInitialBackupsCreated = true;
		return;
	}

	FString BackupFolder = ToolingSettings->BackupsFolder.Path;
	if (BackupFolder.IsEmpty())
	{
		BackupFolder = TEXT("/Game/Data/Backups");
	}
	if (!BackupFolder.StartsWith(TEXT("/")))
	{
		BackupFolder = FString(TEXT("/")) + BackupFolder;
	}
	BackupFolder.RemoveFromEnd(TEXT("/"));

	if (!FPackageName::IsValidLongPackageName(BackupFolder))
	{
		SetStatus(FString::Printf(TEXT("Backups folder '%s' is not a valid package path. Skipping backups."), *BackupFolder));
		bInitialBackupsCreated = true;
		return;
	}

	const int32 RetentionCount = FMath::Max(1, ToolingSettings->BackupRetentionCount);
	if (WaveTable)
	{
		CreateTableBackup(WaveTable, BackupFolder, RetentionCount);
	}
	if (StageTable)
	{
		CreateTableBackup(StageTable, BackupFolder, RetentionCount);
	}

	bInitialBackupsCreated = true;
}

void SInvaderAuthoringPanel::CreateTableBackup(UDataTable* Table, const FString& BackupFolderLongPackagePath, int32 RetentionCount)
{
	if (!Table || !Table->GetOutermost())
	{
		return;
	}

	const FString SourcePackageName = Table->GetOutermost()->GetName();
	const FString SourceAssetName = FPackageName::GetLongPackageAssetName(SourcePackageName);
	const FDateTime Now = FDateTime::Now();
	const FString Timestamp = FString::Printf(TEXT("%04d%02d%02d_%02d%02d%02d"), Now.GetYear(), Now.GetMonth(), Now.GetDay(), Now.GetHour(), Now.GetMinute(), Now.GetSecond());
	const FString BackupAssetName = FString::Printf(TEXT("%s__%s"), *SourceAssetName, *Timestamp);
	const FString BackupPackageName = BackupFolderLongPackagePath / BackupAssetName;
	const FString BackupFilename = FPackageName::LongPackageNameToFilename(BackupPackageName, FPackageName::GetAssetPackageExtension());

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(BackupFilename), true);

	UPackage* BackupPackage = CreatePackage(*BackupPackageName);
	if (!BackupPackage)
	{
		SetStatus(FString::Printf(TEXT("Failed to create backup package for '%s'."), *SourceAssetName));
		return;
	}

	UObject* ExistingObject = StaticFindObject(UDataTable::StaticClass(), BackupPackage, *BackupAssetName);
	if (ExistingObject)
	{
		ExistingObject->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
	}

	UDataTable* BackupTable = DuplicateObject<UDataTable>(Table, BackupPackage, *BackupAssetName);
	if (!BackupTable)
	{
		SetStatus(FString::Printf(TEXT("Failed to duplicate backup table for '%s'."), *SourceAssetName));
		return;
	}

	BackupPackage->MarkPackageDirty();
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GError;
	// Backups are editor safety snapshots; save as autosave-style writes to avoid startup validator churn.
	SaveArgs.SaveFlags = SAVE_NoError | SAVE_FromAutosave;
	if (!UPackage::SavePackage(BackupPackage, BackupTable, *BackupFilename, SaveArgs))
	{
		SetStatus(FString::Printf(TEXT("Failed to save backup '%s'."), *BackupAssetName));
		return;
	}

	PruneTableBackups(BackupFolderLongPackagePath, SourceAssetName, RetentionCount);
}

void SInvaderAuthoringPanel::PruneTableBackups(const FString& BackupFolderLongPackagePath, const FString& AssetBaseName, int32 RetentionCount)
{
	const FString BackupDirectory = FPackageName::LongPackageNameToFilename(BackupFolderLongPackagePath, TEXT(""));
	const FString Pattern = FString::Printf(TEXT("%s__*.uasset"), *AssetBaseName);
	TArray<FString> BackupFiles;
	IFileManager::Get().FindFiles(BackupFiles, *(BackupDirectory / Pattern), true, false);
	BackupFiles.Sort([](const FString& A, const FString& B)
	{
		return A > B;
	});

	for (int32 Index = RetentionCount; Index < BackupFiles.Num(); ++Index)
	{
		const FString BackupUAssetPath = BackupDirectory / BackupFiles[Index];
		const FString BasePath = FPaths::ChangeExtension(BackupUAssetPath, TEXT(""));
		IFileManager::Get().Delete(*BackupUAssetPath, false, true, true);
		IFileManager::Get().Delete(*(BasePath + TEXT(".uexp")), false, true, true);
		IFileManager::Get().Delete(*(BasePath + TEXT(".ubulk")), false, true, true);
	}
}

void SInvaderAuthoringPanel::SetStatus(const FString& Message)
{
	UE_LOG(ARLog, Log, TEXT("[InvaderAuthoring] %s"), *Message);
	if (StatusText.IsValid())
	{
		StatusText->SetText(FText::FromString(Message));
	}
}

TArray<FName> SInvaderAuthoringPanel::GetSelectedRowNames() const
{
	TArray<FName> Result;
	if (!RowListView.IsValid())
	{
		return Result;
	}

	const TArray<TSharedPtr<FRowNameItem>> SelectedItems = RowListView->GetSelectedItems();
	Result.Reserve(SelectedItems.Num());
	for (const TSharedPtr<FRowNameItem>& Item : SelectedItems)
	{
		if (Item.IsValid() && !Item->RowName.IsNone())
		{
			Result.AddUnique(Item->RowName);
		}
	}
	if (Result.IsEmpty())
	{
		const FName Fallback = Mode == EAuthoringMode::Waves ? SelectedWaveRow : SelectedStageRow;
		if (!Fallback.IsNone())
		{
			Result.Add(Fallback);
		}
	}
	return Result;
}

FName SInvaderAuthoringPanel::GetPrimarySelectedRowName() const
{
	const TArray<FName> SelectedRows = GetSelectedRowNames();
	if (!SelectedRows.IsEmpty())
	{
		return SelectedRows[0];
	}

	return Mode == EAuthoringMode::Waves ? SelectedWaveRow : SelectedStageRow;
}

void SInvaderAuthoringPanel::SetMode(EAuthoringMode NewMode)
{
	if (Mode == NewMode)
	{
		return;
	}

	HandleCanvasEndSpawnDrag();
	Mode = NewMode;
	RefreshRowItems();
	RefreshDetailsObjects();
}

ECheckBoxState SInvaderAuthoringPanel::IsModeChecked(EAuthoringMode QueryMode) const
{
	return Mode == QueryMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SInvaderAuthoringPanel::SelectWaveRow(FName RowName)
{
	SelectedWaveRow = RowName;
	ClearSpawnSelection();
	RefreshLayerItems();
	RefreshSpawnItems();
	RefreshDetailsObjects();
}

void SInvaderAuthoringPanel::SelectStageRow(FName RowName)
{
	SelectedStageRow = RowName;
	SelectedSpawnIndex = INDEX_NONE;
	SelectedSpawnIndices.Reset();
	RefreshDetailsObjects();
}

void SInvaderAuthoringPanel::SelectLayerByDelay(float Delay)
{
	SelectedLayerDelay = Delay;
	RefreshSpawnItems();
	if (WaveCanvas.IsValid())
	{
		WaveCanvas->Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}
}

void SInvaderAuthoringPanel::SelectSpawn(int32 SpawnIndex)
{
	TSet<int32> Selection;
	if (SpawnIndex != INDEX_NONE)
	{
		Selection.Add(SpawnIndex);
	}
	ApplySpawnSelection(Selection, SpawnIndex);
}

void SInvaderAuthoringPanel::ApplySpawnSelection(const TSet<int32>& NewSelection, int32 PreferredPrimary)
{
	SelectedSpawnIndices = NewSelection;
	if (SelectedSpawnIndices.IsEmpty())
	{
		SelectedSpawnIndex = INDEX_NONE;
	}
	else if (PreferredPrimary != INDEX_NONE && SelectedSpawnIndices.Contains(PreferredPrimary))
	{
		SelectedSpawnIndex = PreferredPrimary;
	}
	else if (!SelectedSpawnIndices.Contains(SelectedSpawnIndex))
	{
		SelectedSpawnIndex = *SelectedSpawnIndices.CreateConstIterator();
	}

	if (SpawnListView.IsValid())
	{
		SyncSpawnListSelectionFromState();
	}

	RefreshDetailsObjects();
	if (WaveCanvas.IsValid())
	{
		WaveCanvas->Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}
}

void SInvaderAuthoringPanel::ClearSpawnSelection()
{
	ApplySpawnSelection(TSet<int32>(), INDEX_NONE);
}

void SInvaderAuthoringPanel::SyncSpawnListSelectionFromState()
{
	if (!SpawnListView.IsValid())
	{
		return;
	}

	TGuardValue<bool> SelectionGuard(bSyncingSpawnSelection, true);
	SpawnListView->ClearSelection();
	for (const TSharedPtr<FSpawnItem>& Item : SpawnItems)
	{
		if (Item.IsValid() && SelectedSpawnIndices.Contains(Item->SpawnIndex))
		{
			SpawnListView->SetItemSelection(Item, true, ESelectInfo::Direct);
		}
	}
}

FReply SInvaderAuthoringPanel::OnCreateRow()
{
	UDataTable* Table = GetActiveTable();
	if (!Table)
	{
		SetStatus(TEXT("No active table loaded."));
		return FReply::Handled();
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringCreateRow", "Create Invader Row"));
	Table->Modify();
	const FString RequestedName = RenameTextBox.IsValid() ? RenameTextBox->GetText().ToString().TrimStartAndEnd() : FString();

	if (Mode == EAuthoringMode::Waves)
	{
		const FString BaseName = RequestedName.IsEmpty() ? FString(TEXT("Wave_New")) : RequestedName;
		const FName RowName = MakeUniqueRowName(Table, BaseName);
		FARWaveDefRow NewRow;
		Table->AddRow(RowName, NewRow);
		SelectedWaveRow = RowName;
	}
	else
	{
		const FString BaseName = RequestedName.IsEmpty() ? FString(TEXT("Stage_New")) : RequestedName;
		const FName RowName = MakeUniqueRowName(Table, BaseName);
		FARStageDefRow NewRow;
		Table->AddRow(RowName, NewRow);
		SelectedStageRow = RowName;
	}

	MarkTableDirty(Table);
	RefreshRowItems();
	RefreshDetailsObjects();
	if (!RequestedName.IsEmpty() && RenameTextBox.IsValid())
	{
		RenameTextBox->SetText(FText::GetEmpty());
	}
	SetStatus(TEXT("Created new row."));
	return FReply::Handled();
}

FReply SInvaderAuthoringPanel::OnDuplicateRow()
{
	UDataTable* Table = GetActiveTable();
	if (!Table)
	{
		SetStatus(TEXT("No active table loaded."));
		return FReply::Handled();
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringDuplicateRow", "Duplicate Invader Row"));
	Table->Modify();
	const TArray<FName> SourceRows = GetSelectedRowNames();
	if (SourceRows.IsEmpty())
	{
		SetStatus(TEXT("Select one or more rows first."));
		return FReply::Handled();
	}

	int32 DuplicatedCount = 0;

	if (Mode == EAuthoringMode::Waves)
	{
		for (const FName SourceRow : SourceRows)
		{
			const FARWaveDefRow* Source = Table->FindRow<FARWaveDefRow>(SourceRow, TEXT("Duplicate"), false);
			if (!Source)
			{
				continue;
			}

			const FName NewName = MakeUniqueRowName(Table, SourceRow.ToString() + TEXT("_Copy"));
			Table->AddRow(NewName, *Source);
			SelectedWaveRow = NewName;
			++DuplicatedCount;
		}
	}
	else
	{
		for (const FName SourceRow : SourceRows)
		{
			const FARStageDefRow* Source = Table->FindRow<FARStageDefRow>(SourceRow, TEXT("Duplicate"), false);
			if (!Source)
			{
				continue;
			}

			const FName NewName = MakeUniqueRowName(Table, SourceRow.ToString() + TEXT("_Copy"));
			Table->AddRow(NewName, *Source);
			SelectedStageRow = NewName;
			++DuplicatedCount;
		}
	}

	if (DuplicatedCount <= 0)
	{
		SetStatus(TEXT("No valid selected rows to duplicate."));
		return FReply::Handled();
	}

	MarkTableDirty(Table);
	RefreshRowItems();
	RefreshDetailsObjects();
	SetStatus(FString::Printf(TEXT("Duplicated %d row(s)."), DuplicatedCount));
	return FReply::Handled();
}

FReply SInvaderAuthoringPanel::OnRenameRow()
{
	UDataTable* Table = GetActiveTable();
	const FString NewNameString = RenameTextBox.IsValid() ? RenameTextBox->GetText().ToString().TrimStartAndEnd() : FString();
	if (!Table || NewNameString.IsEmpty())
	{
		SetStatus(TEXT("Enter a new row name."));
		return FReply::Handled();
	}

	const FName OldName = (Mode == EAuthoringMode::Waves) ? SelectedWaveRow : SelectedStageRow;
	const FName NewName(*NewNameString);
	if (OldName.IsNone())
	{
		SetStatus(TEXT("Select a row first."));
		return FReply::Handled();
	}
	if (Table->GetRowMap().Contains(NewName))
	{
		SetStatus(TEXT("Target row name already exists."));
		return FReply::Handled();
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringRenameRow", "Rename Invader Row"));
	Table->Modify();
	if (Mode == EAuthoringMode::Waves)
	{
		const FARWaveDefRow* Source = Table->FindRow<FARWaveDefRow>(OldName, TEXT("Rename"), false);
		if (!Source)
		{
			return FReply::Handled();
		}
		FARWaveDefRow Copy = *Source;
		Table->RemoveRow(OldName);
		Table->AddRow(NewName, Copy);
		SelectedWaveRow = NewName;
	}
	else
	{
		const FARStageDefRow* Source = Table->FindRow<FARStageDefRow>(OldName, TEXT("Rename"), false);
		if (!Source)
		{
			return FReply::Handled();
		}
		FARStageDefRow Copy = *Source;
		Table->RemoveRow(OldName);
		Table->AddRow(NewName, Copy);
		SelectedStageRow = NewName;
	}

	MarkTableDirty(Table);
	RefreshRowItems();
	RefreshDetailsObjects();
	if (RenameTextBox.IsValid())
	{
		RenameTextBox->SetText(FText::GetEmpty());
	}
	SetStatus(FString::Printf(TEXT("Renamed row '%s' -> '%s'."), *OldName.ToString(), *NewName.ToString()));
	return FReply::Handled();
}

FReply SInvaderAuthoringPanel::OnDeleteRow()
{
	UDataTable* Table = GetActiveTable();
	if (!Table)
	{
		return FReply::Handled();
	}

	const TArray<FName> Targets = GetSelectedRowNames();
	if (Targets.IsEmpty())
	{
		SetStatus(TEXT("Select a row first."));
		return FReply::Handled();
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringDeleteRow", "Delete Invader Row"));
	Table->Modify();
	int32 DeletedCount = 0;
	for (const FName Target : Targets)
	{
		if (Table->GetRowMap().Contains(Target))
		{
			Table->RemoveRow(Target);
			++DeletedCount;
		}
	}
	if (DeletedCount <= 0)
	{
		SetStatus(TEXT("No valid selected rows to delete."));
		return FReply::Handled();
	}
	MarkTableDirty(Table);

	if (Mode == EAuthoringMode::Waves)
	{
		SelectedWaveRow = NAME_None;
		SelectedSpawnIndex = INDEX_NONE;
		SelectedSpawnIndices.Reset();
	}
	else
	{
		SelectedStageRow = NAME_None;
	}

	RefreshRowItems();
	RefreshDetailsObjects();
	SetStatus(FString::Printf(TEXT("Deleted %d row(s)."), DeletedCount));
	return FReply::Handled();
}

FReply SInvaderAuthoringPanel::OnSaveTable()
{
	SaveActiveTable();
	SetStatus(TEXT("Save prompt opened for active table."));
	return FReply::Handled();
}

FReply SInvaderAuthoringPanel::OnAddLayer()
{
	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row || !WaveTable)
	{
		SetStatus(TEXT("Select a wave row first."));
		return FReply::Handled();
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringAddLayer", "Add Wave Layer"));
	WaveTable->Modify();

	float NewDelay = 0.f;
	if (!LayerInfos.IsEmpty())
	{
		NewDelay = LayerInfos.Last().Delay + 1.f;
	}

	FARWaveEnemySpawnDef Spawn;
	Spawn.SpawnDelay = NewDelay;
	if (ActivePaletteEntry.IsSet())
	{
		UClass* ResolvedClass = ActivePaletteEntry->EnemyClassPath.ResolveClass();
		if (!ResolvedClass)
		{
			ResolvedClass = LoadClass<AAREnemyBase>(nullptr, *ActivePaletteEntry->EnemyClassPath.ToString());
		}
		Spawn.EnemyClass = ResolvedClass;
		Spawn.EnemyColor = ActivePaletteEntry->Color;
	}

	Row->EnemySpawns.Add(Spawn);
	SelectedLayerDelay = NewDelay;
	SelectSpawn(Row->EnemySpawns.Num() - 1);

	MarkTableDirty(WaveTable);
	RefreshLayerItems();
	RefreshSpawnItems();
	SetStatus(TEXT("Added layer with an initial spawn entry."));
	return FReply::Handled();
}

FReply SInvaderAuthoringPanel::OnAddSpawnToLayer()
{
	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row || !WaveTable)
	{
		SetStatus(TEXT("Select a wave row first."));
		return FReply::Handled();
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringAddSpawn", "Add Wave Spawn"));
	WaveTable->Modify();

	FARWaveEnemySpawnDef Spawn;
	Spawn.SpawnDelay = SelectedLayerDelay;
	if (ActivePaletteEntry.IsSet())
	{
		UClass* ResolvedClass = ActivePaletteEntry->EnemyClassPath.ResolveClass();
		if (!ResolvedClass)
		{
			ResolvedClass = LoadClass<AAREnemyBase>(nullptr, *ActivePaletteEntry->EnemyClassPath.ToString());
		}
		Spawn.EnemyClass = ResolvedClass;
		Spawn.EnemyColor = ActivePaletteEntry->Color;
	}

	Row->EnemySpawns.Add(Spawn);
	SelectSpawn(Row->EnemySpawns.Num() - 1);
	MarkTableDirty(WaveTable);
	RefreshLayerItems();
	RefreshSpawnItems();
	SetStatus(TEXT("Added spawn to selected layer."));
	return FReply::Handled();
}

FReply SInvaderAuthoringPanel::OnDeleteSelectedSpawn()
{
	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row || !WaveTable || SelectedSpawnIndices.IsEmpty())
	{
		SetStatus(TEXT("Select a spawn first."));
		return FReply::Handled();
	}

	TArray<int32> ToDelete = SelectedSpawnIndices.Array();
	ToDelete.Sort(TGreater<int32>());
	ToDelete = ToDelete.FilterByPredicate([Row](int32 Index) { return Row->EnemySpawns.IsValidIndex(Index); });
	if (ToDelete.IsEmpty())
	{
		SetStatus(TEXT("Selected spawn set is out of date. Refreshing selection."));
		ClearSpawnSelection();
		RefreshSpawnItems();
		return FReply::Handled();
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringDeleteSpawn", "Delete Wave Spawn"));
	WaveTable->Modify();
	for (int32 Index : ToDelete)
	{
		Row->EnemySpawns.RemoveAt(Index);
	}
	ClearSpawnSelection();

	MarkTableDirty(WaveTable);
	RefreshLayerItems();
	RefreshSpawnItems();
	SetStatus(FString::Printf(TEXT("Deleted %d selected spawn(s)."), ToDelete.Num()));
	return FReply::Handled();
}

void SInvaderAuthoringPanel::SetSelectedSpawnColor(EAREnemyColor NewColor)
{
	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row || !WaveTable || SelectedSpawnIndices.IsEmpty())
	{
		return;
	}

	TArray<int32> TargetIndices = SelectedSpawnIndices.Array();
	TargetIndices.Sort();
	bool bAnyChanged = false;
	for (int32 Index : TargetIndices)
	{
		if (Row->EnemySpawns.IsValidIndex(Index) && Row->EnemySpawns[Index].EnemyColor != NewColor)
		{
			bAnyChanged = true;
			break;
		}
	}

	if (!bAnyChanged)
	{
		return;
	}
	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringSetSpawnColor", "Set Wave Spawn Color"));
	WaveTable->Modify();
	for (int32 Index : TargetIndices)
	{
		if (Row->EnemySpawns.IsValidIndex(Index))
		{
			Row->EnemySpawns[Index].EnemyColor = NewColor;
		}
	}
	MarkTableDirty(WaveTable);
	RefreshSpawnItems();
	RefreshDetailsObjects();
	if (WaveCanvas.IsValid())
	{
		WaveCanvas->Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}
	SetStatus(FString::Printf(TEXT("Set selected spawn color: %s."), *EnemyColorToName(NewColor)));
}

TSharedPtr<SWidget> SInvaderAuthoringPanel::OnOpenSpawnContextMenu()
{
	if (Mode != EAuthoringMode::Waves || SelectedSpawnIndices.IsEmpty())
	{
		return nullptr;
	}

	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row)
	{
		return nullptr;
	}

	if (!Row->EnemySpawns.IsValidIndex(SelectedSpawnIndex))
	{
		for (int32 Candidate : SelectedSpawnIndices)
		{
			if (Row->EnemySpawns.IsValidIndex(Candidate))
			{
				SelectedSpawnIndex = Candidate;
				break;
			}
		}
		if (!Row->EnemySpawns.IsValidIndex(SelectedSpawnIndex))
		{
			return nullptr;
		}
	}

	return BuildSpawnContextMenu();
}

TSharedRef<SWidget> SInvaderAuthoringPanel::BuildSpawnContextMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.BeginSection("SpawnActions", FText::FromString("Spawn"));
	MenuBuilder.AddMenuEntry(
		FText::FromString("Set Color: Red"),
		FText::FromString("Set selected spawn enemy color to Red."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SInvaderAuthoringPanel::SetSelectedSpawnColor, EAREnemyColor::Red)));
	MenuBuilder.AddMenuEntry(
		FText::FromString("Set Color: Blue"),
		FText::FromString("Set selected spawn enemy color to Blue."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SInvaderAuthoringPanel::SetSelectedSpawnColor, EAREnemyColor::Blue)));
	MenuBuilder.AddMenuEntry(
		FText::FromString("Set Color: White"),
		FText::FromString("Set selected spawn enemy color to White."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SInvaderAuthoringPanel::SetSelectedSpawnColor, EAREnemyColor::White)));
	MenuBuilder.AddMenuSeparator();
	MenuBuilder.AddMenuEntry(
		FText::FromString("Delete Spawn"),
		FText::FromString("Delete selected spawn."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			OnDeleteSelectedSpawn();
		})));
	MenuBuilder.EndSection();
	return MenuBuilder.MakeWidget();
}

void SInvaderAuthoringPanel::ReorderSpawnByDrop(int32 SourceSpawnIndex, int32 TargetSpawnIndex, EItemDropZone DropZone)
{
	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row || !WaveTable || !Row->EnemySpawns.IsValidIndex(SourceSpawnIndex) || !Row->EnemySpawns.IsValidIndex(TargetSpawnIndex))
	{
		return;
	}

	if (SourceSpawnIndex == TargetSpawnIndex)
	{
		return;
	}

	const float SourceDelay = Row->EnemySpawns[SourceSpawnIndex].SpawnDelay;
	const float TargetDelay = Row->EnemySpawns[TargetSpawnIndex].SpawnDelay;
	if (!FMath::IsNearlyEqual(SourceDelay, TargetDelay, LayerMatchTolerance))
	{
		return;
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringDragReorderSpawn", "Reorder Wave Spawn"));
	WaveTable->Modify();

	FARWaveEnemySpawnDef Moved = Row->EnemySpawns[SourceSpawnIndex];
	Row->EnemySpawns.RemoveAt(SourceSpawnIndex);

	int32 InsertIndex = TargetSpawnIndex;
	if (DropZone == EItemDropZone::BelowItem)
	{
		InsertIndex = (SourceSpawnIndex < TargetSpawnIndex) ? TargetSpawnIndex : (TargetSpawnIndex + 1);
	}
	else
	{
		InsertIndex = (SourceSpawnIndex < TargetSpawnIndex) ? (TargetSpawnIndex - 1) : TargetSpawnIndex;
	}
	InsertIndex = FMath::Clamp(InsertIndex, 0, Row->EnemySpawns.Num());
	Row->EnemySpawns.Insert(Moved, InsertIndex);

	SelectedSpawnIndices.Reset();
	SelectedSpawnIndices.Add(InsertIndex);
	SelectedSpawnIndex = InsertIndex;

	MarkTableDirty(WaveTable);
	RefreshLayerItems();
	RefreshSpawnItems();
	SetStatus(FString::Printf(TEXT("Reordered spawn to index %d."), InsertIndex));
}

TSharedPtr<SWidget> SInvaderAuthoringPanel::OnOpenRowContextMenu()
{
	if (!RowListView.IsValid())
	{
		return nullptr;
	}

	const TArray<FName> SelectedRows = GetSelectedRowNames();
	if (SelectedRows.IsEmpty())
	{
		return nullptr;
	}

	return BuildRowContextMenu();
}

TSharedRef<SWidget> SInvaderAuthoringPanel::BuildRowContextMenu()
{
	const TArray<FName> SelectedRows = GetSelectedRowNames();
	const bool bSingleSelection = (SelectedRows.Num() == 1);
	const bool bWaves = (Mode == EAuthoringMode::Waves);
	const FText TypeText = bWaves ? FText::FromString("Wave") : FText::FromString("Stage");
	const FText NameActionText = bWaves ? FText::FromString("Name Wave") : FText::FromString("Name Stage");
	const FText NameActionDescText = bWaves
		? FText::FromString("Set the selected wave name from the Name Wave field.")
		: FText::FromString("Set the selected stage name from the Name Stage field.");

	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.BeginSection("RowActions", TypeText);
	MenuBuilder.AddMenuEntry(
		NameActionText,
		NameActionDescText,
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]()
			{
				OnRenameRow();
			}),
			FCanExecuteAction::CreateLambda([bSingleSelection]() { return bSingleSelection; })));
	MenuBuilder.AddMenuEntry(
		FText::FromString("Duplicate"),
		FText::FromString("Duplicate selected row(s)."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			OnDuplicateRow();
		})));
	MenuBuilder.AddMenuEntry(
		FText::FromString("Delete"),
		FText::FromString("Delete selected row(s)."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			OnDeleteRow();
		})));
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SInvaderAuthoringPanel::MoveSpawnWithinLayer(int32 Direction)
{
	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row || !WaveTable || SelectedSpawnIndices.IsEmpty())
	{
		return;
	}

	TArray<int32> LayerIndices;
	for (int32 Index = 0; Index < Row->EnemySpawns.Num(); ++Index)
	{
		if (FMath::IsNearlyEqual(Row->EnemySpawns[Index].SpawnDelay, SelectedLayerDelay, LayerMatchTolerance))
		{
			LayerIndices.Add(Index);
		}
	}

	TSet<int32> LayerSelection;
	for (int32 Index : LayerIndices)
	{
		if (SelectedSpawnIndices.Contains(Index))
		{
			LayerSelection.Add(Index);
		}
	}
	if (LayerSelection.IsEmpty())
	{
		return;
	}

	FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringReorderSpawn", "Reorder Wave Spawn"));
	WaveTable->Modify();
	bool bAnyMoved = false;

	auto SwapAndTrackSelection = [this, &LayerSelection, Row, &bAnyMoved](int32 A, int32 B)
	{
		if (!Row->EnemySpawns.IsValidIndex(A) || !Row->EnemySpawns.IsValidIndex(B))
		{
			return;
		}

		const bool bASelected = LayerSelection.Contains(A);
		const bool bBSelected = LayerSelection.Contains(B);
		if (bASelected == bBSelected)
		{
			return;
		}

		Row->EnemySpawns.Swap(A, B);
		bAnyMoved = true;
		if (bASelected)
		{
			LayerSelection.Remove(A);
			LayerSelection.Add(B);
		}
		else
		{
			LayerSelection.Remove(B);
			LayerSelection.Add(A);
		}
		if (SelectedSpawnIndex == A)
		{
			SelectedSpawnIndex = B;
		}
		else if (SelectedSpawnIndex == B)
		{
			SelectedSpawnIndex = A;
		}
	};

	if (Direction < 0)
	{
		for (int32 i = 1; i < LayerIndices.Num(); ++i)
		{
			SwapAndTrackSelection(LayerIndices[i], LayerIndices[i - 1]);
		}
	}
	else if (Direction > 0)
	{
		for (int32 i = LayerIndices.Num() - 2; i >= 0; --i)
		{
			SwapAndTrackSelection(LayerIndices[i], LayerIndices[i + 1]);
		}
	}

	if (!bAnyMoved)
	{
		Tx.Cancel();
		return;
	}

	for (int32 Index : LayerIndices)
	{
		SelectedSpawnIndices.Remove(Index);
	}
	for (int32 Index : LayerSelection)
	{
		SelectedSpawnIndices.Add(Index);
	}

	MarkTableDirty(WaveTable);
	RefreshLayerItems();
	RefreshSpawnItems();
}

FReply SInvaderAuthoringPanel::OnMoveSpawnUp()
{
	MoveSpawnWithinLayer(-1);
	return FReply::Handled();
}

FReply SInvaderAuthoringPanel::OnMoveSpawnDown()
{
	MoveSpawnWithinLayer(1);
	return FReply::Handled();
}

void SInvaderAuthoringPanel::UpdateLayerDelay(float OldDelay, float NewDelay)
{
	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row || !WaveTable || FMath::IsNearlyEqual(OldDelay, NewDelay, LayerMatchTolerance))
	{
		return;
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringLayerDelay", "Edit Wave Layer Delay"));
	WaveTable->Modify();
	for (FARWaveEnemySpawnDef& Spawn : Row->EnemySpawns)
	{
		if (FMath::IsNearlyEqual(Spawn.SpawnDelay, OldDelay, LayerMatchTolerance))
		{
			Spawn.SpawnDelay = NewDelay;
		}
	}
	SelectedLayerDelay = NewDelay;
	MarkTableDirty(WaveTable);
	RefreshLayerItems();
	RefreshSpawnItems();
}

void SInvaderAuthoringPanel::HandleCanvasSpawnSelected(int32 SpawnIndex, bool bToggle, bool bRangeSelect)
{
	if (SpawnIndex == INDEX_NONE)
	{
		return;
	}

	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row || !Row->EnemySpawns.IsValidIndex(SpawnIndex))
	{
		return;
	}

	if (bRangeSelect && SelectedSpawnIndex != INDEX_NONE && Row->EnemySpawns.IsValidIndex(SelectedSpawnIndex))
	{
		const float TargetDelay = Row->EnemySpawns[SpawnIndex].SpawnDelay;
		const float AnchorDelay = Row->EnemySpawns[SelectedSpawnIndex].SpawnDelay;
		if (FMath::IsNearlyEqual(TargetDelay, AnchorDelay, LayerMatchTolerance))
		{
			int32 MinIndex = FMath::Min(SelectedSpawnIndex, SpawnIndex);
			int32 MaxIndex = FMath::Max(SelectedSpawnIndex, SpawnIndex);
			TSet<int32> RangeSelection = bToggle ? SelectedSpawnIndices : TSet<int32>();
			for (int32 Index = MinIndex; Index <= MaxIndex; ++Index)
			{
				if (Row->EnemySpawns.IsValidIndex(Index) && FMath::IsNearlyEqual(Row->EnemySpawns[Index].SpawnDelay, TargetDelay, LayerMatchTolerance))
				{
					RangeSelection.Add(Index);
				}
			}
			ApplySpawnSelection(RangeSelection, SpawnIndex);
			return;
		}
	}

	if (bToggle)
	{
		TSet<int32> UpdatedSelection = SelectedSpawnIndices;
		if (UpdatedSelection.Contains(SpawnIndex))
		{
			UpdatedSelection.Remove(SpawnIndex);
		}
		else
		{
			UpdatedSelection.Add(SpawnIndex);
		}
		ApplySpawnSelection(UpdatedSelection, SpawnIndex);
		return;
	}

	if (SelectedSpawnIndices.Contains(SpawnIndex) && SelectedSpawnIndices.Num() > 1)
	{
		// Preserve current multi-selection so drag can move the group.
		ApplySpawnSelection(SelectedSpawnIndices, SpawnIndex);
		return;
	}

	SelectSpawn(SpawnIndex);
}

void SInvaderAuthoringPanel::HandleCanvasClearSpawnSelection()
{
	ClearSpawnSelection();
}

void SInvaderAuthoringPanel::HandleCanvasOpenSpawnContextMenu(int32 SpawnIndex, const FVector2D& ScreenPosition)
{
	SelectSpawn(SpawnIndex);
	TSharedPtr<SWidget> MenuWidget = OnOpenSpawnContextMenu();
	if (MenuWidget.IsValid())
	{
		FSlateApplication::Get().PushMenu(
			AsShared(),
			FWidgetPath(),
			MenuWidget.ToSharedRef(),
			ScreenPosition,
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
	}
}

void SInvaderAuthoringPanel::HandleCanvasBeginSpawnDrag()
{
	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row || !WaveTable)
	{
		return;
	}

	if (SelectedSpawnIndices.IsEmpty() && SelectedSpawnIndex != INDEX_NONE && Row->EnemySpawns.IsValidIndex(SelectedSpawnIndex))
	{
		SelectedSpawnIndices.Add(SelectedSpawnIndex);
	}
	if (SelectedSpawnIndices.IsEmpty())
	{
		return;
	}

	if (SpawnDragTransaction.IsValid())
	{
		return;
	}

	SpawnDragTransaction = MakeUnique<FScopedTransaction>(
		NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringDragSpawn", "Move Wave Spawn"));
	WaveTable->Modify();
	SpawnDragStartOffsets.Reset();
	for (int32 Index : SelectedSpawnIndices)
	{
		if (Row->EnemySpawns.IsValidIndex(Index))
		{
			SpawnDragStartOffsets.Add(Index, Row->EnemySpawns[Index].AuthoredScreenOffset);
		}
	}
	if (SpawnDragStartOffsets.IsEmpty())
	{
		SpawnDragTransaction->Cancel();
		SpawnDragTransaction.Reset();
		return;
	}
	bSpawnDragChanged = false;
}

void SInvaderAuthoringPanel::HandleCanvasEndSpawnDrag()
{
	if (!SpawnDragTransaction.IsValid())
	{
		return;
	}

	if (!bSpawnDragChanged)
	{
		SpawnDragTransaction->Cancel();
	}
	else
	{
		MarkTableDirty(WaveTable);
		RefreshDetailsObjects();
	}

	SpawnDragTransaction.Reset();
	SpawnDragStartOffsets.Reset();
	bSpawnDragChanged = false;
}

void SInvaderAuthoringPanel::HandleCanvasSelectedSpawnsMoved(const FVector2D& OffsetDelta)
{
	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row || !WaveTable || SpawnDragStartOffsets.IsEmpty())
	{
		return;
	}

	bool bAnyChanged = false;
	for (const TPair<int32, FVector2D>& Pair : SpawnDragStartOffsets)
	{
		if (!Row->EnemySpawns.IsValidIndex(Pair.Key))
		{
			continue;
		}
		const FVector2D NewOffset = Pair.Value + OffsetDelta;
		if (!Row->EnemySpawns[Pair.Key].AuthoredScreenOffset.Equals(NewOffset, 0.01f))
		{
			Row->EnemySpawns[Pair.Key].AuthoredScreenOffset = NewOffset;
			bAnyChanged = true;
		}
	}
	if (!bAnyChanged)
	{
		return;
	}

	if (SpawnDragTransaction.IsValid())
	{
		bSpawnDragChanged = true;
	}

	RefreshDetailsObjects();
}

void SInvaderAuthoringPanel::HandleCanvasSelectionRectChanged(const TArray<int32>& RectSelection, bool bAppendToSelection)
{
	TSet<int32> UpdatedSelection = bAppendToSelection ? SelectedSpawnIndices : TSet<int32>();
	for (int32 Index : RectSelection)
	{
		UpdatedSelection.Add(Index);
	}
	ApplySpawnSelection(UpdatedSelection, SelectedSpawnIndex);
}

void SInvaderAuthoringPanel::HandleCanvasAddSpawnAt(const FVector2D& NewOffset)
{
	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row || !WaveTable)
	{
		return;
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringCanvasAddSpawn", "Add Wave Spawn From Canvas"));
	WaveTable->Modify();
	FARWaveEnemySpawnDef Spawn;
	Spawn.SpawnDelay = SelectedLayerDelay;
	Spawn.AuthoredScreenOffset = NewOffset;
	if (ActivePaletteEntry.IsSet())
	{
		UClass* ResolvedClass = ActivePaletteEntry->EnemyClassPath.ResolveClass();
		if (!ResolvedClass)
		{
			ResolvedClass = LoadClass<AAREnemyBase>(nullptr, *ActivePaletteEntry->EnemyClassPath.ToString());
		}
		Spawn.EnemyClass = ResolvedClass;
		Spawn.EnemyColor = ActivePaletteEntry->Color;
	}

	Row->EnemySpawns.Add(Spawn);
	SelectSpawn(Row->EnemySpawns.Num() - 1);
	MarkTableDirty(WaveTable);
	RefreshLayerItems();
	RefreshSpawnItems();
}

void SInvaderAuthoringPanel::SetPreviewTime(float NewPreviewTime)
{
	PreviewTime = FMath::Max(0.f, NewPreviewTime);
	if (WaveCanvas.IsValid())
	{
		WaveCanvas->Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}
}

float SInvaderAuthoringPanel::GetPreviewTime() const
{
	return PreviewTime;
}

float SInvaderAuthoringPanel::GetMaxPreviewTime() const
{
	const FARWaveDefRow* Row = GetSelectedWaveRowConst();
	if (!Row)
	{
		return 1.f;
	}

	float MaxDelay = 0.f;
	for (const FARWaveEnemySpawnDef& Spawn : Row->EnemySpawns)
	{
		MaxDelay = FMath::Max(MaxDelay, Spawn.SpawnDelay);
	}
	return FMath::Max(1.f, MaxDelay + FMath::Max(0.f, Row->EnterDuration + Row->ActiveDuration));
}

FText SInvaderAuthoringPanel::GetPhaseSummaryText() const
{
	const FARWaveDefRow* Row = GetSelectedWaveRowConst();
	if (!Row)
	{
		return FText::FromString(TEXT("No wave selected."));
	}

	const float EnterEnd = FMath::Max(0.f, Row->EnterDuration);
	const float ActiveEnd = EnterEnd + FMath::Max(0.f, Row->ActiveDuration);
	return FText::FromString(FString::Printf(TEXT("Preview %.2fs | Enter [0-%.2f] Active [%.2f-%.2f] Berserk [%.2f-INF]"), PreviewTime, EnterEnd, EnterEnd, ActiveEnd, ActiveEnd));
}

void SInvaderAuthoringPanel::HandleWaveRowPropertiesChanged(const FPropertyChangedEvent& Event)
{
	(void)Event;
	if (bApplyingProxyToModel)
	{
		return;
	}

	if (Mode == EAuthoringMode::Waves)
	{
		FARWaveDefRow* Row = GetSelectedWaveRow();
		if (!Row || !WaveProxy || !WaveTable)
		{
			return;
		}

		const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringEditWaveRow", "Edit Wave Row"));
		WaveTable->Modify();
		*Row = WaveProxy->Row;
		MarkTableDirty(WaveTable);
		RefreshLayerItems();
		RefreshSpawnItems();
	}
	else
	{
		FARStageDefRow* Row = GetSelectedStageRow();
		if (!Row || !StageProxy || !StageTable)
		{
			return;
		}

		const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringEditStageRow", "Edit Stage Row"));
		StageTable->Modify();
		*Row = StageProxy->Row;
		MarkTableDirty(StageTable);
	}
}

void SInvaderAuthoringPanel::HandleSpawnPropertiesChanged(const FPropertyChangedEvent& Event)
{
	(void)Event;
	if (bApplyingProxyToModel)
	{
		return;
	}

	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row || !SpawnProxy || !WaveTable || !Row->EnemySpawns.IsValidIndex(SelectedSpawnIndex))
	{
		return;
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringEditSpawn", "Edit Wave Spawn"));
	WaveTable->Modify();
	Row->EnemySpawns[SelectedSpawnIndex] = SpawnProxy->Spawn;
	MarkTableDirty(WaveTable);
	RefreshLayerItems();
	RefreshSpawnItems();
}

bool SInvaderAuthoringPanel::ShouldShowWaveDetailProperty(const FPropertyAndParent& PropertyAndParent) const
{
	if (Mode != EAuthoringMode::Waves)
	{
		return true;
	}

	const FName PropertyName = PropertyAndParent.Property.GetFName();
	static const TSet<FName> HiddenWaveProperties =
	{
		TEXT("EntryMode"),
		TEXT("BerserkDuration"),
		TEXT("StageTags"),
		TEXT("BannedArchetypeTags")
	};

	return !HiddenWaveProperties.Contains(PropertyName);
}

bool SInvaderAuthoringPanel::ShouldShowSpawnDetailProperty(const FPropertyAndParent& PropertyAndParent) const
{
	if (Mode != EAuthoringMode::Waves)
	{
		return true;
	}

	const FName PropertyName = PropertyAndParent.Property.GetFName();
	static const TSet<FName> HiddenSpawnProperties =
	{
		TEXT("SlotIndex"),
		TEXT("bFormationLockEnter"),
		TEXT("bFormationLockActive")
	};

	return !HiddenSpawnProperties.Contains(PropertyName);
}

TSharedRef<ITableRow> SInvaderAuthoringPanel::OnGenerateRowNameRow(TSharedPtr<FRowNameItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FRowNameItem>>, OwnerTable)
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		.OnMouseButtonDown_Lambda([this, Item](const FGeometry&, const FPointerEvent& MouseEvent)
		{
			if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && Item.IsValid() && RowListView.IsValid())
			{
				RowListView->SetSelection(Item, ESelectInfo::Direct);
			}
			return FReply::Unhandled();
		})
		[
			SNew(STextBlock).Text(FText::FromName(Item.IsValid() ? Item->RowName : NAME_None))
		]
	];
}

TSharedRef<ITableRow> SInvaderAuthoringPanel::OnGenerateLayerRow(TSharedPtr<FLayerItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const FString Label = Item.IsValid() ? FString::Printf(TEXT("Delay %.2fs (%d spawns)"), Item->Delay, Item->Count) : TEXT("<invalid>");
	return SNew(STableRow<TSharedPtr<FLayerItem>>, OwnerTable)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			SNew(STextBlock).Text(FText::FromString(Label))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
		[
			SNew(SSpinBox<float>)
			.MinValue(0.f)
			.MaxValue(999.f)
			.Delta(0.1f)
			.Value(Item.IsValid() ? Item->Delay : 0.f)
			.OnValueCommitted_Lambda([this, Item](float NewValue, ETextCommit::Type)
			{
				if (Item.IsValid())
				{
					UpdateLayerDelay(Item->Delay, NewValue);
				}
			})
		]
	];
}

TSharedRef<ITableRow> SInvaderAuthoringPanel::OnGenerateSpawnRow(TSharedPtr<FSpawnItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const FString Label = Item.IsValid()
		? FString::Printf(TEXT("#%d  %s  %s"), Item->SpawnIndex, *EnemyColorToName(Item->Color), *Item->EnemyClassName)
		: TEXT("<invalid>");
	return SNew(STableRow<TSharedPtr<FSpawnItem>>, OwnerTable)
		.OnDragDetected_Lambda([Item, Label](const FGeometry&, const FPointerEvent& MouseEvent)
		{
			if (!Item.IsValid() || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
			{
				return FReply::Unhandled();
			}
			return FReply::Handled().BeginDragDrop(FInvaderSpawnDragDropOp::New(Item->SpawnIndex, Label));
		})
		.OnCanAcceptDrop_Lambda([this, Item](const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FSpawnItem>)
		{
			(void)DropZone;
			if (!Item.IsValid())
			{
				return TOptional<EItemDropZone>();
			}

			const TSharedPtr<FInvaderSpawnDragDropOp> DragOp = DragDropEvent.GetOperationAs<FInvaderSpawnDragDropOp>();
			if (!DragOp.IsValid() || DragOp->SourceSpawnIndex == Item->SpawnIndex)
			{
				return TOptional<EItemDropZone>();
			}

			FARWaveDefRow* Row = GetSelectedWaveRow();
			if (!Row || !Row->EnemySpawns.IsValidIndex(DragOp->SourceSpawnIndex) || !Row->EnemySpawns.IsValidIndex(Item->SpawnIndex))
			{
				return TOptional<EItemDropZone>();
			}

			const float SourceDelay = Row->EnemySpawns[DragOp->SourceSpawnIndex].SpawnDelay;
			const float TargetDelay = Row->EnemySpawns[Item->SpawnIndex].SpawnDelay;
			if (!FMath::IsNearlyEqual(SourceDelay, TargetDelay, LayerMatchTolerance))
			{
				return TOptional<EItemDropZone>();
			}

			return TOptional<EItemDropZone>(DropZone);
		})
		.OnAcceptDrop_Lambda([this, Item](const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FSpawnItem>)
		{
			if (!Item.IsValid())
			{
				return FReply::Unhandled();
			}

			const TSharedPtr<FInvaderSpawnDragDropOp> DragOp = DragDropEvent.GetOperationAs<FInvaderSpawnDragDropOp>();
			if (!DragOp.IsValid())
			{
				return FReply::Unhandled();
			}

			ReorderSpawnByDrop(DragOp->SourceSpawnIndex, Item->SpawnIndex, DropZone);
			return FReply::Handled();
		})
	[
		SNew(STextBlock).Text(FText::FromString(Label))
	];
}

TSharedRef<ITableRow> SInvaderAuthoringPanel::OnGenerateIssueRow(TSharedPtr<FInvaderAuthoringIssue> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	const FString Label = Item.IsValid()
		? FString::Printf(TEXT("[%s] %s | %s | %s"), Item->bError ? TEXT("Error") : TEXT("Warn"), Item->bWave ? TEXT("Wave") : TEXT("Stage"), *Item->RowName.ToString(), *Item->Message)
		: TEXT("<invalid>");
	return SNew(STableRow<TSharedPtr<FInvaderAuthoringIssue>>, OwnerTable)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Label))
		.ColorAndOpacity(Item.IsValid() && Item->bError ? FLinearColor::Red : FLinearColor(1.f, 0.85f, 0.2f, 1.f))
	];
}

void SInvaderAuthoringPanel::HandleRowListSelectionChanged(TSharedPtr<FRowNameItem> Item, ESelectInfo::Type)
{
	if (!Item.IsValid())
	{
		return;
	}

	if (Mode == EAuthoringMode::Waves)
	{
		SelectWaveRow(Item->RowName);
	}
	else
	{
		SelectStageRow(Item->RowName);
	}
}

void SInvaderAuthoringPanel::HandleLayerSelectionChanged(TSharedPtr<FLayerItem> Item, ESelectInfo::Type)
{
	if (!Item.IsValid())
	{
		return;
	}
	SelectLayerByDelay(Item->Delay);
}

void SInvaderAuthoringPanel::HandleSpawnSelectionChanged(TSharedPtr<FSpawnItem> Item, ESelectInfo::Type)
{
	if (bSyncingSpawnSelection)
	{
		return;
	}

	TSet<int32> NewSelection;
	if (SpawnListView.IsValid())
	{
		const TArray<TSharedPtr<FSpawnItem>> ListSelection = SpawnListView->GetSelectedItems();
		for (const TSharedPtr<FSpawnItem>& SelectedItem : ListSelection)
		{
			if (SelectedItem.IsValid())
			{
				NewSelection.Add(SelectedItem->SpawnIndex);
			}
		}
	}

	ApplySpawnSelection(NewSelection, Item.IsValid() ? Item->SpawnIndex : SelectedSpawnIndex);
}

void SInvaderAuthoringPanel::HandleIssueSelectionChanged(TSharedPtr<FInvaderAuthoringIssue> Item, ESelectInfo::Type)
{
	if (!Item.IsValid())
	{
		return;
	}

	SetMode(Item->bWave ? EAuthoringMode::Waves : EAuthoringMode::Stages);
	if (Item->bWave)
	{
		SelectWaveRow(Item->RowName);
	}
	else
	{
		SelectStageRow(Item->RowName);
	}
}

void SInvaderAuthoringPanel::ToggleFavoriteClass(const FSoftClassPath& ClassPath)
{
	UARInvaderAuthoringEditorSettings* Settings = GetMutableDefault<UARInvaderAuthoringEditorSettings>();
	if (Settings->FavoriteEnemyClasses.Contains(ClassPath))
	{
		Settings->FavoriteEnemyClasses.Remove(ClassPath);
	}
	else
	{
		Settings->FavoriteEnemyClasses.Add(ClassPath);
	}
	Settings->SaveConfig();
	RefreshPalette();
}

int32 SInvaderAuthoringPanel::GetPaletteShapeCycle(const FSoftClassPath& ClassPath) const
{
	const UARInvaderAuthoringEditorSettings* Settings = GetDefault<UARInvaderAuthoringEditorSettings>();
	if (!Settings)
	{
		return 0;
	}
	if (const int32* SavedShape = Settings->EnemyClassShapeCycles.Find(ClassPath))
	{
		return *SavedShape;
	}
	return 0;
}

void SInvaderAuthoringPanel::CyclePaletteShape(const FSoftClassPath& ClassPath)
{
	UARInvaderAuthoringEditorSettings* Settings = GetMutableDefault<UARInvaderAuthoringEditorSettings>();
	const int32 CurrentShape = GetPaletteShapeCycle(ClassPath);
	const int32 NextShape = (CurrentShape + 1) % static_cast<int32>(EPaletteShape::Count);
	Settings->EnemyClassShapeCycles.Add(ClassPath, NextShape);
	Settings->SaveConfig();
	RefreshPalette();
	if (WaveCanvas.IsValid())
	{
		WaveCanvas->Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}
	SetStatus(FString::Printf(TEXT("Palette shape set: %s -> %s"), *MakePaletteClassDisplayName(ClassPath), *PaletteShapeShortName(NextShape)));
}

void SInvaderAuthoringPanel::SyncPaletteClassInContentBrowser(const FSoftClassPath& ClassPath)
{
	if (!GEditor)
	{
		return;
	}

	FString AssetObjectPath = ClassPath.ToString();
	if (AssetObjectPath.IsEmpty())
	{
		return;
	}

	AssetObjectPath.RemoveFromEnd(TEXT("_C"));
	UObject* AssetObject = LoadObject<UObject>(nullptr, *AssetObjectPath);
	if (!AssetObject)
	{
		SetStatus(FString::Printf(TEXT("Could not locate asset for '%s'."), *MakePaletteClassDisplayName(ClassPath)));
		return;
	}

	TArray<UObject*> AssetsToSync;
	AssetsToSync.Add(AssetObject);
	GEditor->SyncBrowserToObjects(AssetsToSync);
	SetStatus(FString::Printf(TEXT("Synced Content Browser to '%s'."), *AssetObject->GetName()));
}

void SInvaderAuthoringPanel::SetActivePaletteEntry(const FPaletteEntry& Entry)
{
	ActivePaletteEntry = Entry;
	SetStatus(FString::Printf(TEXT("Palette selected: %s"), *Entry.Label.ToString()));
}

TSharedRef<SWidget> SInvaderAuthoringPanel::BuildPaletteWidget()
{
	struct FPaletteClassRow
	{
		FSoftClassPath ClassPath;
		bool bFavorite = false;
		int32 ShapeCycle = 0;
	};

	TArray<FPaletteClassRow> PaletteClassRows;
	TSet<FSoftClassPath> AddedClassPaths;
	for (const FPaletteEntry& Entry : PaletteEntries)
	{
		if (AddedClassPaths.Contains(Entry.EnemyClassPath))
		{
			continue;
		}

		AddedClassPaths.Add(Entry.EnemyClassPath);
		FPaletteClassRow& NewRow = PaletteClassRows.AddDefaulted_GetRef();
		NewRow.ClassPath = Entry.EnemyClassPath;
		NewRow.bFavorite = Entry.bFavorite;
		NewRow.ShapeCycle = Entry.ShapeCycle;
	}

	PaletteClassRows.Sort([](const FPaletteClassRow& A, const FPaletteClassRow& B)
	{
		if (A.bFavorite != B.bFavorite)
		{
			return A.bFavorite && !B.bFavorite;
		}
		return A.ClassPath.ToString() < B.ClassPath.ToString();
	});

	auto SelectPalette = [this](const FSoftClassPath& ClassPath, EAREnemyColor Color)
	{
		if (const FPaletteEntry* FoundEntry = PaletteEntries.FindByPredicate(
			[&ClassPath, Color](const FPaletteEntry& Entry)
			{
				return Entry.EnemyClassPath == ClassPath && Entry.Color == Color;
			}))
		{
			SetActivePaletteEntry(*FoundEntry);
			return;
		}

		FPaletteEntry FallbackEntry;
		FallbackEntry.EnemyClassPath = ClassPath;
		FallbackEntry.Color = Color;
		FallbackEntry.ShapeCycle = GetPaletteShapeCycle(ClassPath);
		FallbackEntry.Label = FText::FromString(
			FString::Printf(TEXT("%s %s"), *MakePaletteClassDisplayName(ClassPath), *EnemyColorToName(Color)));
		SetActivePaletteEntry(FallbackEntry);
	};

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	for (const FPaletteClassRow& Row : PaletteClassRows)
	{
		const FSoftClassPath ClassPath = Row.ClassPath;
		const FString ClassName = MakePaletteClassDisplayName(ClassPath);
		const bool bFavorite = Row.bFavorite;
		const int32 ShapeCycle = Row.ShapeCycle;

		Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
		[
			SNew(SBorder)
			.OnMouseButtonDown_Lambda([this, ClassPath](const FGeometry&, const FPointerEvent& MouseEvent)
			{
				if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
				{
					FMenuBuilder MenuBuilder(true, nullptr);
					MenuBuilder.AddMenuEntry(
						FText::FromString(TEXT("Find in Content Browser")),
						FText::FromString(TEXT("Sync Content Browser to this enemy Blueprint.")),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([this, ClassPath]()
						{
							SyncPaletteClassInContentBrowser(ClassPath);
						})));
					FSlateApplication::Get().PushMenu(
						AsShared(),
						FWidgetPath(),
						MenuBuilder.MakeWidget(),
						MouseEvent.GetScreenSpacePosition(),
						FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
					return FReply::Handled();
				}
				return FReply::Unhandled();
			})
			[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
				[
#if 0
					SNew(SButton)
					.Text(FText::FromString(bFavorite ? TEXT("★") : TEXT("☆")))
					.OnClicked_Lambda([this, ClassPath]()
					{
						ToggleFavoriteClass(ClassPath);
						return FReply::Handled();
					})
#endif
					SNew(SButton)
					.Text(FText::FromString(bFavorite ? TEXT("★") : TEXT("☆")))
					.ToolTipText(FText::FromString("Toggle favorite"))
					.OnClicked_Lambda([this, ClassPath]()
					{
						ToggleFavoriteClass(ClassPath);
						return FReply::Handled();
					})
				]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.ToolTipText(FText::FromString("Cycle shape"))
				.Text(FText::FromString(PaletteShapeGlyph(ShapeCycle)))
				.OnClicked_Lambda([this, ClassPath]()
				{
					CyclePaletteShape(ClassPath);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(ClassName))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.ButtonColorAndOpacity(GetEnemyColor(EAREnemyColor::Red))
				.Text(FText::FromString("R"))
				.OnClicked_Lambda([this, ClassPath, SelectPalette]()
				{
					SelectPalette(ClassPath, EAREnemyColor::Red);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.ButtonColorAndOpacity(GetEnemyColor(EAREnemyColor::Blue))
				.Text(FText::FromString("B"))
				.OnClicked_Lambda([this, ClassPath, SelectPalette]()
				{
					SelectPalette(ClassPath, EAREnemyColor::Blue);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.ButtonColorAndOpacity(GetEnemyColor(EAREnemyColor::White))
				.Text(FText::FromString("W"))
				.OnClicked_Lambda([this, ClassPath, SelectPalette]()
				{
					SelectPalette(ClassPath, EAREnemyColor::White);
					return FReply::Handled();
				})
			]
			]
		];
	}

	return SNew(SScrollBox)
	+ SScrollBox::Slot()
	[
		Box
	];
}

void SInvaderAuthoringPanel::RefreshPalette()
{
	PaletteEntries.Reset();

	TSet<FSoftClassPath> UniqueClasses;
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	const UARInvaderToolingSettings* ToolingSettings = GetDefault<UARInvaderToolingSettings>();
	FString EnemyFolder = ToolingSettings ? ToolingSettings->EnemiesFolder.Path : FString();
	if (EnemyFolder.IsEmpty())
	{
		EnemyFolder = TEXT("/Game");
	}
	if (!EnemyFolder.StartsWith(TEXT("/")))
	{
		EnemyFolder = FString(TEXT("/")) + EnemyFolder;
	}
	EnemyFolder.RemoveFromEnd(TEXT("/"));
	Filter.PackagePaths.Add(*EnemyFolder);
	Filter.bRecursivePaths = true;

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);
	for (const FAssetData& Asset : Assets)
	{
		FString GeneratedClassPath;
		if (!Asset.GetTagValue(TEXT("GeneratedClass"), GeneratedClassPath))
		{
			continue;
		}

		const FString ObjectPath = FPackageName::ExportTextPathToObjectPath(GeneratedClassPath);
		if (ObjectPath.IsEmpty())
		{
			continue;
		}

		FSoftClassPath ClassPath(ObjectPath);
		bool bIsCompatibleEnemyClass = false;
		if (const bool* CachedCompatibility = EnemyPaletteClassCompatibilityCache.Find(ClassPath))
		{
			bIsCompatibleEnemyClass = *CachedCompatibility;
		}
		else
		{
			UClass* LoadedClass = ClassPath.ResolveClass();
			if (!LoadedClass)
			{
				LoadedClass = LoadClass<AAREnemyBase>(nullptr, *ClassPath.ToString());
			}

			bIsCompatibleEnemyClass = IsPaletteClassSupported(LoadedClass);
			EnemyPaletteClassCompatibilityCache.Add(ClassPath, bIsCompatibleEnemyClass);
		}

		if (bIsCompatibleEnemyClass)
		{
			UniqueClasses.Add(ClassPath);
		}
	}

	TArray<FSoftClassPath> SortedClasses = UniqueClasses.Array();
	SortedClasses.Sort([](const FSoftClassPath& A, const FSoftClassPath& B)
	{
		return A.ToString() < B.ToString();
	});

	const TSet<FSoftClassPath> Favorites(GetDefault<UARInvaderAuthoringEditorSettings>()->FavoriteEnemyClasses);
	for (const FSoftClassPath& ClassPath : SortedClasses)
	{
		for (EAREnemyColor Color : { EAREnemyColor::Red, EAREnemyColor::Blue, EAREnemyColor::White })
		{
			FPaletteEntry Entry;
			Entry.EnemyClassPath = ClassPath;
			Entry.Color = Color;
			Entry.bFavorite = Favorites.Contains(ClassPath);
			Entry.ShapeCycle = GetPaletteShapeCycle(ClassPath);
			Entry.Label = FText::FromString(FString::Printf(TEXT("%s %s"), *MakePaletteClassDisplayName(ClassPath), *EnemyColorToName(Color)));
			PaletteEntries.Add(Entry);
		}
	}

	PaletteEntries.Sort([](const FPaletteEntry& A, const FPaletteEntry& B)
	{
		if (A.bFavorite != B.bFavorite)
		{
			return A.bFavorite && !B.bFavorite;
		}
		if (A.EnemyClassPath != B.EnemyClassPath)
		{
			return A.EnemyClassPath.ToString() < B.EnemyClassPath.ToString();
		}
		return static_cast<uint8>(A.Color) < static_cast<uint8>(B.Color);
	});

	if (PaletteHost.IsValid())
	{
		PaletteHost->SetContent(BuildPaletteWidget());
	}
}

FReply SInvaderAuthoringPanel::OnValidateSelected()
{
	TArray<FInvaderAuthoringIssue> Issues;
	ValidateSelected(Issues);
	IssueItems.Reset();
	for (const FInvaderAuthoringIssue& Issue : Issues)
	{
		IssueItems.Add(MakeShared<FInvaderAuthoringIssue>(Issue));
	}
	RefreshIssues();
	SetStatus(FString::Printf(TEXT("Validation (selected) produced %d issues."), IssueItems.Num()));
	return FReply::Handled();
}

FReply SInvaderAuthoringPanel::OnValidateAll()
{
	TArray<FInvaderAuthoringIssue> Issues;
	ValidateAll(Issues);
	IssueItems.Reset();
	for (const FInvaderAuthoringIssue& Issue : Issues)
	{
		IssueItems.Add(MakeShared<FInvaderAuthoringIssue>(Issue));
	}
	RefreshIssues();
	SetStatus(FString::Printf(TEXT("Validation (all) produced %d issues."), IssueItems.Num()));
	return FReply::Handled();
}

void SInvaderAuthoringPanel::ValidateSelected(TArray<FInvaderAuthoringIssue>& OutIssues) const
{
	if (!WaveTable || !StageTable)
	{
		OutIssues.Add({ true, Mode == EAuthoringMode::Waves, NAME_None, TEXT("Missing Wave/Stage table references in director settings.") });
		return;
	}

	if (Mode == EAuthoringMode::Waves)
	{
		const FARWaveDefRow* Row = WaveTable ? WaveTable->FindRow<FARWaveDefRow>(SelectedWaveRow, TEXT("ValidateSelected"), false) : nullptr;
		if (Row)
		{
			ValidateWaveRow(SelectedWaveRow, *Row, OutIssues);
		}
	}
	else
	{
		const FARStageDefRow* Row = StageTable ? StageTable->FindRow<FARStageDefRow>(SelectedStageRow, TEXT("ValidateSelected"), false) : nullptr;
		if (Row)
		{
			ValidateStageRow(SelectedStageRow, *Row, WaveTable, OutIssues);
		}
	}
}

void SInvaderAuthoringPanel::ValidateAll(TArray<FInvaderAuthoringIssue>& OutIssues) const
{
	if (!WaveTable || !StageTable)
	{
		OutIssues.Add({ true, true, NAME_None, TEXT("Missing Wave/Stage table references in director settings.") });
		return;
	}

	for (const TPair<FName, uint8*>& Pair : WaveTable->GetRowMap())
	{
		const FARWaveDefRow* Row = reinterpret_cast<const FARWaveDefRow*>(Pair.Value);
		if (Row)
		{
			ValidateWaveRow(Pair.Key, *Row, OutIssues);
		}
	}

	for (const TPair<FName, uint8*>& Pair : StageTable->GetRowMap())
	{
		const FARStageDefRow* Row = reinterpret_cast<const FARStageDefRow*>(Pair.Value);
		if (Row)
		{
			ValidateStageRow(Pair.Key, *Row, WaveTable, OutIssues);
		}
	}
}

void SInvaderAuthoringPanel::ValidateWaveRow(const FName RowName, const FARWaveDefRow& Row, TArray<FInvaderAuthoringIssue>& OutIssues) const
{
	if (Row.MinThreat > Row.MaxThreat)
	{
		OutIssues.Add({ true, true, RowName, TEXT("MinThreat is greater than MaxThreat.") });
	}
	if (Row.MinPlayers > Row.MaxPlayers)
	{
		OutIssues.Add({ true, true, RowName, TEXT("MinPlayers is greater than MaxPlayers.") });
	}
	if (Row.SelectionWeight <= 0)
	{
		OutIssues.Add({ false, true, RowName, TEXT("SelectionWeight <= 0. Runtime clamps to at least 1.") });
	}
	if (Row.RepeatWeightPenalty < 0.01f || Row.RepeatWeightPenalty > 1.f)
	{
		OutIssues.Add({ false, true, RowName, TEXT("RepeatWeightPenalty is outside runtime clamp range [0.01, 1.0].") });
	}
	for (int32 i = 0; i < Row.EnemySpawns.Num(); ++i)
	{
		if (!Row.EnemySpawns[i].EnemyClass)
		{
			OutIssues.Add({ true, true, RowName, FString::Printf(TEXT("Spawn index %d has no EnemyClass."), i) });
		}
	}
}

void SInvaderAuthoringPanel::ValidateStageRow(const FName RowName, const FARStageDefRow& Row, const UDataTable* WavesTable, TArray<FInvaderAuthoringIssue>& OutIssues) const
{
	if (Row.SelectionWeight <= 0.f)
	{
		OutIssues.Add({ false, false, RowName, TEXT("SelectionWeight <= 0. Runtime clamps to 0.01.") });
	}

	if (!Row.BannedArchetypeTags.IsEmpty())
	{
		OutIssues.Add({ false, false, RowName, TEXT("BannedArchetypeTags is authored but not currently enforced by runtime selection.") });
	}

	bool bAnyCompatibleWave = false;
	if (WavesTable)
	{
		for (const TPair<FName, uint8*>& Pair : WavesTable->GetRowMap())
		{
			const FARWaveDefRow* Wave = reinterpret_cast<const FARWaveDefRow*>(Pair.Value);
			if (!Wave)
			{
				continue;
			}

			if (!Row.RequiredWaveTags.IsEmpty() && !Wave->WaveTags.HasAll(Row.RequiredWaveTags))
			{
				continue;
			}
			if (!Row.BlockedWaveTags.IsEmpty() && Wave->WaveTags.HasAny(Row.BlockedWaveTags))
			{
				continue;
			}

			bAnyCompatibleWave = true;
			break;
		}
	}

	if (!bAnyCompatibleWave)
	{
		OutIssues.Add({ true, false, RowName, TEXT("No compatible wave rows match Required/Blocked wave tag constraints.") });
	}
}

void SInvaderAuthoringPanel::RefreshIssues()
{
	if (IssueListView.IsValid())
	{
		IssueListView->RequestListRefresh();
	}
	if (ValidationIssuesArea.IsValid() && IssueItems.IsEmpty())
	{
		ValidationIssuesArea->SetExpanded(false);
	}
}

void SInvaderAuthoringPanel::HandleObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event)
{
	(void)Event;
	if (!Object)
	{
		return;
	}

	if (Object != WaveTable && Object != StageTable)
	{
		return;
	}

	RefreshRowItems();
	RefreshDetailsObjects();
	RefreshIssues();
	if (WaveCanvas.IsValid())
	{
		WaveCanvas->Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}
}

FReply SInvaderAuthoringPanel::OnStartOrAttachPIE()
{
	if (GetPIEWorld())
	{
		SetStatus(TEXT("Attached to existing PIE session."));
		SchedulePIESaveBootstrap();
		return FReply::Handled();
	}

	if (EnsurePIESession(true))
	{
		SetStatus(TEXT("PIE launch requested..."));
		SchedulePIESaveBootstrap();
	}
	else
	{
		SetStatus(TEXT("Could not start or attach PIE session."));
	}
	return FReply::Handled();
}

FReply SInvaderAuthoringPanel::OnStopPIE()
{
	bPendingPIESaveBootstrap = false;
	PIESaveLoadedBridge.Reset();
	if (GEditor)
	{
		GEditor->RequestEndPlayMap();
		SetStatus(TEXT("PIE stop requested."));
	}
	return FReply::Handled();
}

FReply SInvaderAuthoringPanel::OnStartRun()
{
	if (!EnsurePIESession(true))
	{
		SetStatus(TEXT("No PIE session available."));
		return FReply::Handled();
	}
	if (!GetPIEWorld())
	{
		SetStatus(TEXT("PIE launch requested. Run again once PIE is active."));
		return FReply::Handled();
	}
	if (!GetPIEWorld()->GetAuthGameMode())
	{
		SetStatus(TEXT("PIE server world is not ready yet. Try Start Run again in a moment."));
		return FReply::Handled();
	}

	UARInvaderAuthoringEditorSettings* Settings = GetMutableDefault<UARInvaderAuthoringEditorSettings>();
	if (ExecPIECommand(FString::Printf(TEXT("ar.invader.start %d"), Settings->LastSeed)))
	{
		SetStatus(FString::Printf(TEXT("Started invader run with seed %d."), Settings->LastSeed));
	}
	return FReply::Handled();
}

FReply SInvaderAuthoringPanel::OnStopRun()
{
	if (ExecPIECommand(TEXT("ar.invader.stop")))
	{
		SetStatus(TEXT("Stopped invader run."));
	}
	return FReply::Handled();
}

FReply SInvaderAuthoringPanel::OnForceStage()
{
	if (SelectedStageRow.IsNone())
	{
		SetStatus(TEXT("Select a stage row first."));
		return FReply::Handled();
	}
	if (ExecPIECommand(FString::Printf(TEXT("ar.invader.force_stage %s"), *SelectedStageRow.ToString())))
	{
		SetStatus(FString::Printf(TEXT("Forced stage '%s'."), *SelectedStageRow.ToString()));
	}
	return FReply::Handled();
}

FReply SInvaderAuthoringPanel::OnForceWave()
{
	if (SelectedWaveRow.IsNone())
	{
		SetStatus(TEXT("Select a wave row first."));
		return FReply::Handled();
	}
	if (ExecPIECommand(FString::Printf(TEXT("ar.invader.force_wave %s"), *SelectedWaveRow.ToString())))
	{
		SetStatus(FString::Printf(TEXT("Forced wave '%s'."), *SelectedWaveRow.ToString()));
	}
	return FReply::Handled();
}

FReply SInvaderAuthoringPanel::OnForceThreat()
{
	if (ThreatTextBox.IsValid())
	{
		LexFromString(ForcedThreatValue, *ThreatTextBox->GetText().ToString());
	}
	if (ExecPIECommand(FString::Printf(TEXT("ar.invader.force_threat %.2f"), ForcedThreatValue)))
	{
		SetStatus(FString::Printf(TEXT("Forced threat %.2f."), ForcedThreatValue));
	}
	return FReply::Handled();
}

FReply SInvaderAuthoringPanel::OnDumpState()
{
	if (ExecPIECommand(TEXT("ar.invader.dump_state")))
	{
		SetStatus(TEXT("Dumped invader runtime state."));
	}
	return FReply::Handled();
}

bool SInvaderAuthoringPanel::EnsurePIESession(bool bStartIfNeeded)
{
	if (GetPIEWorld())
	{
		return true;
	}

	if (!bStartIfNeeded || !GEditor)
	{
		return false;
	}

	UARInvaderAuthoringEditorSettings* Settings = GetMutableDefault<UARInvaderAuthoringEditorSettings>();
	const UARInvaderToolingSettings* ToolingSettings = GetDefault<UARInvaderToolingSettings>();
	const bool bUseBootstrapLoadingMap = ToolingSettings
		&& ToolingSettings->bEnablePIESaveBootstrap
		&& !ToolingSettings->PIEBootstrapLoadingMap.IsNull();
	const TSoftObjectPtr<UWorld> StartupMap = bUseBootstrapLoadingMap
		? ToolingSettings->PIEBootstrapLoadingMap
		: Settings->DefaultTestMap;

	if (!StartupMap.IsNull())
	{
		const FSoftObjectPath MapPath = StartupMap.ToSoftObjectPath();
		FString MapPackageName = MapPath.GetLongPackageName();
		if (MapPackageName.IsEmpty())
		{
			MapPackageName = StartupMap.ToString();
		}
		if (!UEditorLoadingAndSavingUtils::LoadMap(MapPackageName))
		{
			SetStatus(FString::Printf(TEXT("Failed to load default test map '%s'."), *MapPackageName));
			return false;
		}
	}

	ULevelEditorPlaySettings* PlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
	PlaySettings->SetPlayNetMode(EPlayNetMode::PIE_ListenServer);
	PlaySettings->SetPlayNumberOfClients(1);
	PlaySettings->SetRunUnderOneProcess(true);
	PlaySettings->SaveConfig();

	FRequestPlaySessionParams Params;
	GEditor->RequestPlaySession(Params);
	return true;
}

void SInvaderAuthoringPanel::SchedulePIESaveBootstrap()
{
	const UARInvaderToolingSettings* ToolingSettings = GetDefault<UARInvaderToolingSettings>();
	if (!ToolingSettings || !ToolingSettings->bEnablePIESaveBootstrap)
	{
		return;
	}
	if (bPendingPIESaveBootstrap)
	{
		return;
	}

	bPendingPIESaveBootstrap = true;
	const TWeakPtr<SInvaderAuthoringPanel> WeakPanel = SharedThis(this);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPanel](float)
	{
		const TSharedPtr<SInvaderAuthoringPanel> Pinned = WeakPanel.Pin();
		if (!Pinned.IsValid())
		{
			return false;
		}

		if (!Pinned->bPendingPIESaveBootstrap)
		{
			return false;
		}

		if (!Pinned->GetPIEWorld())
		{
			return true;
		}

		Pinned->RunPIESaveBootstrap();
		Pinned->bPendingPIESaveBootstrap = false;
		return false;
	}), 0.2f);
}

bool SInvaderAuthoringPanel::RunPIESaveBootstrap()
{
	UWorld* PIEWorld = GetPIEWorld();
	if (!PIEWorld)
	{
		return false;
	}

	const UARInvaderToolingSettings* ToolingSettings = GetDefault<UARInvaderToolingSettings>();
	if (!ToolingSettings || !ToolingSettings->bEnablePIESaveBootstrap)
	{
		return false;
	}

	UGameInstance* GI = PIEWorld->GetGameInstance();
	if (!GI)
	{
		SetStatus(TEXT("PIE save bootstrap failed: no GameInstance."));
		return false;
	}

	UFunction* LoadFn = GI->FindFunction(FName(TEXT("LoadSave")));
	if (!LoadFn)
	{
		SetStatus(TEXT("PIE save bootstrap failed: GameInstance function 'LoadSave' not found."));
		return false;
	}

	TArray<uint8> ParamsBuffer;
	void* ParamsPtr = nullptr;
	if (LoadFn->ParmsSize > 0)
	{
		ParamsBuffer.SetNumZeroed(LoadFn->ParmsSize);
		ParamsPtr = ParamsBuffer.GetData();

		static const FName SlotNameParamName(TEXT("SlotName"));
		static const FName SlotNumberParamName(TEXT("SlotNumber"));

		for (TFieldIterator<FProperty> It(LoadFn); It; ++It)
		{
			FProperty* ParamProp = *It;
			if (!ParamProp || !ParamProp->HasAnyPropertyFlags(CPF_Parm) || ParamProp->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				continue;
			}

			void* ValuePtr = ParamProp->ContainerPtrToValuePtr<void>(ParamsPtr);
			if (!ValuePtr)
			{
				continue;
			}

			if (ParamProp->GetFName() == SlotNameParamName)
			{
				if (FNameProperty* NameProp = CastField<FNameProperty>(ParamProp))
				{
					NameProp->SetPropertyValue(ValuePtr, ToolingSettings->PIELoadSlotName);
				}
				else if (FStrProperty* StrProp = CastField<FStrProperty>(ParamProp))
				{
					StrProp->SetPropertyValue(ValuePtr, ToolingSettings->PIELoadSlotName.ToString());
				}
			}
			else if (ParamProp->GetFName() == SlotNumberParamName)
			{
				if (FIntProperty* IntProp = CastField<FIntProperty>(ParamProp))
				{
					IntProp->SetPropertyValue(ValuePtr, ToolingSettings->PIELoadSlotNumber);
				}
				else if (FInt64Property* Int64Prop = CastField<FInt64Property>(ParamProp))
				{
					Int64Prop->SetPropertyValue(ValuePtr, static_cast<int64>(ToolingSettings->PIELoadSlotNumber));
				}
			}
		}
	}

	FString DebugMapPackageName;
	const TSoftObjectPtr<UWorld> DebugMap = !ToolingSettings->PIEBootstrapDebugMap.IsNull()
		? ToolingSettings->PIEBootstrapDebugMap
		: GetDefault<UARInvaderAuthoringEditorSettings>()->DefaultTestMap;
	if (!DebugMap.IsNull())
	{
		const FSoftObjectPath DebugMapPath = DebugMap.ToSoftObjectPath();
		DebugMapPackageName = DebugMapPath.GetLongPackageName();
		if (DebugMapPackageName.IsEmpty())
		{
			DebugMapPackageName = DebugMap.ToString();
		}
	}

	if (DebugMapPackageName.IsEmpty())
	{
		SetStatus(TEXT("PIE save bootstrap loaded save (no debug travel map set)."));
		return true;
	}

	const TWeakPtr<SInvaderAuthoringPanel> WeakPanel = SharedThis(this);
	const TWeakObjectPtr<UGameInstance> WeakGI = GI;
	const FString TravelMap = DebugMapPackageName;
	const TSharedRef<bool> bTravelIssued = MakeShared<bool>(false);
	auto TryTravel = [WeakPanel, TravelMap, bTravelIssued](const FString& ReasonPrefix) -> bool
	{
		if (*bTravelIssued)
		{
			return true;
		}

		const TSharedPtr<SInvaderAuthoringPanel> Pinned = WeakPanel.Pin();
		if (!Pinned.IsValid())
		{
			return false;
		}

		UWorld* CurrentPIEWorld = Pinned->GetPIEWorld();
		if (!CurrentPIEWorld)
		{
			return false;
		}

		UGameplayStatics::OpenLevel(CurrentPIEWorld, FName(*TravelMap), true);
		Pinned->SetStatus(FString::Printf(TEXT("%s opening level '%s'."), *ReasonPrefix, *TravelMap));
		Pinned->PIESaveLoadedBridge.Reset();
		*bTravelIssued = true;
		return true;
	};

	bool bSubscribedToSignal = false;
	if (FMulticastDelegateProperty* LoadedSignalProperty = FindFProperty<FMulticastDelegateProperty>(GI->GetClass(), FName(TEXT("SignalOnGameLoaded"))))
	{
		UARInvaderPIESaveLoadedBridge* Bridge = NewObject<UARInvaderPIESaveLoadedBridge>(GetTransientPackage());
		Bridge->Configure(FSimpleDelegate::CreateLambda([TryTravel]()
		{
			TryTravel(TEXT("PIE save bootstrap received SignalOnGameLoaded;"));
		}));

		FScriptDelegate ScriptDelegate;
		ScriptDelegate.BindUFunction(Bridge, GET_FUNCTION_NAME_CHECKED(UARInvaderPIESaveLoadedBridge, HandleSignalOnGameLoaded));
		LoadedSignalProperty->RemoveDelegate(ScriptDelegate, GI);
		LoadedSignalProperty->AddDelegate(ScriptDelegate, GI);
		PIESaveLoadedBridge.Reset(Bridge);
		bSubscribedToSignal = true;
		SetStatus(TEXT("PIE save bootstrap subscribed to SignalOnGameLoaded."));
	}

	GI->ProcessEvent(LoadFn, ParamsPtr);
	if (*bTravelIssued)
	{
		return true;
	}

	const double StartTimeSeconds = FPlatformTime::Seconds();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPanel, WeakGI, TryTravel, bTravelIssued, bSubscribedToSignal, StartTimeSeconds](float)
	{
		if (*bTravelIssued)
		{
			return false;
		}

		const TSharedPtr<SInvaderAuthoringPanel> Pinned = WeakPanel.Pin();
		if (!Pinned.IsValid())
		{
			return false;
		}
		if (!Pinned->GetPIEWorld())
		{
			return true;
		}

		const double ElapsedSeconds = FPlatformTime::Seconds() - StartTimeSeconds;
		const bool bTimedOut = ElapsedSeconds >= PIEBootstrapTimeoutSeconds;

		if (bSubscribedToSignal)
		{
			if (!bTimedOut)
			{
				return true;
			}
			TryTravel(TEXT("PIE save bootstrap timed out waiting for SignalOnGameLoaded;"));
			return false;
		}

		bool bHadSignal = false;
		const bool bLoadComplete = IsPIESaveLoadComplete(WeakGI.Get(), bHadSignal);
		const bool bFallbackReady = ElapsedSeconds >= PIEBootstrapFallbackDelaySeconds;
		if (!(bLoadComplete || (!bHadSignal && bFallbackReady) || bTimedOut))
		{
			return true;
		}

		if (bTimedOut)
		{
			TryTravel(TEXT("PIE save bootstrap timeout waiting for load complete;"));
		}
		else if (bLoadComplete)
		{
			TryTravel(TEXT("PIE save bootstrap load complete;"));
		}
		else
		{
			TryTravel(TEXT("PIE save bootstrap load invoked (no completion signal detected);"));
		}
		return false;
	}), PIEBootstrapPollInterval);

	return true;
}

UWorld* SInvaderAuthoringPanel::GetPIEWorld() const
{
	if (GEditor && GEditor->PlayWorld && GEditor->PlayWorld->GetAuthGameMode())
	{
		return GEditor->PlayWorld;
	}

	if (!GEngine)
	{
		return nullptr;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE)
		{
			if (UWorld* World = Context.World())
			{
				if (World->GetAuthGameMode())
				{
					return World;
				}
			}
		}
	}

	if (GEditor && GEditor->PlayWorld)
	{
		return GEditor->PlayWorld;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE)
		{
			return Context.World();
		}
	}

	return nullptr;
}

bool SInvaderAuthoringPanel::ExecPIECommand(const FString& Command)
{
	UWorld* PIEWorld = GetPIEWorld();
	if (!PIEWorld || !GEngine)
	{
		SetStatus(TEXT("No PIE world available for command."));
		return false;
	}

	const bool bResult = GEngine->Exec(PIEWorld, *Command);
	if (!bResult)
	{
		SetStatus(FString::Printf(TEXT("PIE command failed: %s"), *Command));
	}
	return bResult;
}

