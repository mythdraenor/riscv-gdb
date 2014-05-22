/* Target-dependent code for the RISC-V architecture, for GDB, the GNU Debugger.

   Copyright (C) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997,
   1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,
   2010 Free Software Foundation, Inc.

   Contributed by Alessandro Forin(af@cs.cmu.edu) at CMU
   and by Per Bothner(bothner@cs.wisc.edu) at U.Wisconsin.

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

#include "defs.h"
#include "gdb_string.h"
#include "gdb_assert.h"
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "value.h"
#include "gdbcmd.h"
#include "language.h"
#include "gdbcore.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbtypes.h"
#include "target.h"
#include "arch-utils.h"
#include "regcache.h"
#include "osabi.h"
#include "riscv-tdep.h"
#include "block.h"
#include "reggroups.h"
#include "opcode/riscv.h"
#include "elf/riscv.h"
#include "elf-bfd.h"
#include "symcat.h"
#include "sim-regno.h"
#include "dis-asm.h"
#include "frame-unwind.h"
#include "frame-base.h"
#include "trad-frame.h"
#include "infcall.h"
#include "floatformat.h"
#include "remote.h"
#include "target-descriptions.h"
#include "dwarf2-frame.h"
#include "user-regs.h"
#include "valprint.h"
#include "opcode/riscv-opc.h"

static const struct objfile_data *riscv_pdr_data;
static struct cmd_list_element *setriscvcmdlist = NULL;
static struct cmd_list_element *showriscvcmdlist = NULL;

struct riscv_frame_cache
{
  CORE_ADDR base;
  struct trad_frame_saved_reg *saved_regs;
};

const static char *riscv_gdb_reg_names[RISCV_LAST_REGNUM+1] = {
  // general purpose registers
  "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",
  "x8",  "x9",  "x10", "x11", "x12", "x13", "x14", "x15",
  "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
  "x24", "x25", "x26", "x27", "x28", "x29", "x30", "x31", "pc",
  "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
  "f8",  "f9",  "f10", "f11", "f12", "f13", "f14", "f15",
  "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
  "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31", "fcsr",
  "fflags", "frm", "sup0", "sup1", "epc", "badvaddr", "ptbr",
  "asid", "count", "compare", "evec", "cause", "status", "hartid",
  "impl", "fatc", "send_ipi", "clear_ipi", "stats", "reset", "tohost", 
  "fromhost", "cycle", "time", "instret",
};

struct register_alias
{
  const char *name;
  int regnum;
};

const struct register_alias riscv_register_aliases[] = {
  { "zero", 0 },
  { "ra",   1 },
  { "fp",   2 },
  { "s1",   3 },
  { "s2",   4 },
  { "s3",   5 },
  { "s4",   6 },
  { "s5",   7 },
  { "s6",   8 },
  { "s7",   9 },
  { "s8",  10 },
  { "s9",  11 },
  { "l0",  12 },
  { "l1",  13 },
  { "sp",  14 },
  { "tp",  15 },
  { "v0",  16 },
  { "v1",  17 },
  { "a0",  18 },
  { "a1",  19 },
  { "a2",  20 },
  { "a3",  21 },
  { "a4",  22 },
  { "a5",  23 },
  { "a6",  24 },
  { "a7",  25 },
  { "t0",  26 },
  { "t1",  27 },
  { "t2",  28 },
  { "t3",  29 },
  { "t4",  30 },
  { "gp",  31 },

  { "fs0", 33 },
  { "fs1", 34 },
  { "fs2", 35 },
  { "fs3", 36 },
  { "fs4", 37 },
  { "fs5", 38 },
  { "fs6", 39 },
  { "fs7", 40 },
  { "fs8", 41 },
  { "fs9", 42 },
  { "fs10", 43 },
  { "fs11", 44 },
  { "fs12", 45 },
  { "fs13", 46 },
  { "fs14", 47 },
  { "fs15", 48 },
  { "fv0", 49 },
  { "fv1", 50 },
  { "fa0", 51 },
  { "fa1", 52 },
  { "fa2", 53 },
  { "fa3", 54 },
  { "fa4", 55 },
  { "fa5", 56 },
  { "fa6", 57 },
  { "fa7", 58 },
  { "ft0", 59 },
  { "ft1", 60 },
  { "ft2", 61 },
  { "ft3", 62 },
  { "ft4", 63 },
  { "ft5", 64 }
};

static int riscv_debug = 0;

/* Return the RISC-V ABI associated with GDBARCH. */
int
riscv_abi (struct gdbarch *gdbarch)
{
  return gdbarch_tdep(gdbarch)->riscv_abi;
}

int 
riscv_isa_regsize(struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep(gdbarch);

  // If we know how big the registers are, use that size
  if (tdep->register_size_valid) {
    return tdep->register_size;
  }

  // fall back to the previous behavior
  return (gdbarch_bfd_arch_info(gdbarch)->bits_per_word / gdbarch_bfd_arch_info (gdbarch)->bits_per_byte);
}

unsigned int
riscv_abi_regsize(struct gdbarch *gdbarch)
{
  int abi = riscv_abi(gdbarch);
  if (IS_RV32I(abi)) {
    return 4;
  } else if (IS_RV64I(abi)) {
    return 8;
  } else if (IS_RV128I(abi)) {
    return 16;
  } else {
    internal_error (__FILE__, __LINE__, _("bad switch"));
  }
}

// static void
// riscv_find_abi_section(bfd *abfd, asection *sect, void *obj)
// {
//   const char *name = bfd_get_section_name (abfd, sect);

//   fprintf_unfiltered(gdb_stdlog, "riscv_find_abi_section: %s", name);
// }

