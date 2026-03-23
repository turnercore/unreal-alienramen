/**
 * @file ARCharacterSubsystem.h
 * @brief World subsystem that coordinates character runtime actors and pawn bindings.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "ARCharacterSubsystem.generated.h"

class AARCharacterStateRuntime;
class AARPlayerController;
class AARPlayerStateBase;
class APawn;
class AController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARCharacterRuntimeRegisteredSignature, AARCharacterStateRuntime*, Runtime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARCharacterRuntimeUnregisteredSignature, AARCharacterStateRuntime*, Runtime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARCharacterRuntimePawnBoundSignature, AARCharacterStateRuntime*, Runtime, APawn*, Pawn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FARCharacterSwapCompletedSignature,
	AARPlayerController*,
	RequestingController,
	FGameplayTag,
	CharacterTag,
	APawn*,
	NewPawn,
	APawn*,
	PreviousPawn);

/**
 * Coordination/lookup subsystem for per-character runtime actors.
 *
 * This subsystem is orchestration only:
 * - discovers and registers runtime actors in the current world
 * - resolves character tag to runtime/pawn/controller
 * - executes authority-validated character swap flows
 * Replicated authoritative state remains on AARCharacterStateRuntime.
 */
UCLASS()
class ALIENRAMEN_API UARCharacterSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	/** Ensures a runtime actor exists for the given owner+character pair (server-only spawn path). */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Subsystem")
	AARCharacterStateRuntime* EnsureCharacterRuntime(AARPlayerStateBase* OwningPlayerState, FGameplayTag CharacterTag, bool& bOutCreated);

	/** Convenience overload when caller does not care whether runtime was newly created. */
	AARCharacterStateRuntime* EnsureCharacterRuntime(AARPlayerStateBase* OwningPlayerState, FGameplayTag CharacterTag);

	/** Registers an externally discovered runtime actor. Safe to call multiple times. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Subsystem")
	void RegisterRuntime(AARCharacterStateRuntime* Runtime);

	/** Unregisters a runtime actor that is ending play or no longer valid. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Subsystem")
	void UnregisterRuntime(AARCharacterStateRuntime* Runtime);

	/** Updates runtime pawn binding and rebroadcasts world-level notification. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Subsystem")
	void BindRuntimePawn(AARCharacterStateRuntime* Runtime, APawn* NewPawn);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Subsystem")
	AARCharacterStateRuntime* FindCharacterRuntimeByTag(FGameplayTag CharacterTag) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Subsystem")
	AARCharacterStateRuntime* FindCharacterRuntimeForPlayer(const AARPlayerStateBase* OwningPlayerState, FGameplayTag CharacterTag) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Subsystem")
	APawn* FindCharacterPawnByTag(FGameplayTag CharacterTag) const;

	/** Resolves the runtime actor currently bound to a specific pawn, if any. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Subsystem")
	AARCharacterStateRuntime* FindCharacterRuntimeByPawn(const APawn* Pawn) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Character Subsystem")
	AController* FindCharacterControllerByTag(FGameplayTag CharacterTag) const;

	/** Returns all currently registered runtime actors (invalid entries are filtered out). */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Subsystem")
	void GetRegisteredRuntimes(TArray<AARCharacterStateRuntime*>& OutRuntimes) const;

	/**
	 * Authority-validated character swap flow.
	 * - ensures/loads target runtime
	 * - reuses existing target pawn when available, otherwise restarts player to spawn a new pawn
	 * - updates PlayerState current-character pointers and runtime pawn binding
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Character Subsystem")
	bool TrySwapCharacter(AARPlayerController* RequestingController, FGameplayTag TargetCharacterTag, FString& OutFailureReason);

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Character Subsystem|Events")
	FARCharacterRuntimeRegisteredSignature OnCharacterRuntimeRegistered;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Character Subsystem|Events")
	FARCharacterRuntimeUnregisteredSignature OnCharacterRuntimeUnregistered;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Character Subsystem|Events")
	FARCharacterRuntimePawnBoundSignature OnCharacterRuntimePawnBound;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Character Subsystem|Events")
	FARCharacterSwapCompletedSignature OnCharacterSwapCompleted;

private:
	void CompactRegistry() const;
	AARCharacterStateRuntime* SpawnRuntimeActor(AARPlayerStateBase* OwningPlayerState, FGameplayTag CharacterTag) const;

	UPROPERTY(Transient)
	mutable TArray<TObjectPtr<AARCharacterStateRuntime>> RegisteredRuntimes;

	UPROPERTY(Transient)
	mutable TMap<FGameplayTag, TObjectPtr<AARCharacterStateRuntime>> RuntimeByCharacterTag;
};
