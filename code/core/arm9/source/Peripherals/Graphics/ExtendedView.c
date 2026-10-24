#include "common.h"
#include "Emulator/IoRegisters.h"
#include "MemoryEmulator/MemoryLoadStore.h"
#include "GbaIoRegOffsets.h"
#include "ExtendedView.h"

#define EXT_MAX_OFFSET_X    16
#define EXT_MAX_OFFSET_Y    32

#define EXT_GBA_LCD_WIDTH   240
#define EXT_GBA_LCD_HEIGHT  160

#define EXT_REG_VCOUNT      (*(vu16*)0x04000006)
#define EXT_REG_BG_AFFINE   0x04000028 // BG2X; BG3X is 0x10 further

typedef struct
{
    s32 offsetX;
    s32 offsetY;

    // what the gba software wrote to REG_BGxPA to REG_BGxPD, for BG2 and BG3
    s16 matrix[2][4];

    // what the gba software wrote to REG_BGxX and REG_BGxY, for BG2 and BG3
    u32 ref[2][2];
} ext_state_t;

static ext_state_t sExt;

u32 gExtendedViewOamActive;

// These are the asm wrappers from ExtendedViewAsm.s. They are what ends up in the memory handler tables.
extern void ext_store16ScrollHandler(void);
extern void ext_store32ScrollHandler(void);
extern void ext_store16AffineMatrixHandler(void);
extern void ext_store32AffineMatrixHandler(void);
extern void ext_store16AffineRefHandler(void);
extern void ext_store32AffineRefHandler(void);
extern void ext_store16WindowHandler(void);
extern void ext_store32WindowHandler(void);
extern void ext_load8OamHandler(void);
extern void ext_load16OamHandler(void);
extern void ext_load32OamHandler(void);
extern void ext_store16OamHandler(void);
extern void ext_store32OamHandler(void);

extern u16 memu_store16IoTable[];
extern u16 memu_store32IoTable[];

#define EXT_ITCM [[gnu::section(".itcm")]]

// ------------------------------------------------------------------------------------------------
// Background scroll
// ------------------------------------------------------------------------------------------------

static inline u32 ext_adjustScroll(u32 value, s32 offset)
{
    // the scroll registers are 9 bit and the backgrounds wrap, so subtracting is enough
    return (value - offset) & 0x1FF;
}

EXT_ITCM void ext_store16Scroll(u32 address, u32 value)
{
    u32 reg = address & 0xFF;
    *(u16*)&emu_ioRegisters[reg] = value;
    // REG_BGxHOFS is at 0x10, 0x14, ... and REG_BGxVOFS at 0x12, 0x16, ...
    s32 offset = (reg & 2) ? sExt.offsetY : sExt.offsetX;
    *(vu16*)address = ext_adjustScroll(value, offset);
}

EXT_ITCM void ext_store32Scroll(u32 address, u32 value)
{
    u32 reg = address & 0xFF;
    *(u32*)&emu_ioRegisters[reg] = value;
    *(vu32*)address =
        ext_adjustScroll(value & 0xFFFF, sExt.offsetX) |
        (ext_adjustScroll(value >> 16, sExt.offsetY) << 16);
}

// ------------------------------------------------------------------------------------------------
// Affine backgrounds
// ------------------------------------------------------------------------------------------------

/// The reference point is where in the background the top left screen pixel is taken from. Moving the scene to
/// the right and down by an offset means taking the top left pixel from the background position that is
/// (offsetX, offsetY) screen pixels before where the software wants it.
static void ext_writeRef(u32 bg, u32 axis)
{
    // sign extend the 28 bit value (20.8 fixed point)
    s32 value = (s32)(sExt.ref[bg][axis] << 4) >> 4;
    value -= sExt.offsetX * sExt.matrix[bg][axis * 2] + sExt.offsetY * sExt.matrix[bg][axis * 2 + 1];
    *(vu32*)(EXT_REG_BG_AFFINE + bg * 0x10 + axis * 4) = (u32)value & 0x0FFFFFFF;
}

