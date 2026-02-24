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
	UPROPERTY(EditAnywhere, Category = "Wave", meta=(ShowOnlyInnerProperties))
	FARWaveDefRow Row;
};

UCLASS(Transient)
class ALIENRAMENEDITOR_API UARInvaderStageRowProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Stage", meta=(ShowOnlyInnerProperties))
	FARStageDefRow Row;
};

UCLASS(Transient)
class ALIENRAMENEDITOR_API UARInvaderSpawnProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Spawn", meta=(ShowOnlyInnerProperties))
	FARWaveEnemySpawnDef Spawn;
};
