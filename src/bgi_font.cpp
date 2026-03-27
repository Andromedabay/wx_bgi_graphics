#include "bgi_font.h"

#include "bgi_draw.h"
#include "bgi_state.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

namespace bgi
{

    namespace
    {
        struct Stroke
        {
            int x1;
            int y1;
            int x2;
            int y2;
        };

        struct StrokeGlyph
        {
            int advance;
            const Stroke *strokes;
            std::size_t count;
        };

        struct FontProfile
        {
            int scaleXNum;
            int scaleXDen;
            int scaleYNum;
            int scaleYDen;
            int extraAdvance;
            int thickness;
            int slant;
        };

        constexpr int kGlyphWidth = 10;
        constexpr int kGlyphHeight = 14;
        constexpr int kDefaultAdvance = 12;

        constexpr Stroke kUnknownStrokes[] = {{0, 0, 10, 0}, {10, 0, 10, 14}, {10, 14, 0, 14}, {0, 14, 0, 0}, {0, 0, 10, 14}};
        constexpr Stroke kDashStrokes[] = {{2, 7, 8, 7}};
        constexpr Stroke kUnderscoreStrokes[] = {{0, 14, 10, 14}};
        constexpr Stroke kPeriodStrokes[] = {{5, 13, 5, 13}};
        constexpr Stroke kColonStrokes[] = {{5, 4, 5, 4}, {5, 12, 5, 12}};
        constexpr Stroke kSemicolonStrokes[] = {{5, 4, 5, 4}, {5, 12, 4, 14}};
        constexpr Stroke kCommaStrokes[] = {{5, 12, 4, 14}};
        constexpr Stroke kExclamationStrokes[] = {{5, 0, 5, 10}, {5, 13, 5, 13}};
        constexpr Stroke kQuestionStrokes[] = {{1, 2, 3, 0}, {3, 0, 7, 0}, {7, 0, 9, 2}, {9, 2, 9, 4}, {9, 4, 5, 7}, {5, 9, 5, 9}, {5, 13, 5, 13}};
        constexpr Stroke kSlashStrokes[] = {{0, 14, 10, 0}};
        constexpr Stroke kBackslashStrokes[] = {{0, 0, 10, 14}};
        constexpr Stroke kPlusStrokes[] = {{1, 7, 9, 7}, {5, 3, 5, 11}};
        constexpr Stroke kAsteriskStrokes[] = {{5, 3, 5, 11}, {1, 5, 9, 9}, {1, 9, 9, 5}};
        constexpr Stroke kLeftParenStrokes[] = {{7, 0, 4, 3}, {4, 3, 3, 7}, {3, 7, 4, 11}, {4, 11, 7, 14}};
        constexpr Stroke kRightParenStrokes[] = {{3, 0, 6, 3}, {6, 3, 7, 7}, {7, 7, 6, 11}, {6, 11, 3, 14}};
        constexpr Stroke kLeftBracketStrokes[] = {{7, 0, 3, 0}, {3, 0, 3, 14}, {3, 14, 7, 14}};
        constexpr Stroke kRightBracketStrokes[] = {{3, 0, 7, 0}, {7, 0, 7, 14}, {7, 14, 3, 14}};
        constexpr Stroke kEqualStrokes[] = {{2, 5, 8, 5}, {2, 9, 8, 9}};
        constexpr Stroke kQuoteStrokes[] = {{3, 0, 3, 4}, {7, 0, 7, 4}};

