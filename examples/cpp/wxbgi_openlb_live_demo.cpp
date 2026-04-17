#include "wx_bgi.h"
#include "wx_bgi_ext.h"
#include "wx_bgi_openlb.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

using namespace bgi;

namespace
{
    constexpr int   kCols      = 96;
    constexpr int   kRows      = 64;
    constexpr int   kCell      = 6;
    constexpr int   kLegendW   = 20;
    constexpr int   kHudLeft   = kCols * kCell + 12;
    constexpr int   kWindowW   = kCols * kCell + 170;
    constexpr int   kWindowH   = kRows * kCell + 24;
    constexpr float kScalarMin = -1.2f;
    constexpr float kScalarMax =  1.2f;

    void buildMockField(double t, std::vector<float>& scalar, std::vector<float>& vectors)
    {
        for (int row = 0; row < kRows; ++row)
        {
            for (int col = 0; col < kCols; ++col)
            {
                const float x = (static_cast<float>(col) / static_cast<float>(kCols - 1)) * 2.0f - 1.0f;
                const float y = (static_cast<float>(row) / static_cast<float>(kRows - 1)) * 2.0f - 1.0f;
                const float r2 = x * x + y * y;
                const float swirl = std::sin(static_cast<float>(3.2 * t) - r2 * 7.0f);
                scalar[static_cast<std::size_t>(row * kCols + col)] =
                    0.75f * swirl + 0.25f * std::cos(5.0f * x - static_cast<float>(t));

                const float vx = -y * (0.55f + 0.25f * std::cos(static_cast<float>(t) + x * 3.0f));
                const float vy =  x * (0.55f + 0.25f * std::sin(static_cast<float>(t) + y * 3.0f));
                const std::size_t idx = static_cast<std::size_t>((row * kCols + col) * 2);
                vectors[idx + 0] = vx;
                vectors[idx + 1] = vy;
            }
        }
    }

    void drawHud(int frame)
    {
        static char kTitle[]   = "OpenLB-style live loop";
        static char kLine1[]   = "step solver -> draw -> pump GUI";
        static char kLine2[]   = "uses standalone wx + wxbgi_poll_events()";
        static char kScalar[]  = "scalar: mock velocity magnitude";
        static char kVector[]  = "vectors: decimated flow arrows";

        setcolor(WHITE);
        outtextxy(kHudLeft, 12, kTitle);
        setcolor(LIGHTGRAY);
        outtextxy(kHudLeft, 28, kLine1);
        outtextxy(kHudLeft, 44, kLine2);

        char line[64] = {};
        std::snprintf(line, sizeof(line), "frame: %d", frame);
        outtextxy(kHudLeft, 74, line);

        outtextxy(kHudLeft, 100, kScalar);
        outtextxy(kHudLeft, 116, kVector);
    }
}

int main(int argc, char** argv)
{
    const bool testMode = (argc > 1 && std::strcmp(argv[1], "--test") == 0);

    wxbgi_openlb_begin_session(kWindowW, kWindowH, "wx_bgi OpenLB Live Loop Demo");
    setbkcolor(BLACK);
    cleardevice();

    std::vector<float> scalar(static_cast<std::size_t>(kCols * kRows));
    std::vector<float> vectors(static_cast<std::size_t>(kCols * kRows * 2));

    const int maxFrames = testMode ? 4 : 1000000;
    int frame = 0;
    while (frame < maxFrames && wxbgi_openlb_pump())
    {
        const double t = static_cast<double>(frame) * (testMode ? 0.12 : 0.04);
        buildMockField(t, scalar, vectors);

        cleardevice();
        wxbgi_field_draw_scalar_grid(8, 8, kCols, kRows,
                                     scalar.data(), static_cast<int>(scalar.size()),
                                     kCell, kScalarMin, kScalarMax,
                                     WXBGI_FIELD_PALETTE_TURBO);
        wxbgi_field_draw_vector_grid(8, 8, kCols, kRows,
                                     vectors.data(), static_cast<int>(vectors.size()),
                                     kCell, 0.7f, 5, WHITE);
        wxbgi_field_draw_scalar_legend(kCols * kCell + 18, 12,
                                       kLegendW, kRows * kCell - 24,
                                       kScalarMin, kScalarMax,
                                       WXBGI_FIELD_PALETTE_TURBO,
                                       "|u|");
        drawHud(frame);

        if (!wxbgi_openlb_present())
            break;

        if (!testMode)
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        ++frame;
    }

    std::printf("wxbgi_openlb_live_demo: interactive loop OK\n");
    return 0;
}
