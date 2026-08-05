#include "engine/editor/Collision.h"
#include "engine/editor/Editor.h"
#include "port/ui/DefaultProperties.h"

TrackEditor::Editor* TrackEditor::Editor::Instance = nullptr;

TrackEditor::Editor::Editor() {
    Instance = this;
}

TrackEditor::Editor::~Editor() {
    if (Instance == this) {
        Instance = nullptr;
    }
}

void TrackEditor::Editor::Load() {}
void TrackEditor::Editor::Enable() {}
void TrackEditor::Editor::Disable() {}
bool TrackEditor::Editor::IsEnabled() { return false; }
void TrackEditor::Editor::Play() {}
void TrackEditor::Editor::Pause() {}
bool TrackEditor::Editor::IsPaused() { return false; }
void TrackEditor::Editor::TogglePlayState() {}
void TrackEditor::Editor::Tick() {}
void TrackEditor::Editor::Draw() {}
void TrackEditor::Editor::GenerateCollision() {}
TrackEditor::GameObject* TrackEditor::Editor::AddObject(FVector, IRotator, FVector, const char*, float,
                                                        GameObject::CollisionType, float) {
    return nullptr;
}
void TrackEditor::Editor::AddLight(const char*, FVector*, s8*) {}
void TrackEditor::Editor::ClearObjects() { eGameObjects.clear(); }
void TrackEditor::Editor::ResetGizmo() {}
void TrackEditor::Editor::RemoveObject() {}
void TrackEditor::Editor::SelectObjectFromSceneExplorer(std::variant<AActor*, OObject*, GameObject*>) {}
void TrackEditor::Editor::SetLevelDimensions(s16, s16, s16, s16, s16, s16) {}
void TrackEditor::Editor::ClearMatrixPool() {}
void TrackEditor::Editor::DeleteObject() {}

namespace TrackEditor {
void GenerateCollisionMesh(std::variant<AActor*, OObject*, GameObject*>, Gfx*, float) {}
void DebugCollision(GameObject*, FVector, IRotator, FVector, const std::vector<Triangle>&) {}
}

void DrawDefaultEditorProperties() {}

extern "C" {
void Editor_Launch(const char*) {}
void Editor_SetLevelDimensions(s16, s16, s16, s16, s16, s16) {}
bool Editor_IsEnabled() { return false; }
bool Editor_IsPaused() { return false; }
}