        constexpr Stroke kZeroStrokes[] = {{2, 0, 8, 0}, {8, 0, 10, 2}, {10, 2, 10, 12}, {10, 12, 8, 14}, {8, 14, 2, 14}, {2, 14, 0, 12}, {0, 12, 0, 2}, {0, 2, 2, 0}, {2, 14, 8, 0}};
        constexpr Stroke kOneStrokes[] = {{5, 0, 5, 14}, {3, 2, 5, 0}, {3, 14, 7, 14}};
        constexpr Stroke kTwoStrokes[] = {{1, 2, 3, 0}, {3, 0, 8, 0}, {8, 0, 10, 2}, {10, 2, 10, 4}, {10, 4, 0, 14}, {0, 14, 10, 14}};
        constexpr Stroke kThreeStrokes[] = {{1, 0, 9, 0}, {9, 0, 5, 7}, {5, 7, 9, 14}, {1, 14, 9, 14}, {9, 14, 10, 12}, {10, 12, 10, 10}, {10, 4, 10, 2}, {10, 2, 9, 0}};
        constexpr Stroke kFourStrokes[] = {{8, 0, 8, 14}, {0, 8, 10, 8}, {0, 8, 6, 0}};
        constexpr Stroke kFiveStrokes[] = {{10, 0, 0, 0}, {0, 0, 0, 7}, {0, 7, 8, 7}, {8, 7, 10, 9}, {10, 9, 10, 12}, {10, 12, 8, 14}, {8, 14, 1, 14}};
        constexpr Stroke kSixStrokes[] = {{9, 0, 2, 0}, {2, 0, 0, 7}, {0, 7, 0, 12}, {0, 12, 2, 14}, {2, 14, 8, 14}, {8, 14, 10, 12}, {10, 12, 10, 9}, {10, 9, 8, 7}, {8, 7, 0, 7}};
        constexpr Stroke kSevenStrokes[] = {{0, 0, 10, 0}, {10, 0, 3, 14}};
        constexpr Stroke kEightStrokes[] = {{2, 0, 8, 0}, {8, 0, 10, 2}, {10, 2, 10, 5}, {10, 5, 8, 7}, {8, 7, 2, 7}, {2, 7, 0, 5}, {0, 5, 0, 2}, {0, 2, 2, 0}, {2, 7, 8, 7}, {8, 7, 10, 9}, {10, 9, 10, 12}, {10, 12, 8, 14}, {8, 14, 2, 14}, {2, 14, 0, 12}, {0, 12, 0, 9}, {0, 9, 2, 7}};
        constexpr Stroke kNineStrokes[] = {{10, 7, 2, 7}, {2, 7, 0, 5}, {0, 5, 0, 2}, {0, 2, 2, 0}, {2, 0, 8, 0}, {8, 0, 10, 2}, {10, 2, 10, 14}};

        constexpr Stroke kAStrokes[] = {{0, 14, 5, 0}, {5, 0, 10, 14}, {2, 8, 8, 8}};
        constexpr Stroke kBStrokes[] = {{0, 0, 0, 14}, {0, 0, 7, 0}, {7, 0, 10, 3}, {10, 3, 7, 7}, {7, 7, 0, 7}, {7, 7, 10, 10}, {10, 10, 7, 14}, {7, 14, 0, 14}};
        constexpr Stroke kCStrokes[] = {{10, 1, 8, 0}, {8, 0, 2, 0}, {2, 0, 0, 2}, {0, 2, 0, 12}, {0, 12, 2, 14}, {2, 14, 8, 14}, {8, 14, 10, 13}};
        constexpr Stroke kDStrokes[] = {{0, 0, 0, 14}, {0, 0, 7, 0}, {7, 0, 10, 3}, {10, 3, 10, 11}, {10, 11, 7, 14}, {7, 14, 0, 14}};
        constexpr Stroke kEStrokes[] = {{10, 0, 0, 0}, {0, 0, 0, 14}, {0, 7, 7, 7}, {0, 14, 10, 14}};
        constexpr Stroke kFStrokes[] = {{0, 0, 0, 14}, {0, 0, 10, 0}, {0, 7, 7, 7}};
        constexpr Stroke kGStrokes[] = {{10, 2, 8, 0}, {8, 0, 2, 0}, {2, 0, 0, 2}, {0, 2, 0, 12}, {0, 12, 2, 14}, {2, 14, 8, 14}, {8, 14, 10, 12}, {10, 12, 10, 9}, {10, 9, 6, 9}};
        constexpr Stroke kHStrokes[] = {{0, 0, 0, 14}, {10, 0, 10, 14}, {0, 7, 10, 7}};
        constexpr Stroke kIStrokes[] = {{0, 0, 10, 0}, {5, 0, 5, 14}, {0, 14, 10, 14}};
        constexpr Stroke kJStrokes[] = {{0, 0, 10, 0}, {10, 0, 10, 12}, {10, 12, 8, 14}, {8, 14, 2, 14}, {2, 14, 0, 12}};
        constexpr Stroke kKStrokes[] = {{0, 0, 0, 14}, {10, 0, 0, 7}, {0, 7, 10, 14}};
        constexpr Stroke kLStrokes[] = {{0, 0, 0, 14}, {0, 14, 10, 14}};
        constexpr Stroke kMStrokes[] = {{0, 14, 0, 0}, {0, 0, 5, 7}, {5, 7, 10, 0}, {10, 0, 10, 14}};
        constexpr Stroke kNStrokes[] = {{0, 14, 0, 0}, {0, 0, 10, 14}, {10, 14, 10, 0}};
        constexpr Stroke kOStrokes[] = {{2, 0, 8, 0}, {8, 0, 10, 2}, {10, 2, 10, 12}, {10, 12, 8, 14}, {8, 14, 2, 14}, {2, 14, 0, 12}, {0, 12, 0, 2}, {0, 2, 2, 0}};
        constexpr Stroke kPStrokes[] = {{0, 14, 0, 0}, {0, 0, 8, 0}, {8, 0, 10, 2}, {10, 2, 10, 5}, {10, 5, 8, 7}, {8, 7, 0, 7}};
        constexpr Stroke kQStrokes[] = {{2, 0, 8, 0}, {8, 0, 10, 2}, {10, 2, 10, 12}, {10, 12, 8, 14}, {8, 14, 2, 14}, {2, 14, 0, 12}, {0, 12, 0, 2}, {0, 2, 2, 0}, {6, 10, 10, 14}};
        constexpr Stroke kRStrokes[] = {{0, 14, 0, 0}, {0, 0, 8, 0}, {8, 0, 10, 2}, {10, 2, 10, 5}, {10, 5, 8, 7}, {8, 7, 0, 7}, {0, 7, 10, 14}};
        constexpr Stroke kSStrokes[] = {{10, 1, 8, 0}, {8, 0, 2, 0}, {2, 0, 0, 2}, {0, 2, 0, 5}, {0, 5, 2, 7}, {2, 7, 8, 7}, {8, 7, 10, 9}, {10, 9, 10, 12}, {10, 12, 8, 14}, {8, 14, 0, 14}};
        constexpr Stroke kTStrokes[] = {{0, 0, 10, 0}, {5, 0, 5, 14}};
        constexpr Stroke kUStrokes[] = {{0, 0, 0, 12}, {0, 12, 2, 14}, {2, 14, 8, 14}, {8, 14, 10, 12}, {10, 12, 10, 0}};
        constexpr Stroke kVStrokes[] = {{0, 0, 5, 14}, {5, 14, 10, 0}};
        constexpr Stroke kWStrokes[] = {{0, 0, 2, 14}, {2, 14, 5, 7}, {5, 7, 8, 14}, {8, 14, 10, 0}};
        constexpr Stroke kXStrokes[] = {{0, 0, 10, 14}, {10, 0, 0, 14}};
        constexpr Stroke kYStrokes[] = {{0, 0, 5, 7}, {10, 0, 5, 7}, {5, 7, 5, 14}};
        constexpr Stroke kZStrokes[] = {{0, 0, 10, 0}, {10, 0, 0, 14}, {0, 14, 10, 14}};