static void
riscv_xfer_register (struct gdbarch *gdbarch, struct regcache *regcache,
		     int reg_num, int length,
		     enum bfd_endian endian, gdb_byte *in,
		     const gdb_byte *out, int buf_offset)
{
  int reg_offset = 0;

  gdb_assert (reg_num >= gdbarch_num_regs (gdbarch));
  
  // need to transfer the left or right part of the register, based on the targets byte order
  switch (endian) {
    case BFD_ENDIAN_BIG:
      reg_offset = register_size (gdbarch, reg_num) - length;
      break;
    case BFD_ENDIAN_LITTLE:
      reg_offset = 0;
      break;
    case BFD_ENDIAN_UNKNOWN: // indicates no alignment
      reg_offset = 0;
      break;
    default:
      internal_error (__FILE__, __LINE__, _("bad switch"));
  }
  
  if (riscv_debug) {
    fprintf_unfiltered(gdb_stderr,
		       "xfer $%d, reg offset %d, buf offset %d, length %d, ",
		       reg_num, reg_offset, buf_offset, length);
  }
  
  if (riscv_debug && out != NULL) {
    int i;
    fprintf_unfiltered(gdb_stdlog, "out ");
    for (i = 0; i < length; i++) {
      fprintf_unfiltered (gdb_stdlog, "%02x", out[buf_offset + i]);
    }
  }

  if (in != NULL)
    regcache_cooked_read_part (regcache, reg_num, reg_offset, length, in + buf_offset);
  if (out != NULL)
    regcache_cooked_write_part (regcache, reg_num, reg_offset, length, out + buf_offset);

  if (riscv_debug && in != NULL) {
    int i;
    fprintf_unfiltered (gdb_stdlog, "in ");
    for(i = 0; i < length; i++) {
      fprintf_unfiltered (gdb_stdlog, "%02x", in[buf_offset + i]);
    }
  }

  if (riscv_debug) {
    fprintf_unfiltered (gdb_stdlog, "\n");
  }
}

static CORE_ADDR
riscv_read_pc (struct regcache *regcache)
{
  ULONGEST pc;
  int regnum = RISCV_PC_REGNUM;
  regcache_cooked_read_signed (regcache, regnum, &pc);
  return pc;
}

//static CORE_ADDR
//riscv_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
//{
  //  return frame_unwind_register_signed (next_frame, gdb
//}

static void
riscv_write_pc (struct regcache *regcache, CORE_ADDR pc)
{
  int regnum = RISCV_PC_REGNUM;
  regcache_cooked_write_unsigned (regcache, regnum, pc);
}

// assume instructions are 32-bits in length
static ULONGEST
riscv_fetch_instruction (struct gdbarch *gdbarch, CORE_ADDR addr)
{
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
  gdb_byte buf[4];
  int instlen = 4;
  int status;
  
  status = target_read_memory (addr, buf, instlen);
  if (status) {
    memory_error (status, addr);
  }
  return extract_unsigned_integer (buf, instlen, byte_order);
}


static const gdb_byte *
riscv_breakpoint_from_pc (struct gdbarch *gdbarch,
			  CORE_ADDR      *bp_addr,
			  int            *bp_size)
{
  static const gdb_byte breakpoint[] = RISCV_SBREAK_INSTR_STRUCT;

  *bp_size = RISCV_INSTLEN;
  return breakpoint;
}

static void
riscv_remote_breakpoint_from_pc (struct gdbarch *gdbarch, CORE_ADDR *pcptr, int *kindptr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  riscv_breakpoint_from_pc (gdbarch, pcptr, kindptr);
}

static struct value *
value_of_riscv_user_reg (struct frame_info *frame, const void *baton)
{
  const int *reg_p = baton;
  return value_of_register (*reg_p, frame);
}

static const char *
riscv_register_name (struct gdbarch *gdbarch,
		     int             regnum)
{
  int i;

  if (tdesc_has_registers (gdbarch_target_desc (gdbarch)))
    return tdesc_register_name(gdbarch, regnum);
  else {
    if (0 <= regnum && regnum < RISCV_LAST_REGNUM) {
      for(i = 0; i < (sizeof(riscv_register_aliases)/sizeof(riscv_register_aliases[0])); i++) {
	if (regnum == riscv_register_aliases[i].regnum) {
	  return riscv_register_aliases[i].name;
	}
      }
      return riscv_gdb_reg_names[regnum];
    } else {
      return NULL;
    }
  }
}

