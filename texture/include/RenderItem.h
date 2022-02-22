#ifndef __RENDERITEM_H__
#define __RENDERITEM_H__

#include "Model.h"

class RenderItem
{
    std::unique_ptr<Model> m_model;
private:

public:
    RenderItem();
    ~RenderItem();
};

#endif