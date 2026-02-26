#pragma once

#include "CoreMinimal.h"
#include "ARInvaderTypes.h"
#include "Input/Reply.h"
#include "UObject/SoftObjectPath.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class IDetailsView;
class UDataTable;
class UARInvaderWaveRowProxy;
class UARInvaderStageRowProxy;
class UARInvaderSpawnProxy;
class UARInvaderPIESaveLoadedBridge;
class SInvaderWaveCanvas;
class FSpawnTabArgs;
class SDockTab;
class ITableRow;
class STableViewBase;
class SBorder;
class SExpandableArea;
enum class EItemDropZone;
struct FPropertyChangedEvent;
struct FPropertyAndParent;
class FScopedTransaction;
class UObject;
class FTransactionObjectEvent;

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

struct FInvaderPaletteDragPayload
{
	FSoftClassPath EnemyClassPath;
	EAREnemyColor Color = EAREnemyColor::Red;
	int32 ShapeCycle = 0;
	FString Label;
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
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

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
		int32 ShapeCycle = 0;
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

	struct FSpawnClipboardEntry
	{
		FARWaveEnemySpawnDef Spawn;
		FVector2D RelativeOffset = FVector2D::ZeroVector;
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
	const FARInvaderEnemyDefRow* FindEnemyDefinitionByIdentifierTag(FGameplayTag IdentifierTag, FName* OutRowName = nullptr) const;
	bool ResolveIdentifierTagForPaletteClass(const FSoftClassPath& ClassPath, FGameplayTag& OutIdentifierTag) const;
	FString FormatSpawnEnemyLabel(const FARWaveEnemySpawnDef& Spawn) const;
	FText GetSelectedSpawnEnemySummaryText() const;
	EVisibility GetSelectedSpawnEnemySummaryVisibility() const;
	FReply OnOpenSelectedSpawnEnemyRow();
	FARWaveDefRow* GetMutableWaveRow(FName RowName);
	FARStageDefRow* GetMutableStageRow(FName RowName);
	FARWaveDefRow* GetSelectedWaveRow();
	FARStageDefRow* GetSelectedStageRow();
	const FARWaveDefRow* GetSelectedWaveRowConst() const;
	const FARStageDefRow* GetSelectedStageRowConst() const;
	FARWaveEnemySpawnDef* GetSelectedSpawn();
	FName MakeUniqueRowName(UDataTable* Table, const FString& BaseName) const;
	void MarkTableDirty(UDataTable* Table);
	void SaveTable(UDataTable* Table, bool bPromptForCheckout = true);
	void SaveActiveTable(bool bPromptForCheckout = true);
	bool SaveTablePackage(UDataTable* Table) const;
	void EnsureInitialTableBackups();
	void CreateTableBackup(UDataTable* Table, const FString& BackupFolderLongPackagePath, int32 RetentionCount);
	void PruneTableBackups(const FString& BackupFolderLongPackagePath, const FString& AssetBaseName, int32 RetentionCount);
	void SetStatus(const FString& Message);
	TArray<FName> GetSelectedRowNames() const;
	FName GetPrimarySelectedRowName() const;

	// mode and selection
	void SetMode(EAuthoringMode NewMode);
	ECheckBoxState IsModeChecked(EAuthoringMode QueryMode) const;
	void SelectWaveRow(FName RowName);
	void SelectStageRow(FName RowName);
	void SelectLayerByDelay(float Delay);
	void SelectSpawn(int32 SpawnIndex);
	void ApplySpawnSelection(const TSet<int32>& NewSelection, int32 PreferredPrimary = INDEX_NONE);
	void ClearSpawnSelection();
	void SyncSpawnListSelectionFromState();
	void HandleObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event);

	// row CRUD
	FReply OnCreateRow();
	FReply OnDuplicateRow();
	FReply OnRenameRow();
	FReply OnDeleteRow();
	FReply OnToggleSelectedRowsEnabled();
	FReply OnSaveTable();

