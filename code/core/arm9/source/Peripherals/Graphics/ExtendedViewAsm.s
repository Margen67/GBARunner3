.section ".itcm", "ax"

#include "AsmMacros.inc"

// Memory handler entry points for the extended view. They have the same register interface as the other
// load/store handlers and forward to the C implementations in ExtendedView.c.

/// @brief Forwards a store to a C function: void cfunc(u32 address, u32 value).
/// @param r0-r7 Preserved.
/// @param r8 The address to store to. This register is preserved.
/// @param r9 The value to store. Trashed.
/// @param r10-r12 Trashed.
/// @param r13 Preserved.
/// @param lr Return address.
.macro ext_storeHandler name, cfunc
arm_func \name
    push {r0-r3,lr}
    mov r0, r8
    mov r1, r9
    bl \cfunc
    pop {r0-r3,pc}
.endm

/// @brief Forwards a load to a C function: u32 cfunc(u32 address).
/// @param r0-r7 Preserved.
/// @param r8 The address to load from. This register is preserved.
/// @param r9 Receives the loaded value.
/// @param r10-r12 Trashed.
/// @param r13 Preserved.
/// @param lr Return address.
.macro ext_loadHandler name, cfunc
arm_func \name
    push {r0-r3,lr}
    mov r0, r8
    bl \cfunc
    mov r9, r0
    pop {r0-r3,pc}
.endm

ext_storeHandler ext_store16ScrollHandler, ext_store16Scroll
ext_storeHandler ext_store32ScrollHandler, ext_store32Scroll
ext_storeHandler ext_store16AffineMatrixHandler, ext_store16AffineMatrix
ext_storeHandler ext_store32AffineMatrixHandler, ext_store32AffineMatrix
ext_storeHandler ext_store16AffineRefHandler, ext_store16AffineRef
ext_storeHandler ext_store32AffineRefHandler, ext_store32AffineRef
ext_storeHandler ext_store16WindowHandler, ext_store16Window
ext_storeHandler ext_store32WindowHandler, ext_store32Window
ext_storeHandler ext_store16OamHandler, ext_store16Oam
ext_storeHandler ext_store32OamHandler, ext_store32Oam

ext_loadHandler ext_load8OamHandler, ext_load8Oam
ext_loadHandler ext_load16OamHandler, ext_load16Oam
ext_loadHandler ext_load32OamHandler, ext_load32Oam

.pool
.end