        // ── Missing printable ASCII symbols ──────────────────────────────────────

        // # hash: two vertical bars crossed by two horizontal bars
        constexpr Stroke kHashStrokes[] = {
            {3, 2, 3, 12}, {7, 2, 7, 12},
            {1, 5, 9, 5},  {1, 9, 9, 9}};

        // $ dollar: S-curve with vertical bar through centre
        constexpr Stroke kDollarStrokes[] = {
            {5, 0, 5, 14},
            {9, 3, 7, 1}, {7, 1, 3, 1}, {3, 1, 1, 3}, {1, 3, 1, 5}, {1, 5, 3, 7},
            {3, 7, 7, 7},
            {7, 7, 9, 9}, {9, 9, 9, 11}, {9, 11, 7, 13}, {7, 13, 3, 13}, {3, 13, 1, 11}};

        // % percent: upper diamond, diagonal, lower oval
        constexpr Stroke kPercentStrokes[] = {
            {1, 2, 3, 0}, {3, 0, 5, 2}, {5, 2, 3, 5}, {3, 5, 1, 2},
            {0, 14, 10, 0},
            {5, 9, 8, 9}, {8, 9, 10, 11}, {10, 11, 8, 14}, {8, 14, 5, 14}, {5, 14, 4, 11}, {4, 11, 5, 9}};

        // & ampersand
        constexpr Stroke kAmpersandStrokes[] = {
            {7, 0, 9, 2}, {9, 2, 9, 4}, {9, 4, 7, 7},
            {7, 7, 1, 5}, {1, 5, 1, 2}, {1, 2, 3, 0}, {3, 0, 7, 0},
            {7, 7, 9, 10}, {9, 10, 9, 12}, {9, 12, 7, 14},
            {7, 14, 2, 14}, {2, 14, 0, 12}, {0, 12, 0, 9}, {0, 9, 2, 7}, {2, 7, 7, 7}};

        // < less-than
        constexpr Stroke kLessThanStrokes[] = {
            {9, 2, 1, 7}, {1, 7, 9, 12}};

        // > greater-than
        constexpr Stroke kGreaterThanStrokes[] = {
            {1, 2, 9, 7}, {9, 7, 1, 12}};

