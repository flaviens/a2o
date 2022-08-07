# A2O variants

import os

from migen import *

from litex import get_data_mod
from litex.soc.interconnect import wishbone
from litex.soc.interconnect.csr import *
from litex.soc.cores.cpu import CPU

dir = os.path.dirname(os.path.realpath(__file__))

# these select the top RTL file for each variant name
CPU_VARIANTS = {
    'WB_32BE' : 'a2owb',
    'WB_64LE' : 'a2owb',
    'standard' : 'a2owb'
}

# 32 is from a2p plus -ma2; can get rid of some of them
GCC_FLAGS = {
   'WB_32BE' : '-ma2 -m32 -mbig-endian fomit-frame-pointer -Wall -fno-builtin -nostdinc -fno-stack-protector -fexceptions -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes',
   'WB_64LE' : '-ma2 -m64 -mlittle-endian -mabi=elfv2 -fnostack-protector'
}

class A2O(CPU, AutoCSR):
   name = 'a2o'
   human_name = 'a2o'
   variants = CPU_VARIANTS
   family = 'ppc64'
   data_width = 64
   endianness = 'little'
   gcc_triple = ('powerpc64le-linux', 'powerpc64le-linux-gnu')
   linker_output_format = 'elf64-powerpcle'
   nop = 'nop'
   io_regions = {0xF0000000: 0x10000000} # origin, length

   @property
   def mem_map(self):
      return {
         'rom':          0x00000000,   # on-board
         'sram':         0x00004000,   # on-board
         'main_ram':     0x00100000,   # external 1M+
         'csr':          0xF0000000,
      }

   @property
   def gcc_flags(self):
      flags = GCC_FLAGS[self.variant]
      flags += ' -D__a2o__'
      return flags

   def __init__(self, platform, variant='WB'):

      if variant == 'standard':
         variant = 'WB_64LE'

      if variant == 'WB_32LE':
         self.family = 'ppc32'
         self.data_width = 32
         self.endianness = 'big'
         self.gcc_triple = 'powerpc-linux-gnu'
         self.linker_output_format = 'elf32-powerpc'

      self.platform         = platform
      self.variant          = variant
      self.human_name       = CPU_VARIANTS.get(variant, 'a2o')
      self.external_variant = None
      self.reset            = Signal()
      self.interrupt        = Signal(3)
      self.interruptS       = Signal()
      self.dbus             = dbus = wishbone.Interface()
      self.periph_buses     = [dbus]
      self.memory_buses     = []
      self.enableDebug      = False
      self.enableJTAG       = False
      self.reset_address    = 0x00000000

      self.cpu_params = dict(
         i_clk_1x            = ClockSignal('sys'),
         i_clk_2x            = ClockSignal('sys2x'),
         i_rst               = ResetSignal() | self.reset,

         # how do i connect these to csr?
         #i_cfg_wr           = csr_0[0],        # wr command - will be 3+ bit cmd
         #i_cfg_dat          = csr_1,           # wr data
         #o_status           = status,          # should update csr continuously
         i_cfg_wr            = 0,

         i_externalInterrupt  = self.interrupt[0],
         i_timerInterrupt     = self.interrupt[1],
         i_softwareInterrupt  = self.interrupt[2],
         i_externalInterruptS = self.interruptS,

         #wtf i guess you get these names from the Inteface() def - but what about other sigs?
         o_wb_cyc            = dbus.cyc,
         o_wb_stb            = dbus.stb,
         o_wb_adr            = Cat(dbus.adr,Signal(2)),
         o_wb_we             = dbus.we,
         o_wb_sel            = dbus.sel,
         o_wb_datw           = dbus.dat_w,
         i_wb_ack            = dbus.ack,
         i_wb_datr           = dbus.dat_r
      )

   def set_reset_address(self, reset_address):
      if reset_address != self.reset_address:
         print(f'Reset address = {self.reset_address} and cannot be changed here!')
         assert False

   @staticmethod
   def add_sources(platform, variant='WB_64LE'):
      dir = os.path.dirname(os.path.realpath(__file__))

      # unfortunately, vivado doesn't do the right thing and skip modules already analyzed, so overrides dirs don't work; rearrange override after
      platform.add_source(os.path.join(dir, 'verilog/a2o_litex/'))      # node, wrapper
      platform.add_source(os.path.join(dir, 'verilog/trilib/'))         # array, ff
      platform.add_source(os.path.join(dir, 'verilog/trilib_clk1x/'))   # 2r4w override
      #platform.add_source(os.path.join(dir, 'verilog/unisims/'))        # xil array
      platform.add_source(os.path.join(dir, 'verilog/work/'))           # core

   def use_external_variant(self, variant_filename):
      self.external_variant = True
      self.platform.add_source(variant_filename)

   def do_finalize(self):
      if not self.external_variant:
         self.add_sources(self.platform, self.variant)
      self.specials += Instance('a2owb', **self.cpu_params)