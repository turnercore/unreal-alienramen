#include "ARInvaderAuthoringPanel.h"

#include "ARInvaderAuthoringEditorProxies.h"
#include "ARInvaderAuthoringEditorSettings.h"
#include "AREnemyBase.h"
#include "ARInvaderDirectorSettings.h"
#include "ARLog.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorFileUtils.h"
#include "EditorLoadingAndSavingUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "IAssetRegistry.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PlayInEditorDataTypes.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Styling/AppStyle.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Misc/PackageName.h"

namespace
{
	static constexpr float LayerMatchTolerance = 0.0001f;

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
			const float XRange = FMath::Max(1.f, BoundsMax.X - BoundsMin.X);
			const float YRange = FMath::Max(1.f, BoundsMax.Y - BoundsMin.Y);
			const float XAlpha = (Offset.X - BoundsMin.X) / XRange;
			const float YAlpha = (Offset.Y - BoundsMin.Y) / YRange;
			return FVector2D(XAlpha * Size.X, Size.Y - (YAlpha * Size.Y));
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
			TFunction<int32()> InGetSelectedSpawnIndex,
			TFunction<float()> InGetPreviewTime,
			TFunction<void(int32)> InOnSpawnSelected,
			TFunction<void(int32)> InOnBeginDragSpawn,
			TFunction<void()> InOnEndDragSpawn,
			TFunction<void(int32, const FVector2D&)> InOnSpawnMoved,
			TFunction<void(const FVector2D&)> InOnAddSpawnAt)
		{
			GetWaveRow = MoveTemp(InGetWaveRow);
			GetSelectedLayerDelay = MoveTemp(InGetSelectedLayerDelay);
			GetHideOtherLayers = MoveTemp(InGetHideOtherLayers);
			GetSelectedSpawnIndex = MoveTemp(InGetSelectedSpawnIndex);
			GetPreviewTime = MoveTemp(InGetPreviewTime);
			OnSpawnSelected = MoveTemp(InOnSpawnSelected);
			OnBeginDragSpawn = MoveTemp(InOnBeginDragSpawn);
			OnEndDragSpawn = MoveTemp(InOnEndDragSpawn);
			OnSpawnMoved = MoveTemp(InOnSpawnMoved);
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
				const float XRange = FMath::Max(1.f, BoundsMax.X - BoundsMin.X);
				const float YRange = FMath::Max(1.f, BoundsMax.Y - BoundsMin.Y);
				const float XAlpha = (Offset.X - BoundsMin.X) / XRange;
				const float YAlpha = (Offset.Y - BoundsMin.Y) / YRange;
				return FVector2D(XAlpha * Size.X, Size.Y - (YAlpha * Size.Y));
			};

			const FARWaveDefRow* WaveRow = GetWaveRow ? GetWaveRow() : nullptr;
			if (!WaveRow)
			{
				return LayerId + 2;
			}

			const bool bHideOtherLayers = GetHideOtherLayers ? GetHideOtherLayers() : false;
			const float SelectedLayerDelay = GetSelectedLayerDelay ? GetSelectedLayerDelay() : 0.f;
			const int32 SelectedIndex = GetSelectedSpawnIndex ? GetSelectedSpawnIndex() : INDEX_NONE;
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

				const FVector2D DotSize(12.f, 12.f);
				const FVector2D DotPos = Local - (DotSize * 0.5f);
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId + 2,
					AllottedGeometry.ToPaintGeometry(DotSize, FSlateLayoutTransform(DotPos)),
					FAppStyle::GetBrush("WhiteBrush"),
					ESlateDrawEffect::None,
					DotColor);

				if (Index == SelectedIndex)
				{
					FSlateDrawElement::MakeBox(
						OutDrawElements,
						LayerId + 3,
						AllottedGeometry.ToPaintGeometry(FVector2D(18.f, 18.f), FSlateLayoutTransform(Local - FVector2D(9.f, 9.f))),
						FAppStyle::GetBrush("WhiteBrush"),
						ESlateDrawEffect::None,
						FLinearColor(1.f, 1.f, 1.f, 0.3f));
				}
			}

			return LayerId + 4;
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

			const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
			const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			const int32 Closest = FindClosestSpawn(
				WaveRow,
				Local,
				MyGeometry.GetLocalSize(),
				Settings->GameplayBoundsMin,
				Settings->GameplayBoundsMax,
				GetSelectedLayerDelay ? GetSelectedLayerDelay() : 0.f,
				GetHideOtherLayers ? GetHideOtherLayers() : false);

			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
			{
				if (Closest != INDEX_NONE && OnSpawnSelected)
				{
					DraggedSpawnIndex = Closest;
					bStartedDragTransaction = false;
					OnSpawnSelected(Closest);
					return FReply::Handled().CaptureMouse(AsShared());
				}
			}
			else if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
			{
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
			if (!MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) || DraggedSpawnIndex == INDEX_NONE)
			{
				return FReply::Unhandled();
			}

			if (OnSpawnMoved)
			{
				if (!bStartedDragTransaction && OnBeginDragSpawn)
				{
					OnBeginDragSpawn(DraggedSpawnIndex);
					bStartedDragTransaction = true;
				}
				const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
				OnSpawnMoved(DraggedSpawnIndex, LocalToOffset(Local, MyGeometry.GetLocalSize()));
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
		}

	private:
		FVector2D LocalToOffset(const FVector2D& Local, const FVector2D& Size) const
		{
			const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
			const FVector2D BoundsMin = Settings->GameplayBoundsMin;
			const FVector2D BoundsMax = Settings->GameplayBoundsMax;
			const float XRange = FMath::Max(1.f, BoundsMax.X - BoundsMin.X);
			const float YRange = FMath::Max(1.f, BoundsMax.Y - BoundsMin.Y);
			const float XAlpha = FMath::Clamp(Local.X / FMath::Max(1.f, Size.X), 0.f, 1.f);
			const float YAlpha = FMath::Clamp(1.f - (Local.Y / FMath::Max(1.f, Size.Y)), 0.f, 1.f);
			return FVector2D(
				BoundsMin.X + (XRange * XAlpha),
				BoundsMin.Y + (YRange * YAlpha));
		}

	private:
		TFunction<const FARWaveDefRow*()> GetWaveRow;
		TFunction<float()> GetSelectedLayerDelay;
		TFunction<bool()> GetHideOtherLayers;
		TFunction<int32()> GetSelectedSpawnIndex;
		TFunction<float()> GetPreviewTime;
		TFunction<void(int32)> OnSpawnSelected;
		TFunction<void(int32)> OnBeginDragSpawn;
		TFunction<void()> OnEndDragSpawn;
		TFunction<void(int32, const FVector2D&)> OnSpawnMoved;
		TFunction<void(const FVector2D&)> OnAddSpawnAt;

		int32 DraggedSpawnIndex = INDEX_NONE;
		bool bStartedDragTransaction = false;
	};
}

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
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsArgs.bUpdatesFromSelection = false;

	RowDetailsView = PropertyEditorModule.CreateDetailView(DetailsArgs);
	SpawnDetailsView = PropertyEditorModule.CreateDetailView(DetailsArgs);
	RowDetailsView->OnFinishedChangingProperties().AddSP(SharedThis(this), &SInvaderAuthoringPanel::HandleWaveRowPropertiesChanged);
	SpawnDetailsView->OnFinishedChangingProperties().AddSP(SharedThis(this), &SInvaderAuthoringPanel::HandleSpawnPropertiesChanged);

	BuildLayout();
	RefreshTables();
	RefreshPalette();
	RefreshRowItems();
	RefreshDetailsObjects();
	RefreshIssues();
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
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SButton)
				.Text(FText::FromString("Reload Tables"))
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
				.OnClicked(this, &SInvaderAuthoringPanel::OnValidateSelected)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString("Validate All"))
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
						.HintText(FText::FromString("Rename selected row"))
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
						.OnGenerateRow(this, &SInvaderAuthoringPanel::OnGenerateRowNameRow)
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
							SNew(STextBlock)
							.Visibility_Lambda([]()
							{
								return GetDefault<UARInvaderAuthoringEditorSettings>()->bShowApproximatePreviewBanner
									? EVisibility::Visible
									: EVisibility::Collapsed;
							})
							.Text(FText::FromString("Wave layout is approximate preview only. Verify behavior in PIE."))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
							[
								SNew(SButton).Text(FText::FromString("Add Layer")).OnClicked(this, &SInvaderAuthoringPanel::OnAddLayer)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
							[
								SNew(SButton).Text(FText::FromString("Add Spawn"))
								.OnClicked(this, &SInvaderAuthoringPanel::OnAddSpawnToLayer)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
							[
								SNew(SButton).Text(FText::FromString("Delete Spawn"))
								.OnClicked(this, &SInvaderAuthoringPanel::OnDeleteSelectedSpawn)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
							[
								SNew(SButton).Text(FText::FromString("Move Up"))
								.OnClicked(this, &SInvaderAuthoringPanel::OnMoveSpawnUp)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
							[
								SNew(SButton).Text(FText::FromString("Move Down"))
								.OnClicked(this, &SInvaderAuthoringPanel::OnMoveSpawnDown)
							]
							+ SHorizontalBox::Slot().AutoWidth()
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
									SNew(STextBlock).Text(FText::FromString("Hide other layers"))
								]
							]
						]
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
								SAssignNew(LayerListView, SListView<TSharedPtr<FLayerItem>>)
								.ListItemsSource(&LayerItems)
								.OnGenerateRow(this, &SInvaderAuthoringPanel::OnGenerateLayerRow)
								.OnSelectionChanged(this, &SInvaderAuthoringPanel::HandleLayerSelectionChanged)
							]
							+ SSplitter::Slot().Value(0.55f)
							[
								SAssignNew(SpawnListView, SListView<TSharedPtr<FSpawnItem>>)
								.ListItemsSource(&SpawnItems)
								.OnGenerateRow(this, &SInvaderAuthoringPanel::OnGenerateSpawnRow)
								.OnSelectionChanged(this, &SInvaderAuthoringPanel::HandleSpawnSelectionChanged)
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
						SNew(STextBlock).Text(FText::FromString("Enemy Palette (Class + Color)"))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
					[
						SAssignNew(PaletteHost, SBorder)
						[
							BuildPaletteWidget()
						]
					]
					+ SVerticalBox::Slot().FillHeight(0.45f).Padding(0.f, 0.f, 0.f, 6.f)
					[
						RowDetailsView.ToSharedRef()
					]
					+ SVerticalBox::Slot().FillHeight(0.28f).Padding(0.f, 0.f, 0.f, 6.f)
					[
						SpawnDetailsView.ToSharedRef()
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(STextBlock).Text(FText::FromString("Validation Issues"))
					]
					+ SVerticalBox::Slot().FillHeight(0.27f).Padding(0.f, 0.f, 0.f, 6.f)
					[
						SAssignNew(IssueListView, SListView<TSharedPtr<FInvaderAuthoringIssue>>)
						.ListItemsSource(&IssueItems)
						.OnGenerateRow(this, &SInvaderAuthoringPanel::OnGenerateIssueRow)
						.OnSelectionChanged(this, &SInvaderAuthoringPanel::HandleIssueSelectionChanged)
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
			[this]() { return SelectedSpawnIndex; },
			[this]() { return PreviewTime; },
			[this](int32 SpawnIndex) { HandleCanvasSpawnSelected(SpawnIndex); },
			[this](int32 SpawnIndex) { HandleCanvasBeginSpawnDrag(SpawnIndex); },
			[this]() { HandleCanvasEndSpawnDrag(); },
			[this](int32 SpawnIndex, const FVector2D& NewOffset) { HandleCanvasSpawnMoved(SpawnIndex, NewOffset); },
			[this](const FVector2D& NewOffset) { HandleCanvasAddSpawnAt(NewOffset); });
	}
}