        // @ at-sign: outer ring + inner counter + exit tail
        constexpr Stroke kAtStrokes[] = {
            {4, 1, 8, 1}, {8, 1, 10, 3}, {10, 3, 10, 11}, {10, 11, 8, 13},
            {8, 13, 4, 13}, {4, 13, 1, 11}, {1, 11, 1, 3}, {1, 3, 4, 1},
            {5, 5, 8, 5}, {8, 5, 8, 10}, {8, 10, 5, 10}, {5, 10, 4, 9}, {4, 9, 4, 6}, {4, 6, 5, 5},
            {8, 11, 10, 13}};

        // ^ caret
        constexpr Stroke kCaretStrokes[] = {
            {1, 7, 5, 1}, {5, 1, 9, 7}};

        // ` backtick
        constexpr Stroke kBacktickStrokes[] = {
            {3, 0, 5, 3}};

        // { left brace
        constexpr Stroke kLeftBraceStrokes[] = {
            {7, 0, 5, 2}, {5, 2, 5, 6}, {5, 6, 3, 7},
            {3, 7, 5, 8}, {5, 8, 5, 12}, {5, 12, 7, 14}};

        // | vertical pipe
        constexpr Stroke kPipeStrokes[] = {
            {5, 0, 5, 14}};

        // } right brace
        constexpr Stroke kRightBraceStrokes[] = {
            {3, 0, 5, 2}, {5, 2, 5, 6}, {5, 6, 7, 7},
            {7, 7, 5, 8}, {5, 8, 5, 12}, {5, 12, 3, 14}};

        // ~ tilde
        constexpr Stroke kTildeStrokes[] = {
            {0, 8, 2, 6}, {2, 6, 5, 8}, {5, 8, 8, 6}, {8, 6, 10, 8}};

        // ── Distinct lowercase letter glyphs ─────────────────────────────────────
        // x-height: y=5..14  ascenders: y=0..14  descender tails reach y=14

        // a – single-story: counter + right stem
        constexpr Stroke kLowerAStrokes[] = {
            {2, 5, 8, 5}, {8, 5, 10, 7}, {10, 7, 10, 14},
            {0, 7, 2, 5}, {0, 7, 0, 12}, {0, 12, 2, 14},
            {2, 14, 8, 14}, {8, 14, 10, 12}};

        // b – full ascender stem + right bowl
        constexpr Stroke kLowerBStrokes[] = {
            {0, 0, 0, 14},
            {0, 7, 8, 7}, {8, 7, 10, 9}, {10, 9, 10, 12},
            {10, 12, 8, 14}, {8, 14, 0, 14}};

        // c – open oval
        constexpr Stroke kLowerCStrokes[] = {
            {10, 7, 8, 5}, {8, 5, 2, 5}, {2, 5, 0, 7},
            {0, 7, 0, 12}, {0, 12, 2, 14},
            {2, 14, 8, 14}, {8, 14, 10, 12}};

        // d – oval + right ascender stem
        constexpr Stroke kLowerDStrokes[] = {
            {10, 0, 10, 14},
            {10, 7, 8, 5}, {8, 5, 2, 5}, {2, 5, 0, 7},
            {0, 7, 0, 12}, {0, 12, 2, 14}, {2, 14, 10, 14}};

        // e – oval with mid-bar, open right
        constexpr Stroke kLowerEStrokes[] = {
            {0, 10, 10, 10},
            {10, 10, 8, 5}, {8, 5, 2, 5}, {2, 5, 0, 7},
            {0, 7, 0, 12}, {0, 12, 2, 14},
            {2, 14, 8, 14}, {8, 14, 10, 12}};

        // f – stem with top hook and crossbar
        constexpr Stroke kLowerFStrokes[] = {
            {5, 0, 5, 14}, {5, 2, 7, 0}, {2, 6, 8, 6}};

        // g – counter + descender tail
        constexpr Stroke kLowerGStrokes[] = {
            {2, 5, 8, 5}, {8, 5, 10, 7},
            {10, 7, 10, 11}, {10, 11, 8, 13}, {8, 13, 2, 13},
            {2, 13, 0, 11}, {0, 11, 0, 7}, {0, 7, 2, 5},
            {10, 7, 10, 14}, {8, 14, 10, 14}};

        // h – left ascender stem + right arch
        constexpr Stroke kLowerHStrokes[] = {
            {0, 0, 0, 14},
            {0, 7, 8, 7}, {8, 7, 10, 9}, {10, 9, 10, 14}};

        // i – dot + stem
        constexpr Stroke kLowerIStrokes[] = {
            {5, 5, 5, 14}, {5, 3, 5, 3}};

        // j – dot + stem + bottom hook (descender)
        constexpr Stroke kLowerJStrokes[] = {
            {5, 5, 5, 12}, {5, 12, 3, 14}, {5, 3, 5, 3}};

