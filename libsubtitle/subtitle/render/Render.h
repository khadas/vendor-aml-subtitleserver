#ifndef __SUBTITLE_RENDER_H__
#define __SUBTITLE_RENDER_H__

#include "Display.h"
#include "sub_types.h"

// Currently, only skia render
// maybe, we also can generic to support multi-render. such as
// openGL render, skia render, customer render...

class Render {
public:
    enum RenderType {
        CALLBACK_RENDER = 0,
        DIRECT_RENDER = 1,
    };

    Render(std::shared_ptr<Display> display){};
    Render() {};
    virtual ~Render() {};

    virtual RenderType getType() = 0;
    // TODO: the subtitle may has some params, config how to render
    //       Need impl later.
    virtual bool showSubtitleItem(std::shared_ptr<AML_SPUVAR> spu,int type) = 0;
    virtual bool hideSubtitleItem(std::shared_ptr<AML_SPUVAR> spu) = 0;
    virtual void resetSubtitleItem() = 0;;
    virtual void removeSubtitleItem(std::shared_ptr<AML_SPUVAR> spu) = 0;
};

#endif
