#pragma once
#include "../core/Scene.h"

// 属性信息页场景
class InfoScene : public Scene {
public:
    InfoScene() = default;

    void onEnter() override;
    void onExit() override;
    SceneID update() override;
    void render() override;
    bool onButton(const ButtonEvent& ev) override;

private:
    static const char* STAGE_NAMES[4];
};