        // k – stem + upper arm + lower leg
        constexpr Stroke kLowerKStrokes[] = {
            {0, 0, 0, 14}, {10, 5, 0, 10}, {3, 9, 10, 14}};

        // l – stem with bottom foot
        constexpr Stroke kLowerLStrokes[] = {
            {5, 0, 5, 14}, {5, 14, 8, 14}};

        // m – left leg + two arches
        constexpr Stroke kLowerMStrokes[] = {
            {0, 5, 0, 14}, {0, 5, 5, 5}, {5, 5, 5, 14},
            {5, 5, 10, 5}, {10, 5, 10, 14}};

        // n – left stem + single arch
        constexpr Stroke kLowerNStrokes[] = {
            {0, 5, 0, 14}, {0, 5, 8, 5},
            {8, 5, 10, 7}, {10, 7, 10, 14}};

        // o – closed oval
        constexpr Stroke kLowerOStrokes[] = {
            {2, 5, 8, 5}, {8, 5, 10, 7}, {10, 7, 10, 12},
            {10, 12, 8, 14}, {8, 14, 2, 14}, {2, 14, 0, 12},
            {0, 12, 0, 7}, {0, 7, 2, 5}};

        // p – left stem (descender) + right bowl
        constexpr Stroke kLowerPStrokes[] = {
            {0, 5, 0, 14},
            {0, 5, 8, 5}, {8, 5, 10, 7}, {10, 7, 10, 11},
            {10, 11, 8, 13}, {8, 13, 0, 13}};

        // q – right stem (descender) + left oval
        constexpr Stroke kLowerQStrokes[] = {
            {10, 5, 10, 14},
            {10, 5, 2, 5}, {2, 5, 0, 7}, {0, 7, 0, 11},
            {0, 11, 2, 13}, {2, 13, 10, 13}};

        // r – left stem + short shoulder
        constexpr Stroke kLowerRStrokes[] = {
            {0, 5, 0, 14}, {0, 5, 6, 5}, {6, 5, 8, 7}};

        // s – small s-curve
        constexpr Stroke kLowerSStrokes[] = {
            {9, 6, 7, 5}, {7, 5, 2, 5}, {2, 5, 0, 7},
            {0, 7, 2, 9}, {2, 9, 8, 9}, {8, 9, 10, 11},
            {10, 11, 8, 14}, {8, 14, 2, 14}, {2, 14, 0, 13}};

        // t – stem with crossbar
        constexpr Stroke kLowerTStrokes[] = {
            {5, 0, 5, 14}, {2, 6, 8, 6}};

        // u – two stems joined at bottom
        constexpr Stroke kLowerUStrokes[] = {
            {0, 5, 0, 12}, {0, 12, 2, 14}, {2, 14, 8, 14},
            {8, 14, 10, 12}, {10, 12, 10, 5}};

        // v – two diagonals meeting at bottom
        constexpr Stroke kLowerVStrokes[] = {
            {0, 5, 5, 14}, {5, 14, 10, 5}};

        // w – double-v
        constexpr Stroke kLowerWStrokes[] = {
            {0, 5, 2, 14}, {2, 14, 5, 8}, {5, 8, 8, 14}, {8, 14, 10, 5}};

        // x – crossing diagonals
        constexpr Stroke kLowerXStrokes[] = {
            {0, 5, 10, 14}, {10, 5, 0, 14}};

        // y – arms + descending stem
        constexpr Stroke kLowerYStrokes[] = {
            {0, 5, 5, 10}, {10, 5, 5, 10}, {5, 10, 3, 14}};

        // z – Z-shape at x-height
        constexpr Stroke kLowerZStrokes[] = {
            {0, 5, 10, 5}, {10, 5, 0, 14}, {0, 14, 10, 14}};

        const StrokeGlyph kGlyphUnknown{12, kUnknownStrokes, std::size(kUnknownStrokes)};
        const StrokeGlyph kGlyphSpace{6, nullptr, 0};

        FontProfile currentFontProfile()
        {
            switch (gState.textSettings.font)
            {
            case TRIPLEX_FONT:
                return {11, 10, 11, 10, 2, 2, 0};
            case SMALL_FONT:
                return {8, 10, 8, 10, 1, 1, 0};
            case SANS_SERIF_FONT:
                return {12, 10, 10, 10, 2, 1, 0};
            case GOTHIC_FONT:
                return {12, 10, 11, 10, 2, 2, 2};
            case DEFAULT_FONT:
            default:
                return {10, 10, 10, 10, 2, 1, 0};
            }
        }

        int scaledValue(int value, int numerator, int denominator)
        {
            return std::max(1, (value * numerator + denominator - 1) / denominator);
        }

