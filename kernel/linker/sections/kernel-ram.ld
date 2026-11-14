/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    .kernel_data : ALIGN(32) {
        __data_start = .; /* This is used by the startup in order to initialize the .data section */

        *(.data)
        *(.data.*)

        . = ALIGN(8);
        __data_end = .;
    } >RAM AT>FLASH
    __data_load_start = LOADADDR(.kernel_data);

#if CONFIG_CCM_SIZE
    /* Zeroed data that is never a DMA target, moved out of the scarce SRAM.
       Must come before the generic .bss rules below, which would take it. */
    .ccm_stacks (NOLOAD) : ALIGN(CONFIG_KERNEL_STACK_ALIGN) {
        /* Kernel thread stacks (PBL_THREAD_STACK_DEFINE), not zeroed. */
        *(.ccm_stacks)
    } >CCM

    .ccm_bss (NOLOAD) : ALIGN(8) {
        __ccm_bss_start = .;

#ifdef CONFIG_NIMBLE_POOLS_SRAM
        /* The buffer pools stay in SRAM. */
        EXCLUDE_FILE(*transport/src/transport.c.obj
                     *porting/nimble/src/os_msys.c.obj) *nimble.dir*(.bss .bss.* COMMON)
#else
        *nimble.dir*(.bss .bss.* COMMON)
#endif
        /* The app mic stream's state and encoded frame. */
        *services__mic_stream.dir*(.bss .bss.*)
#ifdef CONFIG_AUDIO_STM32_I2S_PDM_TABLES_CCM
        /* The PDM filter's tables: used by the CPU only, the DMA ring is elsewhere. */
        *OpenPDMFilter.c.obj(.bss .bss.*)
#endif
#ifdef CONFIG_BOARD_SPALDING
        /* Kernel-only state, moved out of SRAM to fit spalding. */
        *do_not_disturb.c.obj(.bss .bss.*)
        *kernel_ui.c.obj(.bss .bss.*)
#endif

        . = ALIGN(8);
        __ccm_bss_end = .;
    } >CCM
#endif

    .kernel_ro_bss (NOLOAD) : {
        __bss_start = .; /* This is used by the startup in order to initialize the .bss section */

        . = ALIGN(__unpriv_ro_bss_size__);
        __unpriv_ro_bss_start__ = .;

        *:lib_a-timelocal.o(.bss.*)
        *:lib_a-tzvars.o(.bss.*)

        *(.kernel_unpriv_ro_bss)

        . = __unpriv_ro_bss_start__ + __unpriv_ro_bss_size__;
    } >RAM

    .kernel_bss (NOLOAD) : {
        *(.freertos_privileged_data)

        *(.bss)
        *(.bss.*)

        *(COMMON)

        __bss_end = .;
    } >RAM

    .stack (NOLOAD) : {
        . = ALIGN(CONFIG_KERNEL_STACK_ALIGN);
        __isr_stack_start__ = .;

        . = . + CONFIG_KERNEL_STACK_GUARD_SIZE + CONFIG_KERNEL_ISR_STACK_SIZE;

        _estack = .;
    } >REGION_ISR_STACK

    /* The heap gets whatever is left. Keep the section empty so the memory
       usage report shows static usage only. */
    .kernel_heap (NOLOAD) : {
        _heap_start = .;
    } >REGION_KERNEL_HEAP
    _heap_end = ORIGIN(REGION_KERNEL_HEAP) + LENGTH(REGION_KERNEL_HEAP);