static enum return_value_convention
riscv_return_value (struct gdbarch  *gdbarch,
		    struct type     *functype,
		    struct type     *type,
		    struct regcache *regcache,
		    gdb_byte        *readbuf,
		    const gdb_byte  *writebuf)
{
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
  enum type_code  rv_type    = TYPE_CODE (type);
  unsigned int    rv_size    = TYPE_LENGTH (type);
  unsigned int    rv_fields  = TYPE_NFIELDS (type);
  unsigned int    bpw        = (gdbarch_tdep (gdbarch))->bytes_per_word;
  int             fp         = 0;
  int             regnum     = 0;
  int             offset, xfer;

  // Paragraph on return values taken from 1.9999 version of RISC-V specification
  // "Values are returned from functions in integer registers v0 and v1 and floating-point
  // registers fv0 and fv1.  Floating point values are returned in floating-point registers
  // only if they are primitives or members of a struct consisting of only one or two floating
  // point values.  Other return values that fit into two pointer-words are returned in v0 and v1.
  // Larger return values are passed entirely in memory; the caller allocates this memory region 
  // and passes a pointer to it as an implicit first parameter to the callee."

  // deal with struct/unions first that are too large to fit into 2 registers.
  if (rv_size > 2 * riscv_isa_regsize (gdbarch)) {
    if (readbuf) {
      ULONGEST tmp;
      regcache_cooked_read_unsigned (regcache, RISCV_V0_REGNUM, &tmp);
      read_memory (tmp, readbuf, rv_size);
    }
    if (writebuf) {
      ULONGEST tmp;
      regcache_cooked_read_unsigned (regcache, RISCV_V0_REGNUM, &tmp);
      write_memory (tmp, writebuf, rv_size);
    }
    return RETURN_VALUE_ABI_RETURNS_ADDRESS;
  }

  // Are we dealing with a floating point value?
  if (TYPE_CODE_FLT == rv_type)
    fp = 1;
  if (((TYPE_CODE_STRUCT == rv_type) || (TYPE_CODE_UNION == rv_type)) && (rv_fields == 1)) {
    struct type *fieldtype = TYPE_FIELD_TYPE (type, 0);
    if (TYPE_CODE_FLT == TYPE_CODE (check_typedef (fieldtype)))
      fp = 1;
  }
  if (((TYPE_CODE_STRUCT == rv_type) || (TYPE_CODE_UNION == rv_type)) && (rv_fields == 2)) {
    struct type *fieldtype0 = TYPE_FIELD_TYPE (type, 0);
    struct type *fieldtype1 = TYPE_FIELD_TYPE (type, 1);
    if ((TYPE_CODE_FLT == TYPE_CODE (check_typedef (fieldtype0))) && (TYPE_CODE_FLT == TYPE_CODE (check_typedef (fieldtype1))))
      fp = 1;
  }

  if (fp) {
    if (riscv_debug)
      fprintf_unfiltered (gdb_stderr, "Return float in $fv0\n");
    regnum = RISCV_FV0_REGNUM;
  } else {
    if (riscv_debug)
      fprintf_unfiltered (gdb_stderr, "Return scalar in $v0\n");
    regnum = RISCV_V0_REGNUM;
  }

  for (offset = 0;
       offset < rv_size;
       offset += riscv_abi_regsize (gdbarch), regnum++)
    {
      xfer = riscv_abi_regsize (gdbarch);
      if (offset + xfer > rv_size)
	xfer = rv_size - offset;
      riscv_xfer_register (gdbarch, regcache,
			   gdbarch_num_regs (gdbarch) + regnum, xfer,
			   gdbarch_byte_order (gdbarch), readbuf, writebuf,
			   offset);
    }
  return RETURN_VALUE_REGISTER_CONVENTION;
}

static void
riscv_pseudo_register_read (struct gdbarch  *gdbarch,
			    struct regcache *regcache,
			    int              regnum,
			    gdb_byte        *buf)
{
  regcache_raw_read (regcache, regnum, buf);
}

static void
riscv_pseudo_register_write (struct gdbarch  *gdbarch,
			     struct regcache *regcache,
			     int              cookednum,
			     const gdb_byte  *buf)
{
  regcache_raw_write (regcache, cookednum, buf);
}

static struct type *
riscv_register_type (struct gdbarch  *gdbarch,
		     int              regnum)
{
  if (regnum < RISCV_FIRST_FP_REGNUM) {
    if (riscv_isa_regsize (gdbarch) == 4) {
      return builtin_type (gdbarch)->builtin_int32;
    } else {
      return builtin_type (gdbarch)->builtin_int64;
    }
  } else if (regnum < RISCV_FCSR_REGNUM) {
    if (riscv_isa_regsize (gdbarch) == 4) {
      return builtin_type (gdbarch)->builtin_float;
    } else {
      return builtin_type (gdbarch)->builtin_double;
    }
  } else {
    if (riscv_isa_regsize (gdbarch) == 4) {
      return builtin_type (gdbarch)->builtin_int32;
    } else {
      return builtin_type (gdbarch)->builtin_int64;
    }
  }
}

static void
riscv_read_fp_register_single (struct frame_info *frame, int regno,
			       gdb_byte *rare_buffer)
{
  struct gdbarch *gdbarch = get_frame_arch (frame);
  int raw_size = register_size (gdbarch, regno);
  gdb_byte *raw_buffer = alloca (raw_size);

  if (!frame_register_read (frame, regno, raw_buffer)) {
    error(_("can't read register %d (%s)"), regno, gdbarch_register_name (gdbarch, regno));
  }

  if (raw_size == 8) {
    int offset;
    
    if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
      offset = 4;
    else
      offset = 0;

    memcpy(rare_buffer, raw_buffer + offset, 4);
  } else {
    memcpy(rare_buffer, raw_buffer, 4);
  }
}

static void
riscv_read_fp_register_double (struct frame_info *frame, int regno,
			       gdb_byte *rare_buffer)
{
  struct gdbarch *gdbarch = get_frame_arch (frame);
  int raw_size = register_size (gdbarch, regno);

  if (raw_size == 8) {
    if (!frame_register_read (frame, regno, rare_buffer)) {
      error(_("can't read register %d (%s)"), regno, gdbarch_register_name (gdbarch, regno));
    }
  } else {
    internal_error(__FILE__, __LINE__, _("riscv_read_fp_register_double: size says 32-bits, read is 64-bits."));
  }
}