        int effectiveScaleX()
        {
            const auto profile = currentFontProfile();
            return scaledValue(currentTextScaleX(), profile.scaleXNum, profile.scaleXDen);
        }

        int effectiveScaleY()
        {
            const auto profile = currentFontProfile();
            return scaledValue(currentTextScaleY(), profile.scaleYNum, profile.scaleYDen);
        }

        int glyphAdvance(const StrokeGlyph &glyph)
        {
            const auto profile = currentFontProfile();
            return glyph.advance * effectiveScaleX() + profile.extraAdvance;
        }

        std::pair<int, int> transformPoint(int baseX, int baseY, int px, int py)
        {
            const auto profile = currentFontProfile();
            const int localX = px * effectiveScaleX() + ((kGlyphHeight - py) * profile.slant) / kGlyphHeight;
            const int localY = py * effectiveScaleY();
            const int rotatedHeight = kGlyphHeight * effectiveScaleY();

            if (gState.textSettings.direction == VERT_DIR)
            {
                return {baseX + (rotatedHeight - localY), baseY + localX};
            }

            return {baseX + localX, baseY + localY};
        }

        void plotThickPixel(int x, int y, int color, int thickness)
        {
            const int radius = std::max(0, thickness - 1);
            for (int offsetY = -radius; offsetY <= radius; ++offsetY)
            {
                for (int offsetX = -radius; offsetX <= radius; ++offsetX)
                {
                    setPixelWithMode(x + offsetX, y + offsetY, color, gState.writeMode);
                }
            }
        }

        void drawStrokeLine(const std::pair<int, int> &start, const std::pair<int, int> &finish, int color, int thickness)
        {
            int x1 = start.first;
            int y1 = start.second;
            const int x2 = finish.first;
            const int y2 = finish.second;

            int dx = std::abs(x2 - x1);
            int sx = x1 < x2 ? 1 : -1;
            int dy = -std::abs(y2 - y1);
            int sy = y1 < y2 ? 1 : -1;
            int err = dx + dy;

            while (true)
            {
                plotThickPixel(x1, y1, color, thickness);
                if (x1 == x2 && y1 == y2)
                {
                    break;
                }

                const int e2 = err * 2;
                if (e2 >= dy)
                {
                    err += dy;
                    x1 += sx;
                }
                if (e2 <= dx)
                {
                    err += dx;
                    y1 += sy;
                }
            }
        }