void SInvaderAuthoringPanel::RefreshTables()
{
	WaveTable = nullptr;
	StageTable = nullptr;

	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	if (!Settings)
	{
		SetStatus(TEXT("Missing UARInvaderDirectorSettings."));
		return;
	}

	WaveTable = Settings->WaveDataTable.LoadSynchronous();
	StageTable = Settings->StageDataTable.LoadSynchronous();
	if (!WaveTable || !StageTable)
	{
		SetStatus(FString::Printf(TEXT("Failed loading tables from settings. Wave='%s' Stage='%s'"), *Settings->WaveDataTable.ToString(), *Settings->StageDataTable.ToString()));
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
	if (!Row || !Row->EnemySpawns.IsValidIndex(SelectedSpawnIndex))
	{
		return nullptr;
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
			Item->EnemyClassName = Spawn.EnemyClass ? Spawn.EnemyClass->GetName() : TEXT("<None>");
			SpawnItems.Add(Item);
		}
		break;
	}

	if (!SpawnItems.ContainsByPredicate([this](const TSharedPtr<FSpawnItem>& Item)
	{
		return Item.IsValid() && Item->SpawnIndex == SelectedSpawnIndex;
	}))
	{
		SelectedSpawnIndex = SpawnItems.IsEmpty() ? INDEX_NONE : SpawnItems[0]->SpawnIndex;
	}

	if (SpawnListView.IsValid())
	{
		SpawnListView->RequestListRefresh();
		for (const TSharedPtr<FSpawnItem>& Item : SpawnItems)
		{
			if (Item.IsValid() && Item->SpawnIndex == SelectedSpawnIndex)
			{
				SpawnListView->SetSelection(Item);
				break;
			}
		}
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

void SInvaderAuthoringPanel::MarkTableDirty(UDataTable* Table) const
{
	if (!Table)
	{
		return;
	}
	Table->MarkPackageDirty();
}

void SInvaderAuthoringPanel::SaveTable(UDataTable* Table)
{
	if (!Table)
	{
		return;
	}
	TArray<UPackage*> Packages;
	Packages.Add(Table->GetOutermost());
	FEditorFileUtils::PromptForCheckoutAndSave(Packages, false, false);
}

void SInvaderAuthoringPanel::SaveActiveTable()
{
	SaveTable(GetActiveTable());
}

void SInvaderAuthoringPanel::SetStatus(const FString& Message)
{
	UE_LOG(ARLog, Log, TEXT("[InvaderAuthoring] %s"), *Message);
	if (StatusText.IsValid())
	{
		StatusText->SetText(FText::FromString(Message));
	}
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
	SelectedSpawnIndex = INDEX_NONE;
	RefreshLayerItems();
	RefreshSpawnItems();
	RefreshDetailsObjects();
}

void SInvaderAuthoringPanel::SelectStageRow(FName RowName)
{
	SelectedStageRow = RowName;
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
	SelectedSpawnIndex = SpawnIndex;
	RefreshDetailsObjects();
	if (WaveCanvas.IsValid())
	{
		WaveCanvas->Invalidate(EInvalidateWidget::LayoutAndVolatility);
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

	if (Mode == EAuthoringMode::Waves)
	{
		const FName RowName = MakeUniqueRowName(Table, TEXT("Wave_New"));
		FARWaveDefRow NewRow;
		Table->AddRow(RowName, NewRow);
		SelectedWaveRow = RowName;
	}
	else
	{
		const FName RowName = MakeUniqueRowName(Table, TEXT("Stage_New"));
		FARStageDefRow NewRow;
		Table->AddRow(RowName, NewRow);
		SelectedStageRow = RowName;
	}

	MarkTableDirty(Table);
	RefreshRowItems();
	RefreshDetailsObjects();
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

	if (Mode == EAuthoringMode::Waves)
	{
		const FARWaveDefRow* Source = Table->FindRow<FARWaveDefRow>(SelectedWaveRow, TEXT("Duplicate"), false);
		if (!Source)
		{
			SetStatus(TEXT("Select a wave row first."));
			return FReply::Handled();
		}

		const FName NewName = MakeUniqueRowName(Table, SelectedWaveRow.ToString() + TEXT("_Copy"));
		Table->AddRow(NewName, *Source);
		SelectedWaveRow = NewName;
	}
	else
	{
		const FARStageDefRow* Source = Table->FindRow<FARStageDefRow>(SelectedStageRow, TEXT("Duplicate"), false);
		if (!Source)
		{
			SetStatus(TEXT("Select a stage row first."));
			return FReply::Handled();
		}
		const FName NewName = MakeUniqueRowName(Table, SelectedStageRow.ToString() + TEXT("_Copy"));
		Table->AddRow(NewName, *Source);
		SelectedStageRow = NewName;
	}

	MarkTableDirty(Table);
	RefreshRowItems();
	RefreshDetailsObjects();
	SetStatus(TEXT("Duplicated row."));
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

	const FName Target = Mode == EAuthoringMode::Waves ? SelectedWaveRow : SelectedStageRow;
	if (Target.IsNone())
	{
		SetStatus(TEXT("Select a row first."));
		return FReply::Handled();
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringDeleteRow", "Delete Invader Row"));
	Table->Modify();
	Table->RemoveRow(Target);
	MarkTableDirty(Table);

	if (Mode == EAuthoringMode::Waves)
	{
		SelectedWaveRow = NAME_None;
		SelectedSpawnIndex = INDEX_NONE;
	}
	else
	{
		SelectedStageRow = NAME_None;
	}

	RefreshRowItems();
	RefreshDetailsObjects();
	SetStatus(FString::Printf(TEXT("Deleted row '%s'."), *Target.ToString()));
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
	SelectedSpawnIndex = Row->EnemySpawns.Num() - 1;

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
	SelectedSpawnIndex = Row->EnemySpawns.Num() - 1;
	MarkTableDirty(WaveTable);
	RefreshLayerItems();
	RefreshSpawnItems();
	SetStatus(TEXT("Added spawn to selected layer."));
	return FReply::Handled();
}

FReply SInvaderAuthoringPanel::OnDeleteSelectedSpawn()
{
	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row || !WaveTable || !Row->EnemySpawns.IsValidIndex(SelectedSpawnIndex))
	{
		SetStatus(TEXT("Select a spawn first."));
		return FReply::Handled();
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringDeleteSpawn", "Delete Wave Spawn"));
	WaveTable->Modify();
	Row->EnemySpawns.RemoveAt(SelectedSpawnIndex);
	SelectedSpawnIndex = INDEX_NONE;

	MarkTableDirty(WaveTable);
	RefreshLayerItems();
	RefreshSpawnItems();
	SetStatus(TEXT("Deleted selected spawn."));
	return FReply::Handled();
}

void SInvaderAuthoringPanel::MoveSpawnWithinLayer(int32 Direction)
{
	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row || !WaveTable || !Row->EnemySpawns.IsValidIndex(SelectedSpawnIndex))
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

	const int32 Pos = LayerIndices.IndexOfByKey(SelectedSpawnIndex);
	const int32 NewPos = Pos + Direction;
	if (Pos == INDEX_NONE || !LayerIndices.IsValidIndex(NewPos))
	{
		return;
	}

	const int32 SwapA = LayerIndices[Pos];
	const int32 SwapB = LayerIndices[NewPos];
	if (!Row->EnemySpawns.IsValidIndex(SwapA) || !Row->EnemySpawns.IsValidIndex(SwapB))
	{
		return;
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringReorderSpawn", "Reorder Wave Spawn"));
	WaveTable->Modify();
	Row->EnemySpawns.Swap(SwapA, SwapB);
	SelectedSpawnIndex = SwapB;

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

void SInvaderAuthoringPanel::HandleCanvasSpawnSelected(int32 SpawnIndex)
{
	SelectSpawn(SpawnIndex);
}

void SInvaderAuthoringPanel::HandleCanvasBeginSpawnDrag(int32 SpawnIndex)
{
	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row || !WaveTable || !Row->EnemySpawns.IsValidIndex(SpawnIndex))
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

	SpawnDragTransaction.Reset();
	bSpawnDragChanged = false;
}

void SInvaderAuthoringPanel::HandleCanvasSpawnMoved(int32 SpawnIndex, const FVector2D& NewOffset)
{
	FARWaveDefRow* Row = GetSelectedWaveRow();
	if (!Row || !WaveTable || !Row->EnemySpawns.IsValidIndex(SpawnIndex))
	{
		return;
	}

	FARWaveEnemySpawnDef& Spawn = Row->EnemySpawns[SpawnIndex];
	if (Spawn.AuthoredScreenOffset.Equals(NewOffset, 0.01f))
	{
		return;
	}

	if (!SpawnDragTransaction.IsValid())
	{
		const FScopedTransaction Tx(
			NSLOCTEXT("AlienRamenEditor", "InvaderAuthoringMoveSpawnImmediate", "Move Wave Spawn"));
		WaveTable->Modify();
		Spawn.AuthoredScreenOffset = NewOffset;
	}
	else
	{
		Spawn.AuthoredScreenOffset = NewOffset;
		bSpawnDragChanged = true;
	}

	SelectedSpawnIndex = SpawnIndex;
	MarkTableDirty(WaveTable);
	RefreshDetailsObjects();
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
	SelectedSpawnIndex = Row->EnemySpawns.Num() - 1;
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
	return FMath::Max(1.f, MaxDelay + FMath::Max(0.f, Row->EnterDuration + Row->ActiveDuration + Row->BerserkDuration));
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
	const float BerserkEnd = ActiveEnd + FMath::Max(0.f, Row->BerserkDuration);
	return FText::FromString(FString::Printf(TEXT("Preview %.2fs | Enter [0-%.2f] Active [%.2f-%.2f] Berserk [%.2f-%.2f]"), PreviewTime, EnterEnd, EnterEnd, ActiveEnd, ActiveEnd, BerserkEnd));
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

TSharedRef<ITableRow> SInvaderAuthoringPanel::OnGenerateRowNameRow(TSharedPtr<FRowNameItem> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(STableRow<TSharedPtr<FRowNameItem>>, OwnerTable)
	[
		SNew(STextBlock).Text(FText::FromName(Item.IsValid() ? Item->RowName : NAME_None))
	];
}

TSharedRef<ITableRow> SInvaderAuthoringPanel::OnGenerateLayerRow(TSharedPtr<FLayerItem> Item, const TSharedRef<STableViewBase>& OwnerTable) const
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

TSharedRef<ITableRow> SInvaderAuthoringPanel::OnGenerateSpawnRow(TSharedPtr<FSpawnItem> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	const FString Label = Item.IsValid()
		? FString::Printf(TEXT("#%d  %s  %s"), Item->SpawnIndex, *EnemyColorToName(Item->Color), *Item->EnemyClassName)
		: TEXT("<invalid>");
	return SNew(STableRow<TSharedPtr<FSpawnItem>>, OwnerTable)
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
	if (!Item.IsValid())
	{
		return;
	}
	SelectSpawn(Item->SpawnIndex);
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
		FallbackEntry.Label = FText::FromString(
			FString::Printf(TEXT("%s %s"), *FPackageName::ObjectPathToObjectName(ClassPath.ToString()), *EnemyColorToName(Color)));
		SetActivePaletteEntry(FallbackEntry);
	};

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	for (const FPaletteClassRow& Row : PaletteClassRows)
	{
		const FSoftClassPath ClassPath = Row.ClassPath;
		const FString ClassName = FPackageName::ObjectPathToObjectName(ClassPath.ToString());
		const bool bFavorite = Row.bFavorite;

		Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SButton)
					.Text(FText::FromString(bFavorite ? TEXT("Fav") : TEXT("+Fav")))
					.OnClicked_Lambda([this, ClassPath]()
					{
						ToggleFavoriteClass(ClassPath);
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
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Cls = *It;
		if (!Cls || !Cls->IsChildOf(AAREnemyBase::StaticClass()))
		{
			continue;
		}
		if (Cls->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}
		UniqueClasses.Add(FSoftClassPath(Cls));
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	Filter.PackagePaths.Add(TEXT("/Game"));
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

			bIsCompatibleEnemyClass = LoadedClass
				&& LoadedClass->IsChildOf(AAREnemyBase::StaticClass())
				&& !LoadedClass->HasAnyClassFlags(CLASS_Abstract);
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
			Entry.Label = FText::FromString(FString::Printf(TEXT("%s %s"), *FPackageName::ObjectPathToObjectName(ClassPath.ToString()), *EnemyColorToName(Color)));
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
	if (!Row.BannedArchetypeTags.IsEmpty())
	{
		OutIssues.Add({ false, true, RowName, TEXT("BannedArchetypeTags is authored but not currently enforced by runtime selection.") });
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
}

FReply SInvaderAuthoringPanel::OnStartOrAttachPIE()
{
	if (GetPIEWorld())
	{
		SetStatus(TEXT("Attached to existing PIE session."));
		return FReply::Handled();
	}

	if (EnsurePIESession(true))
	{
		SetStatus(TEXT("PIE launch requested..."));
	}
	else
	{
		SetStatus(TEXT("Could not start or attach PIE session."));
	}
	return FReply::Handled();
}

FReply SInvaderAuthoringPanel::OnStopPIE()
{
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
	if (!Settings->DefaultTestMap.IsNull())
	{
		const FSoftObjectPath MapPath = Settings->DefaultTestMap.ToSoftObjectPath();
		FString MapPackageName = MapPath.GetLongPackageName();
		if (MapPackageName.IsEmpty())
		{
			MapPackageName = Settings->DefaultTestMap.ToString();
		}
		if (!UEditorLoadingAndSavingUtils::LoadMap(MapPackageName))
		{
			SetStatus(FString::Printf(TEXT("Failed to load default test map '%s'."), *MapPackageName));
			return false;
		}
	}

	ULevelEditorPlaySettings* PlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
	PlaySettings->PlayNetMode = EPlayNetMode::PIE_ListenServer;
	PlaySettings->PlayNumberOfClients = 1;
	PlaySettings->RunUnderOneProcess = true;
	PlaySettings->SaveConfig();

	FRequestPlaySessionParams Params;
	GEditor->RequestPlaySession(Params);
	return true;
}

UWorld* SInvaderAuthoringPanel::GetPIEWorld() const
{
	if (GEditor && GEditor->PlayWorld)
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
