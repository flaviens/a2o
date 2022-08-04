// simple verilator top
// uses a2owb with sim mem interface

#define TRACING

#include <cstddef>
#include <iostream>
#include <fstream>
#include <iomanip>

#include "verilated.h"
#include "Va2owb.h"

// internal nets
#include "Va2owb___024root.h"
#include "Va2owb_a2owb.h"
#include "Va2owb_a2l2wb.h"

#ifdef TRACING
#include "verilated_vcd_c.h"
VerilatedVcdC *t;
#else
unsigned int t = 0;
#endif

/*
#include "uart/uartsim.h"
*/

Va2owb* m;

vluint64_t main_time = 0;     // in units of timeprecision used in verilog or --timescale-override
// what is it?  it changed to 941621251 after calling loadmem()

double sc_time_stamp() {      // $time in verilog
   return main_time;
}

const int resetCycle = 10;
const int threadRunCycle = resetCycle + 5;
const int runCycles = 500;
const int hbCycles = 500;
const int threads = 1;
const std::string testFile = "../mem/test1/rom.init";

// Cythonize this and use it for cocotb too...

class Memory {
   std::unordered_map<unsigned int, unsigned int> mem;
   public:
      bool le;
      bool logStores;
      int defaultVal;
      Memory();
      void loadFile(std::string filename, unsigned int adr=0, bool le=false, std::string format="ascii");
      int read(unsigned int adr);
      void write(unsigned int adr, unsigned int dat);
      void write(unsigned int adr, unsigned int be, unsigned int dat);
};

Memory::Memory() {

   this->defaultVal = 0;
   this->le = false;
   this->logStores = true;

}

void Memory::loadFile(std::string filename, unsigned int adr, bool le, std::string format) {

   unsigned int dat;
   std::ifstream f;
   f.open(filename, std::fstream::in);
   // "ascii"
   //while (f.peek()!=EOF) {
      //f >> std::hex >> dat;
     // f >> dat;
   while (f >> std::hex >> dat) {
      this->write(adr, dat);
      adr += 4;
   }

}

// adr is word-aligned byte address
int Memory::read(unsigned int adr) {
   if (this->mem.find(adr) != this->mem.end()) {
      return this->mem[adr];
   } else {
      return this->defaultVal;
   }
}

// adr is word-aligned byte address
void Memory::write(unsigned int adr, unsigned int dat) {
   unsigned int startDat = this->read(adr);
   this->mem[adr] = dat;
   if (this->logStores) {
      std::cout << std::setw(8) << std::hex << " * Mem Update @" << adr << " " << startDat << "->" << dat << std::endl;
   }
}

void Memory::write(unsigned int adr, unsigned int be, unsigned int dat) {
   if (be == 0) return;

   int mask, startDat;
   if (be >= 8) {
      be = be - 8;
      mask = 0xFF000000;
   } else {
      mask = 0;
   }
   if (be >= 4) {
      be = be - 4;
      mask |= 0x00FF0000;
   }
   if (be >= 2) {
      be = be - 2;
      mask |= 0x0000FF00;
   }
   if (be = 1) {
      mask |= 0x000000FF;
   }

   startDat = this->read(adr);
   this->mem[adr] = (startDat & ~mask) | (dat & mask);
   if (this->logStores) {
      std::cout << std::setw(8) << std::hex << " * Mem Update @" << adr << " " << startDat << "->" << dat << std::endl;
   }

}

Memory mem;

