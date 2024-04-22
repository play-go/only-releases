#include "LevelScreen.hpp"

#include "../hud.h"
#include "../LevelFrontend.h"
#include "../../debug/Logger.h"
#include "../../audio/audio.h"
#include "../../coders/imageio.h"
#include "../../graphics/core/PostProcessing.h"
#include "../../graphics/core/GfxContext.hpp"
#include "../../graphics/core/Viewport.hpp"
#include "../../graphics/core/ImageData.hpp"
#include "../../graphics/ui/GUI.h"
#include "../../graphics/ui/elements/Menu.hpp"
#include "../../graphics/render/WorldRenderer.h"
#include "../../logic/LevelController.h"
#include "../../logic/scripting/scripting_hud.h"
#include "../../physics/Hitbox.h"
#include "../../voxels/Chunks.h"
#include "../../world/Level.h"
#include "../../world/World.h"
#include "../../window/Camera.h"
#include "../../window/Events.h"
#include "../../engine.h"

static debug::Logger logger("level-screen");

LevelScreen::LevelScreen(Engine* engine, Level* level) : Screen(engine) {
    postProcessing = std::make_unique<PostProcessing>();

    auto& settings = engine->getSettings();
    auto assets = engine->getAssets();
    auto menu = engine->getGUI()->getMenu();
    menu->reset();

    controller = std::make_unique<LevelController>(settings, level);
    frontend = std::make_unique<LevelFrontend>(controller.get(), assets);

    worldRenderer = std::make_unique<WorldRenderer>(engine, frontend.get(), controller->getPlayer());
    hud = std::make_unique<Hud>(engine, frontend.get(), controller->getPlayer());
    
    keepAlive(settings.graphics.backlight.observe([=](bool flag) {
        controller->getLevel()->chunks->saveAndClear();
    }));
    keepAlive(settings.camera.fov.observe([=](double value) {
        controller->getPlayer()->camera->setFov(glm::radians(value));
    }));

    animator = std::make_unique<TextureAnimator>();
    animator->addAnimations(assets->getAnimations());

    auto content = level->content;
    for (auto& entry : content->getPacks()) {
        auto pack = entry.second.get();
        const ContentPack& info = pack->getInfo();
        fs::path scriptFile = info.folder/fs::path("scripts/hud.lua");
        if (fs::is_regular_file(scriptFile)) {
            scripting::load_hud_script(pack->getEnvironment(), info.id, scriptFile);
        }
    }
    scripting::on_frontend_init(hud.get());
}

LevelScreen::~LevelScreen() {
    saveWorldPreview();
    scripting::on_frontend_close();
    controller->onWorldQuit();
    engine->getPaths()->setWorldFolder(fs::path());
}

void LevelScreen::saveWorldPreview() {
    try {
        logger.info() << "saving world preview";
        auto paths = engine->getPaths();
        auto player = controller->getPlayer();
        auto camera = player->camera;
        auto& settings = engine->getSettings();
        int previewSize = settings.ui.worldPreviewSize.get();
        Viewport viewport(previewSize * 1.5, previewSize);
        GfxContext ctx(nullptr, viewport, batch.get());
        worldRenderer->draw(ctx, camera.get(), false, postProcessing.get());
        auto image = postProcessing->toImage();
        image->flipY();
        imageio::write(paths->resolve("world:preview.png"), image.get());
    } catch (const std::exception& err) {
        logger.error() << err.what();
    }
}

void LevelScreen::updateHotkeys() {
    auto& settings = engine->getSettings();
    if (Events::jpressed(keycode::O)) {
        settings.graphics.frustumCulling = !settings.graphics.frustumCulling;
    }
    if (Events::jpressed(keycode::F1)) {
        hudVisible = !hudVisible;
    }
    if (Events::jpressed(keycode::F3)) {
        controller->getPlayer()->debug = !controller->getPlayer()->debug;
    }
    if (Events::jpressed(keycode::F5)) {
        controller->getLevel()->chunks->saveAndClear();
    }
}

void LevelScreen::update(float delta) {
    gui::GUI* gui = engine->getGUI();
    
    bool inputLocked = hud->isPause() || 
                       hud->isInventoryOpen() || 
                       gui->isFocusCaught();
    if (!gui->isFocusCaught()) {
        updateHotkeys();
    }

    auto player = controller->getPlayer();
    auto camera = player->camera;

    bool paused = hud->isPause();
    audio::get_channel("regular")->setPaused(paused);
    audio::get_channel("ambient")->setPaused(paused);
    audio::set_listener(
        camera->position-camera->dir, 
        player->hitbox->velocity,
        camera->dir, 
        camera->up
    );

    if (!hud->isPause()) {
        controller->getLevel()->getWorld()->updateTimers(delta);
        animator->update(delta);
    }
    controller->update(delta, !inputLocked, hud->isPause());
    hud->update(hudVisible);
}

void LevelScreen::draw(float delta) {
    auto camera = controller->getPlayer()->currentCamera;

    Viewport viewport(Window::width, Window::height);
    GfxContext ctx(nullptr, viewport, batch.get());

    worldRenderer->draw(ctx, camera.get(), hudVisible, postProcessing.get());

    if (hudVisible) {
        hud->draw(ctx);
    }
}

void LevelScreen::onEngineShutdown() {
    controller->saveWorld();
}

LevelController* LevelScreen::getLevelController() const {
    return controller.get();
}