        StrokeGlyph glyphForChar(unsigned char c)
        {
            switch (c)
            {
            // ── Whitespace ────────────────────────────────────────────────────────
            case ' ':
                return kGlyphSpace;

            // ── Punctuation / operators already defined ───────────────────────────
            case '!':
                return {6, kExclamationStrokes, std::size(kExclamationStrokes)};
            case '"':
                return {8, kQuoteStrokes, std::size(kQuoteStrokes)};
            case '\'':
                return {6, kQuoteStrokes, std::size(kQuoteStrokes)};
            case '(':
                return {8, kLeftParenStrokes, std::size(kLeftParenStrokes)};
            case ')':
                return {8, kRightParenStrokes, std::size(kRightParenStrokes)};
            case '*':
                return {12, kAsteriskStrokes, std::size(kAsteriskStrokes)};
            case '+':
                return {12, kPlusStrokes, std::size(kPlusStrokes)};
            case ',':
                return {6, kCommaStrokes, std::size(kCommaStrokes)};
            case '-':
                return {10, kDashStrokes, std::size(kDashStrokes)};
            case '.':
                return {6, kPeriodStrokes, std::size(kPeriodStrokes)};
            case '/':
                return {10, kSlashStrokes, std::size(kSlashStrokes)};
            case ':':
                return {6, kColonStrokes, std::size(kColonStrokes)};
            case ';':
                return {6, kSemicolonStrokes, std::size(kSemicolonStrokes)};
            case '=':
                return {12, kEqualStrokes, std::size(kEqualStrokes)};
            case '?':
                return {12, kQuestionStrokes, std::size(kQuestionStrokes)};
            case '[':
                return {8, kLeftBracketStrokes, std::size(kLeftBracketStrokes)};
            case '\\':
                return {10, kBackslashStrokes, std::size(kBackslashStrokes)};
            case ']':
                return {8, kRightBracketStrokes, std::size(kRightBracketStrokes)};
            case '_':
                return {10, kUnderscoreStrokes, std::size(kUnderscoreStrokes)};

            // ── Newly added symbols ───────────────────────────────────────────────
            case '#':
                return {12, kHashStrokes, std::size(kHashStrokes)};
            case '$':
                return {12, kDollarStrokes, std::size(kDollarStrokes)};
            case '%':
                return {14, kPercentStrokes, std::size(kPercentStrokes)};
            case '&':
                return {12, kAmpersandStrokes, std::size(kAmpersandStrokes)};
            case '<':
                return {10, kLessThanStrokes, std::size(kLessThanStrokes)};
            case '>':
                return {10, kGreaterThanStrokes, std::size(kGreaterThanStrokes)};
            case '@':
                return {14, kAtStrokes, std::size(kAtStrokes)};
            case '^':
                return {10, kCaretStrokes, std::size(kCaretStrokes)};
            case '`':
                return {6, kBacktickStrokes, std::size(kBacktickStrokes)};
            case '{':
                return {8, kLeftBraceStrokes, std::size(kLeftBraceStrokes)};
            case '|':
                return {6, kPipeStrokes, std::size(kPipeStrokes)};
            case '}':
                return {8, kRightBraceStrokes, std::size(kRightBraceStrokes)};
            case '~':
                return {10, kTildeStrokes, std::size(kTildeStrokes)};

            // ── Digits ────────────────────────────────────────────────────────────
            case '0':
                return {12, kZeroStrokes, std::size(kZeroStrokes)};
            case '1':
                return {10, kOneStrokes, std::size(kOneStrokes)};
            case '2':
                return {12, kTwoStrokes, std::size(kTwoStrokes)};
            case '3':
                return {12, kThreeStrokes, std::size(kThreeStrokes)};
            case '4':
                return {12, kFourStrokes, std::size(kFourStrokes)};
            case '5':
                return {12, kFiveStrokes, std::size(kFiveStrokes)};
            case '6':
                return {12, kSixStrokes, std::size(kSixStrokes)};
            case '7':
                return {12, kSevenStrokes, std::size(kSevenStrokes)};
            case '8':
                return {12, kEightStrokes, std::size(kEightStrokes)};
            case '9':
                return {12, kNineStrokes, std::size(kNineStrokes)};

            // ── Uppercase letters ─────────────────────────────────────────────────
            case 'A':
                return {12, kAStrokes, std::size(kAStrokes)};
            case 'B':
                return {12, kBStrokes, std::size(kBStrokes)};
            case 'C':
                return {12, kCStrokes, std::size(kCStrokes)};
            case 'D':
                return {12, kDStrokes, std::size(kDStrokes)};
            case 'E':
                return {12, kEStrokes, std::size(kEStrokes)};
            case 'F':
                return {12, kFStrokes, std::size(kFStrokes)};
            case 'G':
                return {12, kGStrokes, std::size(kGStrokes)};
            case 'H':
                return {12, kHStrokes, std::size(kHStrokes)};
            case 'I':
                return {8, kIStrokes, std::size(kIStrokes)};
            case 'J':
                return {12, kJStrokes, std::size(kJStrokes)};
            case 'K':
                return {12, kKStrokes, std::size(kKStrokes)};
            case 'L':
                return {11, kLStrokes, std::size(kLStrokes)};
            case 'M':
                return {14, kMStrokes, std::size(kMStrokes)};
            case 'N':
                return {13, kNStrokes, std::size(kNStrokes)};
            case 'O':
                return {12, kOStrokes, std::size(kOStrokes)};
            case 'P':
                return {12, kPStrokes, std::size(kPStrokes)};
            case 'Q':
                return {12, kQStrokes, std::size(kQStrokes)};
            case 'R':
                return {12, kRStrokes, std::size(kRStrokes)};
            case 'S':
                return {12, kSStrokes, std::size(kSStrokes)};
            case 'T':
                return {12, kTStrokes, std::size(kTStrokes)};
            case 'U':
                return {12, kUStrokes, std::size(kUStrokes)};
            case 'V':
                return {12, kVStrokes, std::size(kVStrokes)};
            case 'W':
                return {14, kWStrokes, std::size(kWStrokes)};
            case 'X':
                return {12, kXStrokes, std::size(kXStrokes)};
            case 'Y':
                return {12, kYStrokes, std::size(kYStrokes)};
            case 'Z':
                return {12, kZStrokes, std::size(kZStrokes)};

            // ── Lowercase letters (distinct glyphs, not mapped to uppercase) ──────
            case 'a':
                return {11, kLowerAStrokes, std::size(kLowerAStrokes)};
            case 'b':
                return {11, kLowerBStrokes, std::size(kLowerBStrokes)};
            case 'c':
                return {10, kLowerCStrokes, std::size(kLowerCStrokes)};
            case 'd':
                return {11, kLowerDStrokes, std::size(kLowerDStrokes)};
            case 'e':
                return {10, kLowerEStrokes, std::size(kLowerEStrokes)};
            case 'f':
                return {8, kLowerFStrokes, std::size(kLowerFStrokes)};
            case 'g':
                return {11, kLowerGStrokes, std::size(kLowerGStrokes)};
            case 'h':
                return {11, kLowerHStrokes, std::size(kLowerHStrokes)};
            case 'i':
                return {6, kLowerIStrokes, std::size(kLowerIStrokes)};
            case 'j':
                return {7, kLowerJStrokes, std::size(kLowerJStrokes)};
            case 'k':
                return {10, kLowerKStrokes, std::size(kLowerKStrokes)};
            case 'l':
                return {6, kLowerLStrokes, std::size(kLowerLStrokes)};
            case 'm':
                return {13, kLowerMStrokes, std::size(kLowerMStrokes)};
            case 'n':
                return {11, kLowerNStrokes, std::size(kLowerNStrokes)};
            case 'o':
                return {11, kLowerOStrokes, std::size(kLowerOStrokes)};
            case 'p':
                return {11, kLowerPStrokes, std::size(kLowerPStrokes)};
            case 'q':
                return {11, kLowerQStrokes, std::size(kLowerQStrokes)};
            case 'r':
                return {8, kLowerRStrokes, std::size(kLowerRStrokes)};
            case 's':
                return {10, kLowerSStrokes, std::size(kLowerSStrokes)};
            case 't':
                return {8, kLowerTStrokes, std::size(kLowerTStrokes)};
            case 'u':
                return {11, kLowerUStrokes, std::size(kLowerUStrokes)};
            case 'v':
                return {11, kLowerVStrokes, std::size(kLowerVStrokes)};
            case 'w':
                return {13, kLowerWStrokes, std::size(kLowerWStrokes)};
            case 'x':
                return {10, kLowerXStrokes, std::size(kLowerXStrokes)};
            case 'y':
                return {11, kLowerYStrokes, std::size(kLowerYStrokes)};
            case 'z':
                return {10, kLowerZStrokes, std::size(kLowerZStrokes)};

            default:
                return kGlyphUnknown;
            }
        }
    } // namespace

