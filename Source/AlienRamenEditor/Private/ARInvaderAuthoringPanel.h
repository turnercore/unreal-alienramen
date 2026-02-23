#pragma once

#include "CoreMinimal.h"
#include "ARInvaderTypes.h"
#include "Input/Reply.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class IDetailsView;
class UDataTable;
class UARInvaderWaveRowProxy;
class UARInvaderStageRowProxy;
class UARInvaderSpawnProxy;
class SInvaderWaveCanvas;
struct FSpawnTabArgs;
class SDockTab;
class ITableRow;
class STableViewBase;
class SBorder;
struct FPropertyChangedEvent;
class FScopedTransaction;

struct FInvaderAuthoringIssue
{
	bool bError = true;
	bool bWave = true;
	FName RowName = NAME_None;
	FString Message;
};

struct FInvaderLayerInfo
{
	float Delay = 0.f;
	TArray<int32> SpawnIndices;
};

namespace ARInvaderAuthoringEditor
{
	extern const FName TabName;
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);
}

class SInvaderAuthoringPanel final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SInvaderAuthoringPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	~SInvaderAuthoringPanel();

private:
	enum class EAuthoringMode : uint8
	{
		Waves,
		Stages
	};

	struct FPaletteEntry
	{
		FSoftClassPath EnemyClassPath;
		EAREnemyColor Color = EAREnemyColor::Red;
		FText Label;
		bool bFavorite = false;
	};

	struct FRowNameItem
	{
		FName RowName = NAME_None;
	};

	struct FLayerItem
	{
		float Delay = 0.f;
		int32 Count = 0;
	};

	struct FSpawnItem
	{
		int32 SpawnIndex = INDEX_NONE;
		float Delay = 0.f;
		EAREnemyColor Color = EAREnemyColor::Red;
		FString EnemyClassName;
	};

	// setup
	void BuildLayout();
	void RefreshTables();
	void RefreshRowItems();
	void RefreshLayerItems();
	void RefreshSpawnItems();
	void RefreshPalette();
	void RefreshIssues();
	void RefreshDetailsObjects();

	// helpers
	UDataTable* GetActiveTable() const;
	const FARWaveDefRow* GetWaveRow(FName RowName) const;
	const FARStageDefRow* GetStageRow(FName RowName) const;
	FARWaveDefRow* GetMutableWaveRow(FName RowName);
	FARStageDefRow* GetMutableStageRow(FName RowName);
	FARWaveDefRow* GetSelectedWaveRow();
	FARStageDefRow* GetSelectedStageRow();
	const FARWaveDefRow* GetSelectedWaveRowConst() const;
	const FARStageDefRow* GetSelectedStageRowConst() const;
	FARWaveEnemySpawnDef* GetSelectedSpawn();
	FName MakeUniqueRowName(UDataTable* Table, const FString& BaseName) const;
	void MarkTableDirty(UDataTable* Table) const;
	void SaveTable(UDataTable* Table);
	void SaveActiveTable();
	void SetStatus(const FString& Message);

	// mode and selection
	void SetMode(EAuthoringMode NewMode);
	ECheckBoxState IsModeChecked(EAuthoringMode QueryMode) const;
	void SelectWaveRow(FName RowName);
	void SelectStageRow(FName RowName);
	void SelectLayerByDelay(float Delay);
	void SelectSpawn(int32 SpawnIndex);

	// row CRUD
	FReply OnCreateRow();
	FReply OnDuplicateRow();
	FReply OnRenameRow();
	FReply OnDeleteRow();
	FReply OnSaveTable();

	// wave editing
	FReply OnAddLayer();
	FReply OnAddSpawnToLayer();
	FReply OnDeleteSelectedSpawn();
	FReply OnMoveSpawnUp();
	FReply OnMoveSpawnDown();
	void MoveSpawnWithinLayer(int32 Direction);
	void UpdateLayerDelay(float OldDelay, float NewDelay);
	void HandleCanvasSpawnSelected(int32 SpawnIndex);
	void HandleCanvasBeginSpawnDrag(int32 SpawnIndex);
	void HandleCanvasEndSpawnDrag();
	void HandleCanvasSpawnMoved(int32 SpawnIndex, const FVector2D& NewOffset);
	void HandleCanvasAddSpawnAt(const FVector2D& NewOffset);

	// preview
	void SetPreviewTime(float NewPreviewTime);
	float GetPreviewTime() const;
	float GetMaxPreviewTime() const;
	FText GetPhaseSummaryText() const;

	// details callbacks
	void HandleWaveRowPropertiesChanged(const FPropertyChangedEvent& Event);
	void HandleSpawnPropertiesChanged(const FPropertyChangedEvent& Event);

	// list callbacks
	TSharedRef<ITableRow> OnGenerateRowNameRow(TSharedPtr<FRowNameItem> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedRef<ITableRow> OnGenerateLayerRow(TSharedPtr<FLayerItem> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedRef<ITableRow> OnGenerateSpawnRow(TSharedPtr<FSpawnItem> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedRef<ITableRow> OnGenerateIssueRow(TSharedPtr<FInvaderAuthoringIssue> Item, const TSharedRef<STableViewBase>& OwnerTable) const;

	void HandleRowListSelectionChanged(TSharedPtr<FRowNameItem> Item, ESelectInfo::Type);
	void HandleLayerSelectionChanged(TSharedPtr<FLayerItem> Item, ESelectInfo::Type);
	void HandleSpawnSelectionChanged(TSharedPtr<FSpawnItem> Item, ESelectInfo::Type);
	void HandleIssueSelectionChanged(TSharedPtr<FInvaderAuthoringIssue> Item, ESelectInfo::Type);

	// palette
	void ToggleFavoriteClass(const FSoftClassPath& ClassPath);
	void SetActivePaletteEntry(const FPaletteEntry& Entry);
	TSharedRef<SWidget> BuildPaletteWidget();

	// validation
	FReply OnValidateSelected();
	FReply OnValidateAll();
	void ValidateSelected(TArray<FInvaderAuthoringIssue>& OutIssues) const;
	void ValidateAll(TArray<FInvaderAuthoringIssue>& OutIssues) const;
	void ValidateWaveRow(const FName RowName, const FARWaveDefRow& Row, TArray<FInvaderAuthoringIssue>& OutIssues) const;
	void ValidateStageRow(const FName RowName, const FARStageDefRow& Row, const UDataTable* WavesTable, TArray<FInvaderAuthoringIssue>& OutIssues) const;

	// PIE harness
	FReply OnStartOrAttachPIE();
	FReply OnStopPIE();
	FReply OnStartRun();
	FReply OnStopRun();
	FReply OnForceStage();
	FReply OnForceWave();
	FReply OnForceThreat();
	FReply OnDumpState();
	bool EnsurePIESession(bool bStartIfNeeded);
	UWorld* GetPIEWorld() const;
	bool ExecPIECommand(const FString& Command);

private:
	EAuthoringMode Mode = EAuthoringMode::Waves;

	TObjectPtr<UDataTable> WaveTable = nullptr;
	TObjectPtr<UDataTable> StageTable = nullptr;

	FName SelectedWaveRow = NAME_None;
	FName SelectedStageRow = NAME_None;
	float SelectedLayerDelay = 0.f;
	int32 SelectedSpawnIndex = INDEX_NONE;
	float PreviewTime = 0.f;
	float ForcedThreatValue = 0.f;

	TArray<FInvaderLayerInfo> LayerInfos;
	TArray<TSharedPtr<FRowNameItem>> RowItems;
	TArray<TSharedPtr<FLayerItem>> LayerItems;
	TArray<TSharedPtr<FSpawnItem>> SpawnItems;
	TArray<TSharedPtr<FInvaderAuthoringIssue>> IssueItems;
	TArray<FPaletteEntry> PaletteEntries;
	TOptional<FPaletteEntry> ActivePaletteEntry;
	TMap<FSoftClassPath, bool> EnemyPaletteClassCompatibilityCache;
	TUniquePtr<FScopedTransaction> SpawnDragTransaction;
	bool bSpawnDragChanged = false;

	TSharedPtr<class SEditableTextBox> RenameTextBox;
	TSharedPtr<class STextBlock> StatusText;
	TSharedPtr<class SListView<TSharedPtr<FRowNameItem>>> RowListView;
	TSharedPtr<class SListView<TSharedPtr<FLayerItem>>> LayerListView;
	TSharedPtr<class SListView<TSharedPtr<FSpawnItem>>> SpawnListView;
	TSharedPtr<class SListView<TSharedPtr<FInvaderAuthoringIssue>>> IssueListView;
	TSharedPtr<SInvaderWaveCanvas> WaveCanvas;
	TSharedPtr<class SSlider> PreviewSlider;
	TSharedPtr<class SEditableTextBox> ThreatTextBox;
	TSharedPtr<IDetailsView> RowDetailsView;
	TSharedPtr<IDetailsView> SpawnDetailsView;
	TSharedPtr<SBorder> PaletteHost;

	TStrongObjectPtr<UARInvaderWaveRowProxy> WaveProxy;
	TStrongObjectPtr<UARInvaderStageRowProxy> StageProxy;
	TStrongObjectPtr<UARInvaderSpawnProxy> SpawnProxy;

	bool bApplyingProxyToModel = false;
};