static void
riscv_print_fp_register (struct ui_file *file, struct frame_info *frame,
			 int regnum)
{
  struct gdbarch *gdbarch = get_frame_arch (frame);
  gdb_byte *raw_buffer;
  double doub, flt1;
  int inv1, inv2;

  raw_buffer = alloca (2 * register_size (gdbarch, RISCV_FIRST_FP_REGNUM));

  fprintf_filtered (file, "%s:", gdbarch_register_name (gdbarch, regnum));
  fprintf_filtered (file, "%*s", 4 - (int)strlen(gdbarch_register_name (gdbarch, regnum)), "");

  if (register_size (gdbarch, regnum) == 4) {
    struct value_print_options opts;

    riscv_read_fp_register_single (frame, regnum, raw_buffer);
    flt1 = unpack_double (builtin_type (gdbarch)->builtin_float, raw_buffer, &inv1);

    get_formatted_print_options (&opts, 'x');
    print_scalar_formatted (raw_buffer, 
			    builtin_type (gdbarch)->builtin_uint32,
			    &opts, 'w', file);

    fprintf_filtered (file, " value: ");
    if (inv1)
      fprintf_filtered (file, " <invalid float> ");
    else
      fprintf_filtered (file, "%-17.9g", flt1);
  } 
  else {
    struct value_print_options opts;

    riscv_read_fp_register_double (frame, regnum, raw_buffer);
    doub = unpack_double (builtin_type (gdbarch)->builtin_double, raw_buffer, &inv2);
    
    get_formatted_print_options (&opts, 'x');
    print_scalar_formatted (raw_buffer, builtin_type (gdbarch)->builtin_uint64, 
			    &opts, 'g', file);

    fprintf_filtered (file, " value: ");
    if (inv2)
      fprintf_filtered (file, "<invalid double>");
    else
      fprintf_filtered (file, "%-24.17g", doub);
  }
}

static void
riscv_print_register (struct ui_file *file, struct frame_info *frame,
		      int regnum)
{
  struct gdbarch *gdbarch = get_frame_arch (frame);
  gdb_byte raw_buffer[MAX_REGISTER_SIZE];
  int offset;
  struct value_print_options opts;

  if (TYPE_CODE (register_type (gdbarch, regnum)) == TYPE_CODE_FLT) {
    riscv_print_fp_register (file, frame, regnum);
    return;
  }

  if (!frame_register_read (frame, regnum, raw_buffer)) {
    fprintf_filtered (file, "%s: [Invalid]", gdbarch_register_name (gdbarch, regnum));
    return;
  }

  fputs_filtered (gdbarch_register_name (gdbarch, regnum), file);
  fprintf_filtered (file, ": ");

  if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG) {
    offset = register_size (gdbarch, regnum) - register_size (gdbarch, regnum);
  } else {
    offset = 0;
  }

  get_formatted_print_options (&opts, 'x');
  print_scalar_formatted (raw_buffer + offset,
			  register_type (gdbarch, regnum), &opts, 0, file);
}

static int
print_fp_register_row (struct ui_file *file, struct frame_info *frame,
		       int regnum)
{
  fprintf_filtered (file, " ");
  riscv_print_fp_register (file, frame, regnum);
  fprintf_filtered (file, "\n");
  return regnum+1;
}

static int
print_gp_register_row (struct ui_file *file, struct frame_info *frame,
		       int start_regnum)
{
  struct gdbarch *gdbarch = get_frame_arch (frame);
  gdb_byte raw_buffer[MAX_REGISTER_SIZE];
  int ncols = (riscv_abi_regsize (gdbarch) == 8 ? 4 : 8);
  int col, byte;
  int regnum;

  for(col = 0, regnum = start_regnum; 
      col < ncols && regnum < RISCV_LAST_REGNUM;
      regnum++) {
    if (*gdbarch_register_name (gdbarch, regnum) == '\0') {
      continue;
    }
    if (TYPE_CODE (register_type (gdbarch, regnum)) == TYPE_CODE_FLT)
      break;
    if (col == 0)
      fprintf_filtered (file, "     ");
    fprintf_filtered(file,
		     riscv_abi_regsize (gdbarch) == 8 ? "%17s" : "%9s",
		     gdbarch_register_name (gdbarch, regnum));
    col++;
  }

  if (col == 0)
    return regnum;

  if (start_regnum < RISCV_PC_REGNUM) {
    fprintf_filtered (file, "\n x%-4d", start_regnum);
  } else {
    fprintf_filtered (file, "\n      ");
  }

  for(col = 0, regnum = start_regnum;
      col < ncols && regnum < RISCV_LAST_REGNUM;
      regnum++) {
    if (*gdbarch_register_name (gdbarch, regnum) == '\0') 
      continue;
    if (TYPE_CODE (register_type (gdbarch, regnum)) == TYPE_CODE_FLT)
      break;
   
    if (!frame_register_read (frame, regnum, raw_buffer)) {
      error (_("can't read register %d (%s)"), regnum, gdbarch_register_name (gdbarch, regnum));
    }

    if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG) {
      for(byte = register_size(gdbarch, regnum) - register_size (gdbarch, regnum);
	  byte < register_size (gdbarch, regnum); byte++) {
	fprintf_filtered (file, "%02x", raw_buffer[byte]);
      }
    } else {
      for(byte = register_size (gdbarch, regnum) - 1; 
	  byte >= 0; byte--) {
	fprintf_filtered (file, "%02x", raw_buffer[byte]);
      }
      fprintf_filtered (file, " ");
      col++;
    }
  }

  if (col > 0)
    fprintf_filtered (file, "\n");
  
  return regnum;
}

