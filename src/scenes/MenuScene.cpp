#include "MenuScene.h"
#include "../core/GameEngine.h"
#include "../core/SaveManager.h"
#include "../core/UiStrings.h"
#include "../hardware/BattleLink.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"
#include "../game/FoodType.h"
#include "../game/ItemCatalog.h"
#include "../assets/FoodAssets.h"
#include "../assets/WoodAssets.h"
#include "../assets/BowlAssets.h"
#include "../assets/MenuAssets.h"
#include "../game/NpcGenerator.h"
#include <cmath>

int MenuScene::lastSelectedByMode[(int)MenuScene::Mode::COUNT] = {};

namespace {

constexpr uint32_t MENU_VISIT_STATUS_INTERVAL_MS = 3000;

bool careItemsNotNeeded(Stage stage) {
    return stage == Stage::EGG || stage == Stage::LARVA || stage == Stage::PUPA;
}

}


#include "menu/MenuSceneMain.inc"
#include "menu/MenuSceneActions.inc"
#include "menu/MenuSceneModes.inc"
#include "menu/MenuSceneSleepGoods.inc"