    int currentTextScaleX()
    {
        const int base = std::max(1, gState.textSettings.charsize);
        const int numerator = std::max(1, gState.userCharSizeXNum);
        const int denominator = std::max(1, gState.userCharSizeXDen);
        return std::max(1, (base * numerator) / denominator);
    }

    int currentTextScaleY()
    {
        const int base = std::max(1, gState.textSettings.charsize);
        const int numerator = std::max(1, gState.userCharSizeYNum);
        const int denominator = std::max(1, gState.userCharSizeYDen);
        return std::max(1, (base * numerator) / denominator);
    }

    std::pair<int, int> measureText(const std::string &text)
    {
        if (text.empty())
        {
            return {0, 0};
        }

        int totalAdvance = 0;
        int maxAdvance = 0;
        for (unsigned char ch : text)
        {
            const StrokeGlyph glyph = glyphForChar(ch);
            const int advance = glyphAdvance(glyph);
            totalAdvance += advance;
            maxAdvance = std::max(maxAdvance, advance);
        }

        const int glyphWidth = kGlyphWidth * effectiveScaleX() + currentFontProfile().slant;
        const int glyphHeight = kGlyphHeight * effectiveScaleY();

        if (gState.textSettings.direction == VERT_DIR)
        {
            return {glyphHeight, totalAdvance - std::max(0, currentFontProfile().extraAdvance)};
        }

        return {std::max(glyphWidth, totalAdvance - std::max(0, currentFontProfile().extraAdvance)), std::max(glyphHeight, maxAdvance)};
    }

    void drawGlyph(int x, int y, unsigned char c, int color)
    {
        const StrokeGlyph glyph = glyphForChar(c);
        const int thickness = std::max(1, currentFontProfile().thickness * std::max(currentTextScaleX(), currentTextScaleY()));
        for (std::size_t index = 0; index < glyph.count; ++index)
        {
            const auto &stroke = glyph.strokes[index];
            const auto start = transformPoint(x, y, stroke.x1, stroke.y1);
            const auto finish = transformPoint(x, y, stroke.x2, stroke.y2);
            drawStrokeLine(start, finish, color, thickness);
        }
    }

    void drawText(int x, int y, const std::string &text, int color)
    {
        int penX = x;
        int penY = y;

        for (unsigned char ch : text)
        {
            drawGlyph(penX, penY, ch, color);
            const int advance = glyphAdvance(glyphForChar(ch));
            if (gState.textSettings.direction == VERT_DIR)
            {
                penY += advance;
            }
            else
            {
                penX += advance;
            }
        }
    }

} // namespace bgi