static void
riscv_print_register_formatted (struct ui_file *file, struct frame_info *frame,
				int regnum)
{
  struct gdbarch *gdbarch = get_frame_arch (frame);
  gdb_byte raw_buffer[MAX_REGISTER_SIZE];
  double doub;
  long long d;
  int inv2, offset;
  struct value_print_options opts;

  // floating point
  if (TYPE_CODE (register_type (gdbarch, regnum)) == TYPE_CODE_FLT) {
    riscv_read_fp_register_double (frame, regnum, raw_buffer);
    doub = unpack_double (builtin_type (gdbarch)->builtin_double, raw_buffer, &inv2);
    
    fprintf_filtered (file, "%-10s     ", riscv_register_name(gdbarch, regnum));
    get_formatted_print_options (&opts, 'x');
    print_scalar_formatted (raw_buffer, builtin_type (gdbarch)->builtin_uint64, &opts, 'g', file);
    if (inv2)
      fprintf_filtered (file, " <invalid double>\n");
    else
      fprintf_filtered (file, " %-24.17g\n", doub);
  } 
  else {
    if (!frame_register_read (frame, regnum, raw_buffer)) {
      fprintf_filtered (file, "%-10s     [Invalid]", riscv_register_name(gdbarch, regnum));
      return;
    }

    fprintf_filtered (file, "%-10s     ", riscv_register_name(gdbarch, regnum));
    if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG) {
      offset = register_size (gdbarch, regnum) - register_size (gdbarch, regnum);
    } else {
      offset = 0;
    }
    
    get_formatted_print_options (&opts, 'x');
    print_scalar_formatted (raw_buffer + offset,
			    register_type (gdbarch, regnum), &opts, 0, file);
    fprintf_filtered (file, "\t");
    get_formatted_print_options (&opts, 'd');
    print_scalar_formatted (raw_buffer + offset,
			    register_type (gdbarch, regnum), &opts, 0, file);
    fprintf_filtered (file, "\n");
  }
}

static void
riscv_print_registers_info (struct gdbarch    *gdbarch,
			    struct ui_file    *file,
			    struct frame_info *frame,
			    int                regnum,
			    int                all)
{
  if (regnum != -1) { // print one specified register
    gdb_assert (regnum < RISCV_LAST_REGNUM);
    if ('\0' == *(riscv_register_name (gdbarch, regnum))) {
      error(_("Not a valid register for the current processor type"));
    }
    riscv_print_register (file, frame, regnum);
    fprintf_filtered (file, "\n");
  }
  else {
    regnum = 0;
    while(regnum < RISCV_LAST_REGNUM) {
      if ('\0' == *(riscv_register_name (gdbarch, regnum))) {
	error(_("Not a valid register for the current processor type"));
      }

      if (TYPE_CODE (register_type (gdbarch, regnum)) == TYPE_CODE_FLT) {
	if (all) riscv_print_register_formatted (file, frame, regnum);
      } else {
	riscv_print_register_formatted (file, frame, regnum);
      }
      regnum++;
    }
  }
}

static int
riscv_register_reggroup_p (struct gdbarch  *gdbarch,
			   int              regnum,
			   struct reggroup *reggroup)
{
  int float_p;
  int raw_p;

  float_p = TYPE_CODE (register_type (gdbarch, regnum)) == TYPE_CODE_FLT;
  if (gdbarch_register_name (gdbarch, regnum) == NULL || gdbarch_register_name (gdbarch, regnum)[0] == '\0')
    return 0;

  if (reggroup == float_reggroup)
    return float_p;
  if (reggroup == general_reggroup)
    return !float_p;

  return 0;
}

static void
set_reg_offset (struct gdbarch *gdbarch, struct riscv_frame_cache *this_cache,
		int regnum, CORE_ADDR offset)
{
  if (this_cache != NULL && this_cache->saved_regs[regnum].addr == -1) {
    this_cache->saved_regs[regnum].addr = offset;
  }
}

static void
reset_saved_regs (struct gdbarch *gdbarch, struct riscv_frame_cache *this_cache)
{
  if (this_cache == NULL || this_cache->saved_regs == NULL)
    return;

  {
    const int num_regs = gdbarch_num_regs (gdbarch);
    int i;

    for (i = 0; i < num_regs; i++) {
      this_cache->saved_regs[i].addr = -1;
    }
  }
}

static int
gdb_print_insn_riscv (bfd_vma memaddr, struct disassemble_info *info)
{
  if (!info->disassembler_options)
    info->disassembler_options = "gpr-names=32";

  return print_insn_little_riscv (memaddr, info);
}

static CORE_ADDR
riscv_scan_prologue (struct gdbarch *gdbarch,
		     CORE_ADDR start_pc, CORE_ADDR limit_pc,
		     struct frame_info *this_frame,
		     struct riscv_frame_cache *this_cache)
{
  CORE_ADDR cur_pc;
  CORE_ADDR frame_addr = 0; 
  CORE_ADDR sp;
  long frame_offset;
  int frame_reg = RISCV_SP_REGNUM;

  CORE_ADDR end_prologue_addr = 0;
  int seen_sp_adjust = 0;
  int load_immediate_bytes = 0;
  int regsize_is_64_bits = (riscv_abi_regsize (gdbarch) == 8);
  
  /* Can be called when there's no process, and hence when there's no THIS_FRAME. */
  if (this_frame != NULL) {
    sp = get_frame_register_signed (this_frame, RISCV_SP_REGNUM);
  } else {
    sp = 0;
  }

  if (limit_pc > start_pc + 200)
    limit_pc = start_pc + 200;

 restart:

  frame_offset = 0;
  for(cur_pc = start_pc; cur_pc < limit_pc; cur_pc += RISCV_INSTLEN) {
    unsigned long inst, opcode;
    int reg, rs1, imm12, rs2, offset12, funct3;

    // fetch the instruction
    inst = (unsigned long) riscv_fetch_instruction (gdbarch, cur_pc);
    opcode = inst & 0x7F;
    reg = (inst >> 7) & 0x1F;
    rs1 = (inst >> 15) & 0x1F;
    imm12 = (inst >> 20) & 0xFFF;
    rs2 = (inst >> 20) & 0x1F;
    offset12 = (((inst >> 25) & 0x7F) << 5) + ((inst >> 7) & 0x1F);
    funct3 = (inst >> 12) & 0x7;

    // look for stack adjustments
    if ((opcode == 0x13 && reg == RISCV_SP_REGNUM && rs1 == RISCV_SP_REGNUM) ||    // addi sp,sp,-i
	(opcode == 0x1B && reg == RISCV_SP_REGNUM && rs1 == RISCV_SP_REGNUM)) {    // addiw sp,sp,-i
      if (imm12 & 0x800)
	frame_offset += 0x1000 - imm12;
      else
	break;
      seen_sp_adjust = 1;
    }

    else if (opcode == 0x23 && funct3 == 0x2 && rs1 == RISCV_SP_REGNUM) { // sw reg, offset(sp)
      set_reg_offset (gdbarch, this_cache, rs1, sp + offset12);
    }

    else if (opcode == 0x23 && funct3 == 0x3 && rs1 == RISCV_SP_REGNUM) { // sd reg, offset(sp)
      set_reg_offset (gdbarch, this_cache, rs1, sp + offset12);
    }

    else if (opcode == 0x13 && reg == RISCV_S0_REGNUM && rs1 == RISCV_SP_REGNUM) {  // addi s0, sp, size
      if ((long) imm12 != frame_offset)
	frame_addr = sp + imm12;
      else if (this_frame && frame_reg == RISCV_SP_REGNUM) {
	unsigned alloca_adjust;

	frame_reg = RISCV_S0_REGNUM;
	frame_addr = get_frame_register_signed (this_frame, RISCV_S0_REGNUM);

	alloca_adjust = (unsigned) (frame_addr - (sp - imm12));
	if (alloca_adjust > 0) {
	  sp += alloca_adjust;
	  reset_saved_regs (gdbarch, this_cache);
	  goto restart;
	}
      }
    }
    
    else if ((opcode == 0x33 && reg == RISCV_S0_REGNUM && rs1 == RISCV_SP_REGNUM && rs2 == RISCV_ZERO_REGNUM) ||
	     (opcode == 0x3B && reg == RISCV_S0_REGNUM && rs1 == RISCV_SP_REGNUM && rs2 == RISCV_ZERO_REGNUM)) { // add s0, sp, 0   addw s0, sp, 0
      if (this_frame && frame_reg == RISCV_SP_REGNUM) {
	unsigned alloca_adjust;
	frame_reg = RISCV_S0_REGNUM;
	frame_addr = get_frame_register_signed (this_frame, RISCV_S0_REGNUM);

	alloca_adjust = (unsigned) (frame_addr - sp);
	if (alloca_adjust > 0) {
	  sp = frame_addr;
	  reset_saved_regs (gdbarch, this_cache);
	  goto restart;
	}
      }
    }

    else if (opcode == 0x23 && funct3 == 0x2 && rs1 == RISCV_S0_REGNUM) { // sw reg, offset(s0)
      set_reg_offset (gdbarch, this_cache, rs1, frame_addr + offset12);
    }

    else if ((opcode == 0x17 && reg == RISCV_GP_REGNUM) ||                            // auipc gp,n
	     (opcode == 0x13 && reg == RISCV_GP_REGNUM && rs1 == RISCV_GP_REGNUM) ||  // addi gp,gp,n
	     (opcode == 0x33 && reg == RISCV_GP_REGNUM && (rs1 == RISCV_GP_REGNUM || rs2 == RISCV_GP_REGNUM)) || // add gp,gp,reg   add gp,reg,gp
	     (opcode == 0x37 && reg == RISCV_GP_REGNUM)) {                            // lui gp,n
      /* these instructions are part of the prologue, but we don't need to do
	 anything special to handle them */
    }

    //    else if (!seen_sp_adjust
    //	     && (
    //		 (opcode 
    //		 )) {
    //      load_immediate_bytes += RISCV_INSTLEN;
    //    }
    
    else {
      if (end_prologue_addr == 0)
	end_prologue_addr = cur_pc;
    }
  }

  if (this_cache != NULL) {
    this_cache->base = (get_frame_register_signed (this_frame, frame_reg) + frame_offset);
    this_cache->saved_regs[RISCV_PC_REGNUM] = this_cache->saved_regs[RISCV_RA_REGNUM];
  }

  if (end_prologue_addr == 0) {
    end_prologue_addr = cur_pc;
  }

  if (load_immediate_bytes && !seen_sp_adjust) {
    end_prologue_addr -= load_immediate_bytes;
  }

  return end_prologue_addr;
}

static CORE_ADDR
riscv_skip_prologue (struct gdbarch *gdbarch,
		     CORE_ADDR       pc)
{
  CORE_ADDR limit_pc;
  CORE_ADDR func_addr;

  /* See if we can determine the end of the prologue via the symbol table.
     If so, then return either PC, or the PC after the prologue, whichever 
     is greater. */
  if (find_pc_partial_function (pc, NULL, &func_addr, NULL)) {
    CORE_ADDR post_prologue_pc = skip_prologue_using_sal (gdbarch, func_addr);
    if (post_prologue_pc != 0) {
      return max (pc, post_prologue_pc);
    }
  }

  /* Can't determine prologue from the symbol table, need to examine instructions */
  
  /* Find an upper limit on the function prologue using the debug information.  If 
     the debug information could not be used to provide that bound, then use an arbitrary
     large number as the upper bound. */
  limit_pc = skip_prologue_using_sal (gdbarch, pc);
  if (limit_pc == 0) {
    limit_pc = pc + 100;   /* MAGIC! */
  }

  return riscv_scan_prologue (gdbarch, pc, limit_pc, NULL, NULL);
}

