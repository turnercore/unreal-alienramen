#include "ARShopStateTreeAIComponent.h"

#include "ARShopStateTreeAIComponentSchema.h"
#include "StateTree.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeExecutionTypes.h"

TSubclassOf<UStateTreeSchema> UARShopStateTreeAIComponent::GetSchema() const
{
	return UARShopStateTreeAIComponentSchema::StaticClass();
}

void UARShopStateTreeAIComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshActiveStateTags();
}

void UARShopStateTreeAIComponent::StartLogic()
{
	Super::StartLogic();
	RefreshActiveStateTags();
}

void UARShopStateTreeAIComponent::StopLogic(const FString& Reason)
{
	Super::StopLogic(Reason);
	RefreshActiveStateTags();
}

void UARShopStateTreeAIComponent::Cleanup()
{
	Super::Cleanup();
	RefreshActiveStateTags();
}

void UARShopStateTreeAIComponent::RefreshActiveStateTags()
{
	FGameplayTagContainer NewTags;
	bool bCouldReadContext = false;

	if (IsRunning())
	{
		const UStateTree* RootStateTree = StateTreeRef.GetStateTree();
		UObject* OwnerObject = GetOwner();
		if (RootStateTree && OwnerObject)
		{
			FStateTreeReadOnlyExecutionContext Context(OwnerObject, RootStateTree, InstanceData);
			if (Context.IsValid())
			{
				bCouldReadContext = true;
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

		if (!bCouldReadContext)
		{
			return;
		}
	}

	EmitTagDelta(NewTags);
	CurrentActiveStateTags = MoveTemp(NewTags);
}

void UARShopStateTreeAIComponent::EmitTagDelta(const FGameplayTagContainer& NewTags)
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
