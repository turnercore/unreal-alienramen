/**
 * @file ARLobbyCharacterAssignmentWidget.h
 * @brief Concrete lobby-facing character assignment widget so Widget Blueprints can inherit the shared base.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARCharacterAssignmentWidgetBase.h"
#include "ARLobbyCharacterAssignmentWidget.generated.h"

/**
 * Concrete Widget Blueprint parent for lobby character selection and ready-state UI.
 *
 * Use this instead of the abstract shared base when creating UMG assets in the editor.
 * Runtime behavior still lives in UARCharacterAssignmentWidgetBase.
 */
UCLASS(Blueprintable)
class ALIENRAMEN_API UARLobbyCharacterAssignmentWidget : public UARCharacterAssignmentWidgetBase
{
	GENERATED_BODY()
};