static CORE_ADDR
riscv_frame_align (struct gdbarch *gdbarch, CORE_ADDR addr)
{
  return align_down (addr, 16);
}

static CORE_ADDR
riscv_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_signed (next_frame, RISCV_PC_REGNUM);
}

static CORE_ADDR
riscv_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_signed (next_frame, RISCV_SP_REGNUM);
}

static struct frame_id
riscv_dummy_id (struct gdbarch *gdbarch, struct frame_info *this_frame)
{
  return frame_id_build (get_frame_register_signed (this_frame, RISCV_SP_REGNUM), get_frame_pc (this_frame));
}

//static CORE_ADDR
//riscv_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
//		       struct regcache *regcache, CORE_ADDR bp_addr,
//		       int nargs, struct value **args, CORE_ADDR sp,
//		       int struct_return, CORE_ADDR struct_addr)
//{
//}

static struct trad_frame_cache *
riscv_frame_cache (struct frame_info *this_frame, void **this_cache)
{
  CORE_ADDR pc;
  CORE_ADDR start_addr;
  CORE_ADDR stack_addr;
  struct trad_frame_cache *this_trad_cache;
  struct gdbarch *gdbarch = get_frame_arch (this_frame);
  int num_regs = gdbarch_num_regs (gdbarch);

  if ((*this_cache) != NULL)
    return (*this_cache);
  this_trad_cache = trad_frame_cache_zalloc (this_frame);
  (*this_cache) = this_trad_cache;

  trad_frame_set_reg_realreg (this_trad_cache, gdbarch_pc_regnum (gdbarch), RISCV_RA_REGNUM);

  pc = get_frame_pc (this_frame);
  find_pc_partial_function (pc, NULL, &start_addr, NULL);
  stack_addr = get_frame_register_signed (this_frame, RISCV_SP_REGNUM);
  trad_frame_set_id (this_trad_cache, frame_id_build (stack_addr, start_addr));

  trad_frame_set_this_base (this_trad_cache, stack_addr);

  return this_trad_cache;
}

static void
riscv_frame_this_id (struct frame_info *this_frame,
		     void              **prologue_cache,
		     struct frame_id   *this_id)
{
  struct trad_frame_cache *info = riscv_frame_cache (this_frame, prologue_cache);
  trad_frame_get_id (info, this_id);
}

static struct value *
riscv_frame_prev_register (struct frame_info *this_frame,
			   void              **prologue_cache,
			   int                regnum)
{
  struct trad_frame_cache *info = riscv_frame_cache (this_frame, prologue_cache);
  return trad_frame_get_register (info, this_frame, regnum);
}

static const struct frame_unwind riscv_frame_unwind = {
  .type          = NORMAL_FRAME,
  //  .stop_reason   = default_frame_unwind_stop_reason,
  .this_id       = riscv_frame_this_id,
  .prev_register = riscv_frame_prev_register,
  .unwind_data   = NULL,
  .sniffer       = default_frame_sniffer,
  .dealloc_cache = NULL,
  .prev_arch     = NULL
};


