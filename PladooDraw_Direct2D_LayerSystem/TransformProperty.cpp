#include "TransformProperty.h"

void Transform::SetLocation(int x, int y) {
    ACTION* selected = nullptr;

    if (!isMovingAction) {
        auto it = std::find_if(
            Actions.begin(),
            Actions.end(),
            [&](const ACTION& action) {
                return action.Id == selectedActionId;
            }
        );

        selected = &(*it);

        auto MoveAction = std::find_if(
            Actions.begin(),
            Actions.end(),
            [](const ACTION& action) {
                return action.Tool == TMove && action.TargetID == selectedActionId && action.LastMovedPosition == true;
            }
        );

        if (MoveAction != Actions.end()) {
            MoveAction->LastMovedPosition = false;
        }

        ActionsClass actionsClass;
        actionsClass.TCreateMoveAction(selectedActionId, *selected);
    }

    isMovingAction = true;

    auto it = std::find_if(
        Actions.begin(),
        Actions.end(),
        [&](const ACTION& action) {
            return action.Tool == TMove && action.TargetID == selectedActionId && action.LastMovedPosition == true;
        }
    );

    auto targetAction = std::find_if(
        Actions.begin(),
        Actions.end(),
        [&](const ACTION& action) {
            return action.Id == selectedActionId;
        }
    );

    selected = &(*it);

    DELTA delta = CalculateMovementDelta(x, y, &(*targetAction), selected);

    float deltaX = delta.deltaX;
    float deltaY = delta.deltaY;

    ACTION& action = *selected;

    switch (targetAction->Tool) {
        case TBrush:
            for (auto& v : action.FreeForm.vertices) {
                v.x += deltaX;
                v.y += deltaY;
            }
            break;
        case TRectangle:
        case TWrite:
            action.Position.left += deltaX;
            action.Position.top += deltaY;
            action.Position.right += deltaX;
            action.Position.bottom += deltaY;
            break;

        case TEllipse:
            action.Ellipse.point.x += deltaX;
            action.Ellipse.point.y += deltaY;
            break;

        case TLine:
            action.Line.startPoint.x += deltaX;
            action.Line.startPoint.y += deltaY;
            action.Line.endPoint.x += deltaX;
            action.Line.endPoint.y += deltaY;
            break;

        default:
            break;
    }

    bool hasPaintActive = false;

    for (auto& action : Actions) {
        if (action.PaintTarget == selectedActionId) {
            hasPaintActive = true;
            for (auto& pixel : action.pixelsToFill) {
                pixel.x += deltaX;
                pixel.y += deltaY;
            }
        }
    }

    if (!hasPaintActive) {
        for (auto& action : RedoActions) {
            if (action.PaintTarget == selectedActionId) {
                for (auto& pixel : action.pixelsToFill) {
                    pixel.x += deltaX;
                    pixel.y += deltaY;
                }
            }
        }
    }
}

void Transform::SetRotation() {

}

void Transform::SetScale() {

}