#pragma once
#include "../core/Scene.h"

// 属性信息页场景（支持多页切换）
class InfoScene : public Scene {
public:
    InfoScene() = default;

    void onEnter() override;
    void onExit() override;
    SceneID update() override;
    void render() override;
    bool onButton(const ButtonEvent& ev) override;

private:
    static const char* STAGE_NAMES[5];
    static constexpr int PAGE_COUNT = 2;

    int page = 0;

    void renderPageIndicator();
    void renderStatus();
    void renderRecord();
};
