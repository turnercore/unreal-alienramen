#include "ARStateTreeAIComponent.h"

#include "ARLog.h"
#include "ARStateTreeAIComponentSchema.h"
#include "StateTree.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeExecutionTypes.h"

TSubclassOf<UStateTreeSchema> UARStateTreeAIComponent::GetSchema() const
{
	return UARStateTreeAIComponentSchema::StaticClass();
}

void UARStateTreeAIComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshActiveStateTags();
}

void UARStateTreeAIComponent::StartLogic()
{
	if (!StateTreeRef.GetStateTree())
	{
		UE_LOG(ARLog, Error, TEXT("[EnemyAI|StateTreeComp] StartLogic failed on '%s': no StateTree assigned to component."),
			*GetNameSafe(GetOwner()));
	}

	Super::StartLogic();

	if (!IsRunning())
	{
		UE_LOG(ARLog, Error, TEXT("[EnemyAI|StateTreeComp] StartLogic did not enter running state on '%s' (StateTree='%s')."),
			*GetNameSafe(GetOwner()), *GetNameSafe(StateTreeRef.GetStateTree()));
	}
	else
	{
		UE_LOG(ARLog, Verbose, TEXT("[EnemyAI|StateTreeComp] StateTree running on '%s' (StateTree='%s')."),
			*GetNameSafe(GetOwner()), *GetNameSafe(StateTreeRef.GetStateTree()));
	}

	RefreshActiveStateTags();
}

void UARStateTreeAIComponent::StopLogic(const FString& Reason)
{
	Super::StopLogic(Reason);
	UE_LOG(ARLog, Verbose, TEXT("[EnemyAI|StateTreeComp] StopLogic on '%s'. Reason: %s"),
		*GetNameSafe(GetOwner()), *Reason);
	RefreshActiveStateTags();
}

void UARStateTreeAIComponent::Cleanup()
{
	Super::Cleanup();
	RefreshActiveStateTags();
}

void UARStateTreeAIComponent::RefreshActiveStateTags()
{
	FGameplayTagContainer NewTags;

	if (IsRunning())
	{
		const UStateTree* RootStateTree = StateTreeRef.GetStateTree();
		UObject* OwnerObject = GetOwner();
		if (RootStateTree && OwnerObject)
		{
			FStateTreeReadOnlyExecutionContext Context(OwnerObject, RootStateTree, InstanceData);
			if (Context.IsValid())
			{
				const TConstArrayView<FStateTreeExecutionFrame> ActiveFrames = Context.GetActiveFrames();
				for (const FStateTreeExecutionFrame& ActiveFrame : ActiveFrames)
				{
					const UStateTree* FrameStateTree = ActiveFrame.StateTree ? ActiveFrame.StateTree.Get() : RootStateTree;
					if (!FrameStateTree)
					{
						continue;
					}

					for (const FStateTreeStateHandle ActiveStateHandle : ActiveFrame.ActiveStates)
					{
						const FCompactStateTreeState* ActiveState = FrameStateTree->GetStateFromHandle(ActiveStateHandle);
						if (ActiveState && ActiveState->Tag.IsValid())
						{
							NewTags.AddTag(ActiveState->Tag);
						}
					}
				}
			}
		}
	}

	EmitTagDelta(NewTags);
	CurrentActiveStateTags = MoveTemp(NewTags);
}

void UARStateTreeAIComponent::EmitTagDelta(const FGameplayTagContainer& NewTags)
{
	FGameplayTagContainer AddedTags;
	for (const FGameplayTag Tag : NewTags)
	{
		if (!CurrentActiveStateTags.HasTagExact(Tag))
		{
			AddedTags.AddTag(Tag);
		}
	}

	FGameplayTagContainer RemovedTags;
	for (const FGameplayTag Tag : CurrentActiveStateTags)
	{
		if (!NewTags.HasTagExact(Tag))
		{
			RemovedTags.AddTag(Tag);
		}
	}

	if (!AddedTags.IsEmpty() || !RemovedTags.IsEmpty())
	{
		OnActiveStateTagsChanged.Broadcast(AddedTags, RemovedTags);
	}
}