/// Writing the reference point makes the hardware restart its line by line stepping from the new value. So when
/// the matrix changes while the picture is being drawn the reference point must be left alone. Outside of that
/// it is updated, such that a matrix that is written after the reference point (or a reference point that is
/// never written at all, like bitmap modes usually do) still gets the offset applied.
static inline u32 ext_canRewriteRefs(void)
{
    return (emu_ioRegisters[GBA_REG_OFFS_DISPCNT] & 0x80) || EXT_REG_VCOUNT >= EXT_GBA_LCD_HEIGHT;
}

EXT_ITCM void ext_store16AffineMatrix(u32 address, u32 value)
{
    u32 reg = address & 0xFF;
    u32 bg = (reg >> 4) & 1;        // 0x20: BG2, 0x30: BG3
    u32 index = (reg >> 1) & 3;     // PA, PB, PC, PD
    *(u16*)&emu_ioRegisters[reg] = value;
    sExt.matrix[bg][index] = (s16)value;
    *(vu16*)address = value;
    if (ext_canRewriteRefs())
    {
        ext_writeRef(bg, 0);
        ext_writeRef(bg, 1);
    }
}

EXT_ITCM void ext_store32AffineMatrix(u32 address, u32 value)
{
    u32 reg = address & 0xFF;
    u32 bg = (reg >> 4) & 1;
    u32 index = (reg >> 1) & 3;     // 0 for PA and PB, 2 for PC and PD
    *(u32*)&emu_ioRegisters[reg] = value;
    sExt.matrix[bg][index] = (s16)value;
    sExt.matrix[bg][index + 1] = (s16)(value >> 16);
    *(vu32*)address = value;
    if (ext_canRewriteRefs())
    {
        ext_writeRef(bg, 0);
        ext_writeRef(bg, 1);
    }
}

EXT_ITCM void ext_store16AffineRef(u32 address, u32 value)
{
    u32 reg = address & 0xFF;
    u32 bg = (reg >> 4) & 1;        // 0x28 and 0x2C: BG2, 0x38 and 0x3C: BG3
    u32 axis = (reg >> 2) & 1;      // X or Y
    u32 high = (reg >> 1) & 1;      // low or high half
    u32 raw = sExt.ref[bg][axis];
    if (high)
        raw = (raw & 0x0000FFFF) | (value << 16);
    else
        raw = (raw & 0xFFFF0000) | value;
    sExt.ref[bg][axis] = raw;
    *(u16*)&emu_ioRegisters[reg] = value;
    ext_writeRef(bg, axis);
}

EXT_ITCM void ext_store32AffineRef(u32 address, u32 value)
{
    u32 reg = address & 0xFF;
    u32 bg = (reg >> 4) & 1;
    u32 axis = (reg >> 2) & 1;
    sExt.ref[bg][axis] = value;
    *(u32*)&emu_ioRegisters[reg] = value;
    ext_writeRef(bg, axis);
}

// ------------------------------------------------------------------------------------------------
// Windows
// ------------------------------------------------------------------------------------------------

static inline u32 ext_clampByte(u32 value)
{
    return value > 255 ? 255 : value;
}

/// Horizontal window register: left edge in the high byte, right edge (exclusive) in the low byte.
static u32 ext_adjustWindowH(u32 value)
{
    u32 left = (value >> 8) & 0xFF;
    u32 right = value & 0xFF;
    if (right > EXT_GBA_LCD_WIDTH)
        right = EXT_GBA_LCD_WIDTH;
    return (ext_clampByte(left + sExt.offsetX) << 8) | ext_clampByte(right + sExt.offsetX);
}

/// Vertical window register: top edge in the high byte, bottom edge (exclusive) in the low byte.
static u32 ext_adjustWindowV(u32 value)
{
    u32 top = (value >> 8) & 0xFF;
    u32 bottom = value & 0xFF;
    if (bottom > EXT_GBA_LCD_HEIGHT)
        bottom = EXT_GBA_LCD_HEIGHT;
    return (ext_clampByte(top + sExt.offsetY) << 8) | ext_clampByte(bottom + sExt.offsetY);
}

