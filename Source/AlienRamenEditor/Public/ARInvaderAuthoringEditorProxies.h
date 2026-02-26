#pragma once

#include "CoreMinimal.h"
#include "ARInvaderTypes.h"
#include "UObject/Object.h"
#include "ARInvaderAuthoringEditorProxies.generated.h"

UCLASS(Transient)
class ALIENRAMENEDITOR_API UARInvaderWaveRowProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "", meta=(ShowOnlyInnerProperties))
	FARWaveDefRow Row;
};

UCLASS(Transient)
class ALIENRAMENEDITOR_API UARInvaderStageRowProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "", meta=(ShowOnlyInnerProperties))
	FARStageDefRow Row;
};

UCLASS(Transient)
class ALIENRAMENEDITOR_API UARInvaderEnemyRowProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "", meta=(ShowOnlyInnerProperties))
	FARInvaderEnemyDefRow Row;
};

UCLASS(Transient)
class ALIENRAMENEDITOR_API UARInvaderSpawnProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "", meta=(ShowOnlyInnerProperties))
	FARWaveEnemySpawnDef Spawn;
};

UCLASS(Transient)
class ALIENRAMENEDITOR_API UARInvaderPIESaveLoadedBridge : public UObject
{
	GENERATED_BODY()

public:
	void Configure(FSimpleDelegate InOnLoaded);

	UFUNCTION()
	void HandleSignalOnGameLoaded();

private:
	FSimpleDelegate OnLoaded;
};
