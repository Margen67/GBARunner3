#pragma once

/// @file
/// Widescreen-like extended view. The GBA scene is drawn shifted by an offset on the DS main engine, such that the
/// parts of the scene that are normally cropped away (left of and above the GBA screen) become visible together with
/// the parts that normally fall outside the GBA screen on the right and bottom. Everything that is shown is drawn
/// by the DS graphics hardware from the state the GBA software set up; nothing is synthesized.
///
/// This works by applying the offset to everything the software positions relative to the screen:
/// background scroll registers, affine background reference points, window coordinates and sprite positions.

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Non-zero when sprite positions are shifted. DMA must not copy to or from OAM behind the
///        memory handlers' back while this is set.
extern u32 gExtendedViewOamActive;

/// @brief Enables the extended view. Must be called after the emulated io registers have been cleared and
///        before the GBA software starts running.
/// @param offsetX Number of pixels to shift the scene to the right (0 to 16).
/// @param offsetY Number of pixels to shift the scene down (0 to 32).
extern void ext_enable(u32 offsetX, u32 offsetY);

#ifdef __cplusplus
}
#endif
