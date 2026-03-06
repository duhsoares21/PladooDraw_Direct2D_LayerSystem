#include "Actions.h"

void ActionsClass::TCreateMoveAction(int targetID, ACTION selectedAction)
{
    ACTION action;
    actionId++;

    action.Id = actionId;
    action.Tool = TMove;
    action.TargetID = targetID;
    action.Layer = layerIndex;
    action.FrameIndex = CurrentFrameIndex;

    switch (selectedAction.Tool) {
        case TBrush:

            action.FreeForm = selectedAction.FreeForm;

            break;

        case TRectangle:
        case TWrite:
            action.Position = selectedAction.Position;
            break;

        case TEllipse:
            action.Ellipse = selectedAction.Ellipse;
            break;

        case TLine:
            action.Line = selectedAction.Line;
            break;

        default:
            break;

    }

    action.LastMovedPosition = true;

    Actions.emplace_back(action);
}