int main(int argc, char **argv) {
   using namespace std;

   cout << setfill('0');

   Verilated::commandArgs(argc, argv);
   m = new Va2owb;

#ifdef TRACING
      Verilated::traceEverOn(true);
      t = new VerilatedVcdC;
      m->trace(t, 99);
      t->open("a2onode.vcd");
      cout << "Tracing enabled." << endl;
#endif

   bool resetDone = false;
   unsigned int threadStop = 0x3;

   unsigned int tick = 0;
   unsigned int cycle = 1;
   unsigned int readPending = 0;
   unsigned int readAddr = 0;
   unsigned int readTag = 0;
   unsigned int readTID = 0;
   unsigned int countReads = 0;
   bool wbRdPending = false, wbWrPending = false;

   //unsigned int iu0Comp = m->rootp->a2owb->c0->iu_lq_i0_completed;
   //unsigned int iu0Comp = m->rootp->a2owb__DOT__c0__DOT__lq0__DOT__lsq__DOT__odq__DOT__iu_lq_i0_completed_itag_int;
   /*
   iu0CompIFAR = sim.a2o.root.iuq0.iuq_cpl_top0.iuq_cpl0.cp2_i0_ifar
   iu1Comp = sim.a2o.root.iu_lq_i1_completed
   iu1CompIFAR = sim.a2o.root.iuq0.iuq_cpl_top0.iuq_cpl0.cp2_i1_ifar
   iuCompFlushIFAR = sim.a2o.root.cp_t0_flush_ifar
   cp3NIA = sim.a2o.root.iuq0.iuq_cpl_top0.iuq_cpl0.iuq_cpl_ctrl.cp3_nia_q           # nia after last cycle's completions
   */

   mem.write(0xFFFFFFFC, 0x48000002);
   mem.loadFile(testFile);

   m->rst = 1;
   cout << dec << setw(8) << cycle << " Resetting..." << endl;

   //m->an_ac_pm_thread_stop = threadStop;
   //cout << dec << setw(8) << cycle << " Thread stop=" << threadStop << endl;

   const int clocks[4] = {0x3, 0x2, 0x1, 0x0};     // 1x, 2x
   const int ticks1x = 4;

   while (!Verilated::gotFinish()) {

      if (!resetDone && (cycle > resetCycle)) {
         m->rst = 0;
         cout << dec << setw(8) << cycle << " Releasing reset." << endl;
         resetDone = true;
      }

      if (threadStop && (cycle > threadRunCycle)) {
         //threadStop = 0x0;
         //m->an_ac_pm_thread_stop = threadStop;
         //cout << dec << setw(8) << cycle << " Thread stop=" << threadStop << endl;
      }

      m->clk_1x = clocks[tick % ticks1x] >> 1;
      m->clk_2x = clocks[tick % ticks1x] & 0x1;

      tick++;
      m->eval();

      // bus is 1x clock
      if ((tick % ticks1x) == 0) {

         // wb
         m->wb_ack = 0;
         if (wbRdPending) {
            m->wb_datr = mem.read(m->wb_adr);
            m->wb_ack = 1;
            wbRdPending = false;
         } else if (wbWrPending) {
            mem.write(m->wb_adr, m->wb_datw, m->wb_sel);
            m->wb_ack = 1;
            wbWrPending = false;
         }

         if (!wbRdPending && !wbWrPending && m->wb_cyc && m->wb_stb) {
            if (!m->wb_we) {
               cout << dec << setw(8) << setfill('0') << uppercase << cycle << " WB RD RA=" << setw(8)  << hex << setfill('0') << m->wb_adr << endl;
               wbRdPending = true;
            } else {
               cout << dec << setw(8) << setfill('0') << uppercase << cycle << " WB WR RA=" << setw(8)  << hex << setfill('0') << m->wb_adr <<
                  " SEL=" << m->wb_sel << " DATA=" << m->wb_datw << endl;
               wbWrPending = true;
            }
         }

      }

      // finish clock stuff
      if ((tick % ticks1x) == 0) {
         cycle++;
         if ((cycle % hbCycles) == 0) {
            cout << dec << setw(8) << setfill('0') << cycle << " ...tick..." << endl;
         }
      }
      #ifdef TRACING
      t->dump(tick);
      t->flush();
      #endif

      // check for fails

      // hit limit
      if (cycle > runCycles) {
         break;
      }

   }

#ifdef TRACING
   t->close();
#endif
   m->final();

   exit(EXIT_SUCCESS);

}