static inline u32 ext_adjustWindow(u32 reg, u32 value)
{
    // 0x40 and 0x42 are REG_WIN0H and REG_WIN1H, 0x44 and 0x46 are REG_WIN0V and REG_WIN1V
    return (reg & 4) ? ext_adjustWindowV(value) : ext_adjustWindowH(value);
}

EXT_ITCM void ext_store16Window(u32 address, u32 value)
{
    u32 reg = address & 0xFF;
    *(u16*)&emu_ioRegisters[reg] = value;
    *(vu16*)address = ext_adjustWindow(reg, value);
}

EXT_ITCM void ext_store32Window(u32 address, u32 value)
{
    u32 reg = address & 0xFF;
    *(u32*)&emu_ioRegisters[reg] = value;
    *(vu32*)address =
        ext_adjustWindow(reg, value & 0xFFFF) |
        (ext_adjustWindow(reg + 2, value >> 16) << 16);
}

// ------------------------------------------------------------------------------------------------
// Sprites
// ------------------------------------------------------------------------------------------------

/// Applies the offset to (direction = 1), or removes it from (direction = -1), a halfword of an oam entry.
/// reg is the offset within the entry: 0 is attribute 0 (y in the low 8 bits), 2 is attribute 1 (x in the low 9 bits).
static inline u32 ext_oamTransform16(u32 reg, u32 value, s32 direction)
{
    if (reg == 0)
        return (value & 0xFF00) | ((value + direction * sExt.offsetY) & 0x00FF);
    if (reg == 2)
        return (value & 0xFE00) | ((value + direction * sExt.offsetX) & 0x01FF);
    return value;
}

EXT_ITCM void ext_store16Oam(u32 address, u32 value)
{
    *(vu16*)(0x07000000 | (address & 0x3FE)) = ext_oamTransform16(address & 6, value, 1);
}

EXT_ITCM void ext_store32Oam(u32 address, u32 value)
{
    if (!(address & 4))
    {
        value = ext_oamTransform16(0, value & 0xFFFF, 1) | (ext_oamTransform16(2, value >> 16, 1) << 16);
    }
    *(vu32*)(0x07000000 | (address & 0x3FC)) = value;
}

EXT_ITCM u32 ext_load8Oam(u32 address)
{
    u32 aligned = address & 0x3FE;
    u32 value = ext_oamTransform16(aligned & 6, *(vu16*)(0x07000000 | aligned), -1);
    return (address & 1) ? (value >> 8) : (value & 0xFF);
}

EXT_ITCM u32 ext_load16Oam(u32 address)
{
    u32 aligned = address & 0x3FE;
    u32 value = ext_oamTransform16(aligned & 6, *(vu16*)(0x07000000 | aligned), -1);
    // unaligned loads rotate
    if (address & 1)
        value = ((value >> 8) | (value << 24)) & 0xFFFFFFFF;
    return value;
}

EXT_ITCM u32 ext_load32Oam(u32 address)
{
    u32 aligned = address & 0x3FC;
    u32 value = *(vu32*)(0x07000000 | aligned);
    if (!(aligned & 4))
    {
        value = ext_oamTransform16(0, value & 0xFFFF, -1) | (ext_oamTransform16(2, value >> 16, -1) << 16);
    }
    // unaligned loads rotate
    u32 rotate = (address & 3) * 8;
    if (rotate)
        value = (value >> rotate) | (value << (32 - rotate));
    return value;
}

// ------------------------------------------------------------------------------------------------
// Setup
// ------------------------------------------------------------------------------------------------

static void ext_setIoStore16(u32 firstReg, u32 lastReg, memu_load_store_handler_t handler)
{
    for (u32 reg = firstReg; reg <= lastReg; reg += 2)
    {
        memu_store16IoTable[reg >> 1] = (u16)(u32)handler;
    }
}

static void ext_setIoStore32(u32 firstReg, u32 lastReg, memu_load_store_handler_t handler)
{
    for (u32 reg = firstReg; reg <= lastReg; reg += 4)
    {
        memu_store32IoTable[reg >> 2] = (u16)(u32)handler;
    }
}

