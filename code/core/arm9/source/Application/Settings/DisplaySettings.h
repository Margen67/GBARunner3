#pragma once
#include "Enums/GbaScreen.h"
#include "Enums/GbaColorCorrection.h"
#include "Enums/GbaBorderImage.h"

#define DISPLAY_SETTINGS_GBA_SCREEN_BRIGHTNESS_MIN  1
#define DISPLAY_SETTINGS_GBA_SCREEN_BRIGHTNESS_MAX  16

#define DISPLAY_SETTINGS_GBA_DISPLAY_GAMMA_MIN      0
#define DISPLAY_SETTINGS_GBA_DISPLAY_GAMMA_MAX      4

class DisplaySettings
{
public:
    /// @brief Specifies the DS screen to display the GBA game on.
    GbaScreen gbaScreen = GbaScreen::Top;

    /// @brief Specifies the type of color correction to use.
    GbaColorCorrection gbaColorCorrection = GbaColorCorrection::None;

    /// @brief Specifies the precomputed display gamma setting if color correction is enabled.
    ///        Should be a value between 0 (brightest) and 4 (darkest).
    u16 gbaDisplayGamma = DISPLAY_SETTINGS_GBA_DISPLAY_GAMMA_MIN;

    /// @brief Specifies the master brightness setting to use for the display the GBA game on.
    ///        Should be a value between 1 (darkest) and 16 (brightest).
    u16 gbaScreenBrightness = DISPLAY_SETTINGS_GBA_SCREEN_BRIGHTNESS_MAX;

    /// @brief Specifies whether centering and masking of the GBA screen should be enabled.
    ///        This requires display capture with double buffering and adds 1 frame of latency.
    bool16 enableCenterAndMask = true;

    /// @brief Horizontal centering offset to be used when enableCenterAndMask is true.
    u16 centerOffsetX = 8;

    /// @brief Vertical centering offset to be used when enableCenterAndMask is true.
    u16 centerOffsetY = 16;

    /// @brief Width of the visible screen area when enableCenterAndMask is true.
    u16 maskWidth = 240;

    /// @brief Height of the visible screen area when enableCenterAndMask is true.
    u16 maskHeight = 160;

    /// @brief Type of border image to be used when enableCenterAndMask is true.
    GbaBorderImage borderImage = GbaBorderImage::Game;

    /// @brief Specifies whether the visible area should be extended like a widescreen hack. The GBA screen is
    ///        shifted by centerOffsetX and centerOffsetY on the DS screen and the surrounding area is drawn
    ///        by the hardware too, from whatever the GBA software has in its backgrounds and sprites there.
    ///        Because there is no capture involved, there is no added latency, no mask and no border.
    ///        When this is true enableCenterAndMask is ignored. Offsets are limited to 16 horizontally and
    ///        32 vertically. Per scanline effects (hblank dma or irq) end up offset vertically by centerOffsetY
    ///        lines compared to the picture, so use a smaller centerOffsetY for games that depend on those.
    bool16 extendVisibleArea = false;

    /// @brief Returns whether the GBA screen is shown by capturing it and drawing it again on the other
    ///        screen engine (center and mask), as opposed to directly displaying the main engine.
    bool UsesCaptureCentering() const
    {
        return enableCenterAndMask && !extendVisibleArea;
    }
};
