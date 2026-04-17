#include "wx_bgi.h"
#include "wx_bgi_ext.h"

#include <array>
#include <cstdio>

using namespace bgi;

int main()
{
    wxbgi_wx_init_for_canvas(96, 96);
    setbkcolor(BLACK);
    cleardevice();

    constexpr int cols = 4;
    constexpr int rows = 4;
    constexpr int cell = 8;

    const std::array<float, cols * rows> scalar = {{
        -1.0f, -0.5f,  0.0f,  0.5f,
        -0.8f, -0.2f,  0.2f,  0.8f,
        -0.6f, -0.1f,  0.1f,  0.6f,
        -0.4f,  0.0f,  0.4f,  1.0f
    }};

    if (wxbgi_field_draw_scalar_grid(4, 4, cols, rows,
                                     scalar.data(), static_cast<int>(scalar.size()),
                                     cell, -1.0f, 1.0f, WXBGI_FIELD_PALETTE_TURBO) != 1)
    {
        std::fprintf(stderr, "FAIL: scalar grid draw returned error\n");
        return 1;
    }

    const int cA = getpixel(4 + cell / 2, 4 + cell / 2);
    const int cB = getpixel(4 + 3 * cell + cell / 2, 4 + 3 * cell + cell / 2);
    if (cA == BLACK || cB == BLACK || cA == cB)
    {
        std::fprintf(stderr, "FAIL: scalar grid did not paint expected colours (%d, %d)\n", cA, cB);
        return 1;
    }

    cleardevice();
    const std::array<float, cols * rows * 2> vectors = {{
         1.0f,  0.0f,  0.5f,  0.0f,  0.0f,  0.0f, -0.5f,  0.0f,
         0.0f,  1.0f,  0.0f,  0.5f,  0.0f,  0.0f,  0.0f, -0.5f,
         0.7f,  0.7f,  0.4f,  0.4f,  0.0f,  0.0f, -0.4f, -0.4f,
         0.0f,  0.0f, -0.8f,  0.2f,  0.8f, -0.2f,  0.0f,  0.0f
    }};

    if (wxbgi_field_draw_vector_grid(4, 4, cols, rows,
                                     vectors.data(), static_cast<int>(vectors.size()),
                                     cell, 0.7f, 1, WHITE) != 1)
    {
        std::fprintf(stderr, "FAIL: vector grid draw returned error\n");
        return 1;
    }

    if (getpixel(10, 8) != WHITE)
    {
        std::fprintf(stderr, "FAIL: vector glyph not drawn at expected shaft pixel\n");
        return 1;
    }

    cleardevice();
    if (wxbgi_field_draw_scalar_legend(8, 8, 12, 48,
                                       -2.0f, 3.0f,
                                       WXBGI_FIELD_PALETTE_VIRIDIS,
                                       "vel") != 1)
    {
        std::fprintf(stderr, "FAIL: scalar legend draw returned error\n");
        return 1;
    }

    const int legendTop = getpixel(12, 10);
    const int legendBot = getpixel(12, 54);
    if (legendTop == BLACK || legendBot == BLACK || legendTop == legendBot)
    {
        std::fprintf(stderr, "FAIL: scalar legend colours not drawn correctly (%d, %d)\n", legendTop, legendBot);
        return 1;
    }

    std::printf("test_field_vis: field visualisation helpers OK\n");
    return 0;
}