void ext_enable(u32 offsetX, u32 offsetY)
{
    sExt.offsetX = offsetX > EXT_MAX_OFFSET_X ? EXT_MAX_OFFSET_X : offsetX;
    sExt.offsetY = offsetY > EXT_MAX_OFFSET_Y ? EXT_MAX_OFFSET_Y : offsetY;
    // The affine registers are write only, so start from the identity matrix, which is what the gba bios
    // sets up too, and a reference point of zero. If the software never writes them, they must still be shifted.
    for (u32 bg = 0; bg < 2; bg++)
    {
        sExt.matrix[bg][0] = 0x100;
        sExt.matrix[bg][1] = 0;
        sExt.matrix[bg][2] = 0;
        sExt.matrix[bg][3] = 0x100;
        sExt.ref[bg][0] = 0;
        sExt.ref[bg][1] = 0;
        for (u32 i = 0; i < 4; i++)
            *(vu16*)(0x04000020 + bg * 0x10 + i * 2) = sExt.matrix[bg][i];
        ext_writeRef(bg, 0);
        ext_writeRef(bg, 1);
    }

    // everything the software does not write must be shifted as well; the registers start at 0
    for (u32 reg = 0x10; reg <= 0x1E; reg += 2)
    {
        *(vu16*)(0x04000000 + reg) = ext_adjustScroll(0, (reg & 2) ? sExt.offsetY : sExt.offsetX);
    }
    for (u32 reg = 0x40; reg <= 0x46; reg += 2)
    {
        *(vu16*)(0x04000000 + reg) = ext_adjustWindow(reg, 0);
    }

    // io registers
    ext_setIoStore16(GBA_REG_OFFS_BG0HOFS, GBA_REG_OFFS_BG3VOFS, ext_store16ScrollHandler);
    ext_setIoStore32(GBA_REG_OFFS_BG0HOFS, GBA_REG_OFFS_BG3HOFS, ext_store32ScrollHandler);

    ext_setIoStore16(GBA_REG_OFFS_BG2PA, GBA_REG_OFFS_BG2PD, ext_store16AffineMatrixHandler);
    ext_setIoStore16(GBA_REG_OFFS_BG3PA, GBA_REG_OFFS_BG3PD, ext_store16AffineMatrixHandler);
    ext_setIoStore32(GBA_REG_OFFS_BG2PA, GBA_REG_OFFS_BG2PC, ext_store32AffineMatrixHandler);
    ext_setIoStore32(GBA_REG_OFFS_BG3PA, GBA_REG_OFFS_BG3PC, ext_store32AffineMatrixHandler);

    ext_setIoStore16(GBA_REG_OFFS_BG2X_L, GBA_REG_OFFS_BG2Y_H, ext_store16AffineRefHandler);
    ext_setIoStore16(GBA_REG_OFFS_BG3X_L, GBA_REG_OFFS_BG3Y_H, ext_store16AffineRefHandler);
    ext_setIoStore32(GBA_REG_OFFS_BG2X, GBA_REG_OFFS_BG2Y, ext_store32AffineRefHandler);
    ext_setIoStore32(GBA_REG_OFFS_BG3X, GBA_REG_OFFS_BG3Y, ext_store32AffineRefHandler);

    ext_setIoStore16(GBA_REG_OFFS_WIN0H, GBA_REG_OFFS_WIN1V, ext_store16WindowHandler);
    ext_setIoStore32(GBA_REG_OFFS_WIN0H, GBA_REG_OFFS_WIN0V, ext_store32WindowHandler);

    // oam
    gExtendedViewOamActive = 1;
    memu_setLoad8Handler(7, ext_load8OamHandler);
    memu_setLoad16Handler(7, ext_load16OamHandler);
    memu_setLoad32Handler(7, ext_load32OamHandler);
    memu_setStore16Handler(7, ext_store16OamHandler);
    memu_setStore32Handler(7, ext_store32OamHandler);
}
