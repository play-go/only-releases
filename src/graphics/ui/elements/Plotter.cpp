#include "Plotter.hpp"

#include "../../core/Batch2D.hpp"
#include "../../core/Font.hpp"
#include "../../core/GfxContext.hpp"
#include "../../../assets/Assets.h"
#include "../../../util/stringutil.h"

using namespace gui;

void Plotter::act(float delta) {
    index = index + 1 % dmwidth;
    int value = static_cast<int>(delta * multiplier);
    points[index % dmwidth] = std::min(value, dmheight);
}

void Plotter::draw(const GfxContext* pctx, Assets* assets) {
    glm::vec2 pos = calcPos();
    auto batch = pctx->getBatch2D();
    batch->texture(nullptr);
    batch->lineWidth(1);
    for (int i = index+1; i < index+dmwidth; i++) {
        int j = i % dmwidth;
        batch->line(
            pos.x + i - index, pos.y + size.y - points[j], 
            pos.x + i - index, pos.y + size.y, 1.0f, 1.0f, 1.0f, 0.2f
        );
    }
    batch->setColor({1,1,1,0.2f});
    batch->lineRect(pos.x, pos.y, dmwidth, dmheight);
    for (int y = 0; y < dmheight; y += 16) {
        batch->line(
            pos.x+dmwidth-4, pos.y+dmheight-y,
            pos.x+dmwidth+4, pos.y+dmheight-y,
            1.0f, 1.0f, 1.0f, 0.2f
        );
    }

    int current_point = static_cast<int>(points[index % dmwidth]);
    auto font = assets->getFont("normal");
    for (int y = 0; y < dmheight; y += labelsInterval) {
        std::wstring string;
        if (current_point/16 == y/labelsInterval) {
            batch->setColor({1,1,1,0.5f});
            string = util::to_wstring(current_point / multiplier, 3);
        } else {
            batch->setColor({1,1,1,0.2f});
            string = util::to_wstring(y / multiplier, 3);
        }
        font->draw(batch, string, pos.x+dmwidth+2, pos.y+dmheight-y-labelsInterval);
    }
}
