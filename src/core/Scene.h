#pragma once
#include "ButtonDispatcher.h"

// 场景标识枚举
enum SceneID {
    SCENE_NONE = 0,
    SCENE_TERRARIUM,    // 培养缸主场景
    SCENE_MENU,         // 图标菜单
    SCENE_INFO,         // 属性信息页
    SCENE_SETTINGS,     // 设置
    SCENE_BATTLE,       // 对战
};

// 场景基类 — 所有界面继承此接口
class Scene {
public:
    virtual ~Scene() = default;

    // 进入场景（初始化）
    virtual void onEnter() = 0;

    // 退出场景（清理）
    virtual void onExit() = 0;

    // 每帧逻辑更新，返回切换目标场景（NONE=保持）
    virtual SceneID update() = 0;

    // 每帧渲染
    virtual void render() = 0;

    // 按钮事件回调（观察者模式）
    // 返回 true 表示已消费，不再分发给其他监听者
    virtual bool onButton(const ButtonEvent& ev) { (void)ev; return false; }

protected:
    SceneID nextScene = SCENE_NONE;  // 由 onButton 设置，update() 中检查返回
};