	// wave editing
	FReply OnAddLayer();
	FReply OnAddSpawnToLayer();
	FReply OnDeleteSelectedSpawn();
	FReply OnCopySelectedSpawns();
	FReply OnPasteSpawns();
	void SetSelectedSpawnColor(EAREnemyColor NewColor);
	FReply OnMoveSpawnUp();
	FReply OnMoveSpawnDown();
	void ReorderSpawnByDrop(int32 SourceSpawnIndex, int32 TargetSpawnIndex, EItemDropZone DropZone);
	TSharedPtr<SWidget> OnOpenSpawnContextMenu();
	TSharedRef<SWidget> BuildSpawnContextMenu();
	TSharedPtr<SWidget> OnOpenRowContextMenu();
	TSharedRef<SWidget> BuildRowContextMenu();
	void MoveSpawnWithinLayer(int32 Direction);
	void UpdateLayerDelay(float OldDelay, float NewDelay);
	void HandleCanvasSpawnSelected(int32 SpawnIndex, bool bToggle, bool bRangeSelect);
	void HandleCanvasClearSpawnSelection();
	void HandleCanvasOpenSpawnContextMenu(int32 SpawnIndex, const FVector2D& ScreenPosition);
	void HandleCanvasBeginSpawnDrag();
	void HandleCanvasEndSpawnDrag();
	void HandleCanvasSelectedSpawnsMoved(const FVector2D& OffsetDelta);
	void HandleCanvasSelectionRectChanged(const TArray<int32>& RectSelection, bool bAppendToSelection);
	void HandleCanvasAddSpawnAt(const FVector2D& NewOffset, const TOptional<FInvaderPaletteDragPayload>& DragPayload = TOptional<FInvaderPaletteDragPayload>());
	bool IsCanvasSnapEnabled() const;
	float GetCanvasGridSize() const;
	FVector2D SnapOffsetToGrid(const FVector2D& InOffset) const;

	// preview
	void SetPreviewTime(float NewPreviewTime);
	float GetPreviewTime() const;
	float GetMaxPreviewTime() const;
	FText GetPhaseSummaryText() const;

	// details callbacks
	void HandleWaveRowPropertiesChanged(const FPropertyChangedEvent& Event);
	void HandleSpawnPropertiesChanged(const FPropertyChangedEvent& Event);
	bool ShouldShowWaveDetailProperty(const FPropertyAndParent& PropertyAndParent) const;
	bool ShouldShowSpawnDetailProperty(const FPropertyAndParent& PropertyAndParent) const;

	// list callbacks
	TSharedRef<ITableRow> OnGenerateRowNameRow(TSharedPtr<FRowNameItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> OnGenerateLayerRow(TSharedPtr<FLayerItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> OnGenerateSpawnRow(TSharedPtr<FSpawnItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> OnGenerateIssueRow(TSharedPtr<FInvaderAuthoringIssue> Item, const TSharedRef<STableViewBase>& OwnerTable) const;

	void HandleRowListSelectionChanged(TSharedPtr<FRowNameItem> Item, ESelectInfo::Type);
	void HandleLayerSelectionChanged(TSharedPtr<FLayerItem> Item, ESelectInfo::Type);
	void HandleSpawnSelectionChanged(TSharedPtr<FSpawnItem> Item, ESelectInfo::Type);
	void HandleIssueSelectionChanged(TSharedPtr<FInvaderAuthoringIssue> Item, ESelectInfo::Type);

	// palette
	void ToggleFavoriteClass(const FSoftClassPath& ClassPath);
	int32 GetPaletteShapeCycle(const FSoftClassPath& ClassPath) const;
	void CyclePaletteShape(const FSoftClassPath& ClassPath);
	void SyncPaletteClassInContentBrowser(const FSoftClassPath& ClassPath);
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
	void SchedulePIESaveBootstrap();
	bool RunPIESaveBootstrap();
	UWorld* GetPIEWorld() const;
	bool ExecPIECommand(const FString& Command);

private:
	EAuthoringMode Mode = EAuthoringMode::Waves;

	TObjectPtr<UDataTable> WaveTable = nullptr;
	TObjectPtr<UDataTable> StageTable = nullptr;
	TObjectPtr<UDataTable> EnemyTable = nullptr;

	FName SelectedWaveRow = NAME_None;
	FName SelectedStageRow = NAME_None;
	float SelectedLayerDelay = 0.f;
	int32 SelectedSpawnIndex = INDEX_NONE;
	TSet<int32> SelectedSpawnIndices;
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
	TMap<int32, FVector2D> SpawnDragStartOffsets;
	TArray<FSpawnClipboardEntry> SpawnClipboardEntries;
	FVector2D SpawnClipboardAnchorOffset = FVector2D::ZeroVector;
	int32 SpawnPasteSerial = 0;
	FDelegateHandle ObjectTransactedHandle;
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
	TSharedPtr<SExpandableArea> ValidationIssuesArea;

	TStrongObjectPtr<UARInvaderWaveRowProxy> WaveProxy;
	TStrongObjectPtr<UARInvaderStageRowProxy> StageProxy;
	TStrongObjectPtr<UARInvaderSpawnProxy> SpawnProxy;
	TStrongObjectPtr<UARInvaderPIESaveLoadedBridge> PIESaveLoadedBridge;

	bool bApplyingProxyToModel = false;
	bool bSyncingSpawnSelection = false;
	bool bInitialBackupsCreated = false;
	bool bPendingPIESaveBootstrap = false;
	bool bPIESaveBootstrapContinueRequested = false;
	bool bPIESaveBootstrapRouteThroughLoading = false;
};
