#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "StructUtils/InstancedStruct.h"
#include "StructSerializable.generated.h"

UINTERFACE(BlueprintType)
class ALIENRAMEN_API UStructSerializable : public UInterface
{
	GENERATED_BODY()
};

class ALIENRAMEN_API IStructSerializable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Alien Ramen|State Serialization")
	void ExtractStateToStruct(FInstancedStruct& CurrentState) const;
	virtual void ExtractStateToStruct_Implementation(FInstancedStruct& CurrentState) const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Alien Ramen|State Serialization")
	bool ApplyStateFromStruct(const FInstancedStruct& SavedState);
	virtual bool ApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Alien Ramen|State Serialization")
	UScriptStruct* GetStateStruct() const;
	virtual UScriptStruct* GetStateStruct_Implementation() const;
};
