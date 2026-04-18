#pragma once

#include "wx_bgi.h"
#include "wx_bgi_ext.h"

/**
 * @file wx_bgi_openlb.h
 * @brief Convenience wrappers for driving wx_bgi from an OpenLB-style live loop.
 */

static inline void wxbgi_openlb_begin_session(int width, int height, const char* title)
{
    wxbgi_wx_app_create();
    wxbgi_wx_frame_create(width, height, title);
    wxbgi_wx_set_frame_rate(0);
}

static inline int wxbgi_openlb_pump(void)
{
    if (wxbgi_should_close() != 0)
        return 0;
    wxbgi_poll_events();
    return wxbgi_should_close() == 0 ? 1 : 0;
}

static inline int wxbgi_openlb_present(void)
{
    wxbgi_wx_refresh();
    return wxbgi_openlb_pump();
}

BGI_API int BGI_CALL wxbgi_openlb_classify_point_material(float x, float y, float z,
                                                          int defaultFluidMaterial,
                                                          int defaultSolidMaterial,
                                                          int *outMatched);

BGI_API int BGI_CALL wxbgi_openlb_sample_materials_2d(float minX, float minY, float z,
                                                      int cols, int rows,
                                                      float stepX, float stepY,
                                                      int defaultFluidMaterial,
                                                      int defaultSolidMaterial,
                                                      int *outMaterials,
                                                      int materialCount);