static struct gdbarch *
riscv_gdbarch_init (struct gdbarch_info  info, 
		    struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  struct gdbarch_tdep *tdep;
  const struct bfd_arch_info *binfo;
  struct tdesc_arch_data *tdesc_data = NULL;
  
  int found_abi;
  int i;

  // Find a candidate among the list of pre-declared architectures
  arches = gdbarch_list_lookup_by_info(arches, &info);
  if (arches) {
    return arches->gdbarch;
  }

  // None found, so create a new architecture from the information
  // provided.  Can't initialize all the target dependencies until
  // we actually know which target we are talking to, but put in some defaults for now.

  binfo                     = info.bfd_arch_info;
  tdep                      = xmalloc (sizeof *tdep);
  gdbarch                   = gdbarch_alloc (&info, tdep);
  tdep->riscv_abi           = 0;
  tdep->register_size_valid = 0;
  tdep->register_size       = 0;
  tdep->bytes_per_word      = binfo->bits_per_word / binfo->bits_per_byte;

  // For now, base the abi on the elf class.  elf_flags could be used here (like with mips)
  // to further specify the ABI
  //  if (elf_elfheader (info.abfd)->e_ident[EI_CLASS] == ELFCLASS32) {
  //    tdep->register_size_valid = 1;
  //    tdep->register_size = 4;
  //    tdep->riscv_abi = RISCV_ABI_RV32G;
  //  } else if (elf_elfheader (info.abfd)->e_ident[EI_CLASS] == ELFCLASS64) {
    tdep->register_size_valid = 1;
    tdep->register_size = 8;
    tdep->riscv_abi = RISCV_ABI_RV64G;
    //  }

  // target data types
  set_gdbarch_short_bit             (gdbarch, 16);
  set_gdbarch_int_bit               (gdbarch, 32); 
  if (IS_RV32I (riscv_abi (gdbarch)))
    set_gdbarch_long_bit            (gdbarch, 32);
  else
    set_gdbarch_long_bit            (gdbarch, 64);

  set_gdbarch_float_bit             (gdbarch, 32);
  set_gdbarch_double_bit            (gdbarch, 64);
  set_gdbarch_long_double_bit       (gdbarch, 128);
  if (IS_RV32I (riscv_abi (gdbarch)))
    set_gdbarch_ptr_bit             (gdbarch, 32);
  else
    set_gdbarch_ptr_bit             (gdbarch, 64);
  set_gdbarch_char_signed           (gdbarch, 1);

  // information about the target architecture
  set_gdbarch_return_value          (gdbarch, riscv_return_value);
  set_gdbarch_breakpoint_from_pc    (gdbarch, riscv_breakpoint_from_pc);
  set_gdbarch_remote_breakpoint_from_pc (gdbarch, riscv_remote_breakpoint_from_pc);
  //  set_gdbarch_have_nonsteppable_watchpoint
  //                                    (gdbarch, 1);
  set_gdbarch_print_insn            (gdbarch, gdb_print_insn_riscv);

  // register architecture
  set_gdbarch_pseudo_register_read  (gdbarch, riscv_pseudo_register_read); 
  set_gdbarch_pseudo_register_write (gdbarch, riscv_pseudo_register_write);
  set_gdbarch_num_regs              (gdbarch, RISCV_NUM_REGS);
  set_gdbarch_num_pseudo_regs       (gdbarch, RISCV_NUM_REGS);
  set_gdbarch_sp_regnum             (gdbarch, RISCV_SP_REGNUM);
  set_gdbarch_pc_regnum             (gdbarch, RISCV_PC_REGNUM);
  set_gdbarch_ps_regnum             (gdbarch, RISCV_S0_REGNUM);
  set_gdbarch_deprecated_fp_regnum  (gdbarch, RISCV_FIRST_FP_REGNUM);

  // functions to supply register information
  set_gdbarch_register_name         (gdbarch, riscv_register_name);
  set_gdbarch_register_type         (gdbarch, riscv_register_type);
  set_gdbarch_print_registers_info  (gdbarch, riscv_print_registers_info);
  set_gdbarch_register_reggroup_p   (gdbarch, riscv_register_reggroup_p);

  // functions to analyze frames
  set_gdbarch_skip_prologue         (gdbarch, riscv_skip_prologue); 
  set_gdbarch_inner_than            (gdbarch, core_addr_lessthan);
  set_gdbarch_frame_align           (gdbarch, riscv_frame_align);

  // functions to access frame data
  set_gdbarch_read_pc               (gdbarch, riscv_read_pc);
  set_gdbarch_write_pc              (gdbarch, riscv_write_pc);
  set_gdbarch_unwind_pc             (gdbarch, riscv_unwind_pc);
  set_gdbarch_unwind_sp             (gdbarch, riscv_unwind_sp);

  // functions handling dummy frames
  set_gdbarch_call_dummy_location   (gdbarch, ON_STACK); // AT_SYMBOL
  //  set_gdbarch_push_dummy_call       (gdbarch, riscv_push_dummy_call);
  set_gdbarch_dummy_id              (gdbarch, riscv_dummy_id);

  // Frame unwinders.  Use DWARF debug info if available, otherwise use our own unwinder
  dwarf2_append_unwinders           (gdbarch);
  frame_unwind_append_unwinder      (gdbarch, &riscv_frame_unwind);

  // check any target description for validity
  if (tdesc_has_registers (info.target_desc)) {
    const struct tdesc_feature *feature;
    int valid_p;

    feature = tdesc_find_feature(info.target_desc, "org.gnu.gdb.riscv.cpu");
    if (feature == NULL)
      return NULL;

    tdesc_data = tdesc_data_alloc ();

    valid_p = 1;
    for(i = RISCV_ZERO_REGNUM; i <= RISCV_LAST_REGNUM; i++) 
      valid_p &= tdesc_numbered_register (feature, tdesc_data, i, riscv_gdb_reg_names[i]);

    if (!valid_p) {
      tdesc_data_cleanup (tdesc_data);
      return NULL;
    }
  }  


  if (tdesc_data) {
    tdesc_use_registers (gdbarch, info.target_desc, tdesc_data);
  }

  for(i = 0; i < ARRAY_SIZE (riscv_register_aliases); i++)
    user_reg_add (gdbarch, riscv_register_aliases[i].name, value_of_riscv_user_reg, &riscv_register_aliases[i].regnum);

  return gdbarch;
}

static void
show_riscv_command (char *args, int from_tty)
{
  help_list (showriscvcmdlist, "show riscv ", all_commands, gdb_stdout);
}

static void
set_riscv_command (char *args, int from_tty)
{
  printf_unfiltered
    ("\"set riscv\" must be followed by an appropriate subcommand.\n");
  help_list (setriscvcmdlist, "set riscv ", all_commands, gdb_stdout);
}

static void
riscv_dump_tdep(struct gdbarch *gdbarch, struct ui_file *file)
{
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
extern initialize_file_ftype _initialize_riscv_tdep;    // -Wmissing-prototypes

void
_initialize_riscv_tdep(void)
{
  struct cmd_list_element *c;

  gdbarch_register(bfd_arch_riscv, riscv_gdbarch_init, riscv_dump_tdep);

  riscv_pdr_data = register_objfile_data ();

  add_prefix_cmd ("riscv", no_class, set_riscv_command,
		  _("Various RISCV specific commands."),
		  &setriscvcmdlist, "set riscv ", 0, &setlist);

  add_prefix_cmd ("riscv", no_class, show_riscv_command,
		  _("Various RISCV specific commands."),
		  &showriscvcmdlist, "show riscv ", 0, &showlist);

  /* Debug this file's internals. */
  add_setshow_zinteger_cmd ("riscv", class_maintenance, &riscv_debug, _("\
Set riscv debugging."), _("\
Show riscv debugging."), _("\
When non-zero, riscv specific debugging is enabled."),
			    NULL,
			    NULL,
			    &setdebuglist, &showdebuglist);
}
