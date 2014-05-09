/* Target-dependent header for the RISC-V architecture, for GDB, the GNU Debugger.

   Copyright (C) 2002-2014 Free Software Foundation, Inc.
   Copyright (C) 2014 Todd Snyder

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef RISCV_TDEP_H
#define RISCV_TDEP_H

struct gdbarch;

/* All the possible RISC-V ABIs. */
#define RISCV_ABI_FLAG_RV32I          (0x00000000) // 32-bit Integer GPRs
#define RISCV_ABI_FLAG_RV64I          (0x40000000) // 64-bit Integer GPRs
#define RISCV_ABI_FLAG_RV128I         (0x80000000) // 128-bit Integer GPRs
#define RISCV_ABI_FLAG_Xswfp          (0x01000000) // Software Floating Point Emulation
#define RISCV_ABI_FLAG_M              (0x00000001) // Integer Multiply and Division
#define RISCV_ABI_FLAG_A              (0x00000002) // Atomics
#define RISCV_ABI_FLAG_F              (0x00000004) // Single-Precision Floating-Point
#define RISCV_ABI_FLAG_D              (0x00000008) // Double-Precision Floating-Point
#define RISCV_ABI_FLAG_Q              (0x00000010) // Quad-Precision Floating-Point
#define RISCV_ABI_FLAG_L              (0x00000020) // Decimal Floating-Point
#define RISCV_ABI_FLAG_C              (0x00000040) // 16-bit Compressed Instructions
#define RISCV_ABI_FLAG_B              (0x00000080) // Bit Manipulation
#define RISCV_ABI_FLAG_T              (0x00000100) // Transactional Memory
#define RISCV_ABI_FLAG_P              (0x00000200) // Packed-SIMD Extensions

// Shortcuts for common configurations
#define RISCV_ABI_RV32G               (RISCV_ABI_FLAG_RV32I | RISCV_ABI_FLAG_M | RISCV_ABI_FLAG_A | RISCV_ABI_FLAG_F | RISCV_ABI_FLAG_D)
#define RISCV_ABI_RV64G               (RISCV_ABI_FLAG_RV64I | RISCV_ABI_FLAG_M | RISCV_ABI_FLAG_A | RISCV_ABI_FLAG_F | RISCV_ABI_FLAG_D)
#define RISCV_ABI_RV32G_Xswfp         (RISCV_ABI_FLAG_RV32I | RISCV_ABI_FLAG_M | RISCV_ABI_FLAG_A | RISCV_ABI_FLAG_F | RISCV_ABI_FLAG_D | RISCV_ABI_FLAG_Xswfp)
#define RISCV_ABI_RV64G_Xswfp         (RISCV_ABI_FLAG_RV64I | RISCV_ABI_FLAG_M | RISCV_ABI_FLAG_A | RISCV_ABI_FLAG_F | RISCV_ABI_FLAG_D | RISCV_ABI_FLAG_Xswfp)

#define IS_RV32I(x)                   (((x & 0xF0000000)>>28) == (RISCV_ABI_FLAG_RV32I>>28))
#define IS_RV64I(x)                   (((x & 0xF0000000)>>28) == (RISCV_ABI_FLAG_RV64I>>28))
#define IS_RV128I(x)                  (((x & 0xF0000000)>>28) == (RISCV_ABI_FLAG_RV128I>>28))

#define HAS_FPU(x)                    (((x & RISCV_ABI_FLAG_F) != 0) || ((x & RISCV_ABI_FLAG_D) != 0))

#define RISCV_INSTLEN                 (4)
#define RISCV_SBREAK_INSTR_STRUCT     { 0x00, 0x10, 0x00, 0x73 }


enum {
  RISCV_ZERO_REGNUM           = 0,    /* Read-only register, always 0. */
  RISCV_RA_REGNUM             = 1,    /* Return address */
  RISCV_S0_REGNUM             = 2,    /* Saved Register / Frame Pointer */
  RISCV_SP_REGNUM             = 14,   /* Stack Pointer */
  RISCV_TP_REGNUM             = 15,   /* Thread Pointer */
  RISCV_V0_REGNUM             = 16,   /* Return Value */
  RISCV_V1_REGNUM             = 17,   /* Return Value */
  RISCV_GP_REGNUM             = 31,   /* Global Pointer */
  RISCV_PC_REGNUM             = 32,   /* Program Counter */
  RISCV_FIRST_FP_REGNUM       = 33,   /* First Floating Point Register */
  RISCV_FV0_REGNUM            = 49,
  RISCV_FV1_REGNUM            = 50,
  RISCV_LAST_FP_REGNUM        = 64,   /* Last Floating Point Register */
  RISCV_FCSR_REGNUM           = 65,
  RISCV_FFLAGS_REGNUM,
  RISCV_FRM_REGNUM,
  RISCV_SUP0_REGNUM,
  RISCV_SUP1_REGNUM,
  RISCV_EPC_REGNUM,
  RISCV_BADVADDR_REGNUM,
  RISCV_PTBR_REGNUM,
  RISCV_ASID_REGNUM,
  RISCV_COUNT_REGNUM,
  RISCV_COMPARE_REGNUM,
  RISCV_EVEC_REGNUM,
  RISCV_CAUSE_REGNUM,
  RISCV_STATUS_REGNUM,
  RISCV_HARTID_REGNUM,
  RISCV_IMPL_REGNUM,
  RISCV_FATC_REGNUM,
  RISCV_SEND_IPI_REGNUM,
  RISCV_CLEAR_IPI_REGNUM,
  RISCV_STATS_REGNUM,
  RISCV_RESET_REGNUM,
  RISCV_TOHOST_REGNUM,
  RISCV_FROMHOST_REGNUM,
  RISCV_CYCLE_REGNUM,
  RISCV_TIME_REGNUM,
  RISCV_INSTRET_REGNUM,
  RISCV_LAST_REGNUM    // Leave this as the last enum
};

#define RISCV_NUM_REGS                (RISCV_LAST_REGNUM+1)

/* Return the RISC-V ABI associated with GDBARCH. */
int riscv_abi (struct gdbarch *gdbarch);

/* Return the RISC-V ISA's register size. */
extern int riscv_isa_regsize (struct gdbarch *gdbarch);
extern unsigned int riscv_abi_regsize (struct gdbarch *gdbarch);

/* RISC-V specific per-architecture information */
struct gdbarch_tdep
{
  int             riscv_abi;
  int             bytes_per_word;

  int             register_size_valid;
  int             register_size;

  // Return the expected next PC if FRAME is stopped at a SCALL instruction
  CORE_ADDR (*scall_next_pc) (struct frame_info *frame);
};



#endif /* RISCV_TDEP_H */
