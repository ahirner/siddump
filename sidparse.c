#include <stdio.h>
//#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "cpu.h"

#define MAX_INSTR 0x1000000 //1 << 18
#define CPU_FREQ 985248
#define SCREEN_REFRESH 50

#define CYCLE_COUNTER_MAX 0x1FFF
#define CYCLE_OVERFLOW_MARGIN 10
#define CYCLE_SCREEN_REFRESH  (CPU_FREQ / SCREEN_REFRESH)

typedef struct
{
  unsigned short freq;
  unsigned short pulse;
  unsigned short adsr;
  unsigned char wave;
  int note;
} CHANNEL;

typedef struct
{
  unsigned short cutoff;
  unsigned char ctrl;
  unsigned char type;
} FILTER;

int main(int argc, char **argv);
unsigned char readbyte(FILE *f);
unsigned short readword(FILE *f);

CHANNEL chn[3];
CHANNEL prevchn[3];
CHANNEL prevchn2[3];
FILTER filt;
FILTER prevfilt;

extern unsigned short pc;
extern unsigned int cpucycles;
extern unsigned int last_mem_write;

const char *notename[] =
 {"C-0", "C#0", "D-0", "D#0", "E-0", "F-0", "F#0", "G-0", "G#0", "A-0", "A#0", "B-0",
  "C-1", "C#1", "D-1", "D#1", "E-1", "F-1", "F#1", "G-1", "G#1", "A-1", "A#1", "B-1",
  "C-2", "C#2", "D-2", "D#2", "E-2", "F-2", "F#2", "G-2", "G#2", "A-2", "A#2", "B-2",
  "C-3", "C#3", "D-3", "D#3", "E-3", "F-3", "F#3", "G-3", "G#3", "A-3", "A#3", "B-3",
  "C-4", "C#4", "D-4", "D#4", "E-4", "F-4", "F#4", "G-4", "G#4", "A-4", "A#4", "B-4",
  "C-5", "C#5", "D-5", "D#5", "E-5", "F-5", "F#5", "G-5", "G#5", "A-5", "A#5", "B-5",
  "C-6", "C#6", "D-6", "D#6", "E-6", "F-6", "F#6", "G-6", "G#6", "A-6", "A#6", "B-6",
  "C-7", "C#7", "D-7", "D#7", "E-7", "F-7", "F#7", "G-7", "G#7", "A-7", "A#7", "B-7"};

const char *filtername[] =
 {"Off", "Low", "Bnd", "L+B", "Hi ", "L+H", "B+H", "LBH"};

unsigned char freqtbllo[] = {
  0x17,0x27,0x39,0x4b,0x5f,0x74,0x8a,0xa1,0xba,0xd4,0xf0,0x0e,
  0x2d,0x4e,0x71,0x96,0xbe,0xe8,0x14,0x43,0x74,0xa9,0xe1,0x1c,
  0x5a,0x9c,0xe2,0x2d,0x7c,0xcf,0x28,0x85,0xe8,0x52,0xc1,0x37,
  0xb4,0x39,0xc5,0x5a,0xf7,0x9e,0x4f,0x0a,0xd1,0xa3,0x82,0x6e,
  0x68,0x71,0x8a,0xb3,0xee,0x3c,0x9e,0x15,0xa2,0x46,0x04,0xdc,
  0xd0,0xe2,0x14,0x67,0xdd,0x79,0x3c,0x29,0x44,0x8d,0x08,0xb8,
  0xa1,0xc5,0x28,0xcd,0xba,0xf1,0x78,0x53,0x87,0x1a,0x10,0x71,
  0x42,0x89,0x4f,0x9b,0x74,0xe2,0xf0,0xa6,0x0e,0x33,0x20,0xff};

unsigned char freqtblhi[] = {
  0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x02,
  0x02,0x02,0x02,0x02,0x02,0x02,0x03,0x03,0x03,0x03,0x03,0x04,
  0x04,0x04,0x04,0x05,0x05,0x05,0x06,0x06,0x06,0x07,0x07,0x08,
  0x08,0x09,0x09,0x0a,0x0a,0x0b,0x0c,0x0d,0x0d,0x0e,0x0f,0x10,
  0x11,0x12,0x13,0x14,0x15,0x17,0x18,0x1a,0x1b,0x1d,0x1f,0x20,
  0x22,0x24,0x27,0x29,0x2b,0x2e,0x31,0x34,0x37,0x3a,0x3e,0x41,
  0x45,0x49,0x4e,0x52,0x57,0x5c,0x62,0x68,0x6e,0x75,0x7c,0x83,
  0x8b,0x93,0x9c,0xa5,0xaf,0xb9,0xc4,0xd0,0xdd,0xea,0xf8,0xff};

int main(int argc, char **argv)
{
  int subtune = 0;
  int terminalframe = 60*50;
  int frames = 0;
  int spacing = 0;
  int pattspacing = 0;
  int firstframe = 0;
  int counter = 0;
  int basefreq = 0;
  int basenote = 0xb0;
  int lowres = 0;
  int rows = 0;
  int oldnotefactor = 1;
  int timeseconds = 0;
  int usage = 0;
  int profiling = 0;
  int csv_output = 0;
  int binary_out = 0;
  int in_handle = 0;

  unsigned loadend;
  unsigned loadpos;
  unsigned loadsize;
  unsigned loadaddress;
  unsigned initaddress;
  unsigned playaddress;
  unsigned dataoffset;
  FILE *in;
  char *sidname = 0;
  int c;

  // Scan arguments
  for (c = 1; c < argc; c++)
  {
    if (argv[c][0] == '-')
    {
      switch(toupper(argv[c][1]))
      {
        case '?':
        usage = 1;
        break;

        case 'A':
        sscanf(&argv[c][2], "%u", &subtune);
        break;

        case 'C':
        sscanf(&argv[c][2], "%x", &basefreq);
        break;

        case 'D':
        sscanf(&argv[c][2], "%x", &basenote);
        break;

        case 'F':
        sscanf(&argv[c][2], "%u", &firstframe);
        break;

        case 'L':
        lowres = 1;
        break;

        case 'N':
        sscanf(&argv[c][2], "%u", &spacing);
        break;

        case 'O':
        sscanf(&argv[c][2], "%u", &oldnotefactor);
        if (oldnotefactor < 1) oldnotefactor = 1;
        break;

        case 'P':
        sscanf(&argv[c][2], "%u", &pattspacing);
        break;

        case 'S':
        timeseconds = 1;
        break;

        case 'T':
        sscanf(&argv[c][2], "%u", &terminalframe);
        break;

        case 'Z':
        profiling = 1;
        break;

        case 'R':
        csv_output = 1;
        break;

        case 'B':
        binary_out = 1;
        break;

        case 'I':
        sscanf(&argv[c][2], "%u", &in_handle);
        break;
      }
    }
    else
    {
      if (!sidname) sidname = argv[c];
    }
  }

  // Usage display
  if ((argc < 2) || (usage))
  {
    fprintf(stderr, "Usage: SIDDUMP <sidfile> [options]\n"
           "Warning: CPU emulation may be buggy/inaccurate, illegals support very limited\n\n"
           "Options:\n"
           "-a<value> Accumulator value on init (subtune number) default = 0\n"
           "-c<value> Frequency recalibration. Give note frequency in hex\n"
           "-d<value> Select calibration note (abs.notation 80-DF). Default middle-C (B0)\n"
           "-f<value> First frame to display, default 0\n"
           "-l        Low-resolution mode (only display 1 row per note)\n"
           "-n<value> Note spacing, default 0 (none)\n"
           "-o<value> ""Oldnote-sticky"" factor. Default 1, increase for better vibrato display\n"
           "          (when increased, requires well calibrated frequencies)\n"
           "-p<value> Pattern spacing, default 0 (none)\n"
           "-s        Display time in minutes:seconds:frame format\n"
           "-t<value> Terminal frame, default 60*50\n"
           "-z        Include CPU cycles+rastertime (PAL)+rastertime, badline corrected\n"
           "-r        Output table in a command-separated values file (.csv)\n"
           "------------- pipe interface ---------- \n"
           "-b        Output binary (resid compatible) \n"
           "-i        Input from file handle (> 3) instead filename");
    return 1;
  }

  // Recalibrate frequencytable
  if (basefreq)
  {
    basenote &= 0x7f;
    if ((basenote < 0) || (basenote > 96))
    {
      fprintf(stderr, "Warning: Calibration note out of range. Aborting recalibration.\n");
    }
    else
    {
      for (c = 0; c < 96; c++)
      {
        double note = c - basenote;
        double freq = (double)basefreq * pow(2.0, note/12.0);
        int f = freq;
        if (freq > 0xffff) freq = 0xffff;
        freqtbllo[c] = f & 0xff;
        freqtblhi[c] = f >> 8;
      }
    }
  }

  // Check other parameters for correctness
  if ((lowres) && (!spacing)) lowres = 0;
  if (firstframe >= terminalframe){
    fprintf(stderr, "Error: Terminal frame must be higher than first frame.\n");
    return 1;
  }

  // Open SID file
  if ((!sidname) && (!in_handle))
  {
    fprintf(stderr, "Error: No SID input file.\n");
    return 1;
  }
  else if (in_handle)
  {
    in = fdopen(in_handle, "rb");
    if (!in)
    {
      fprintf(stderr, "Error: couldn't open handle %d.\n", in_handle);
      return 1;
    }
  }
  else
  {
    in = fopen(sidname, "rb");
    if (!in)
    {
      fprintf(stderr, "Error: couldn't open SID file.\n");
      return 1;
    }
  }

  // Read interesting parts of the SID header
  fseek(in, 6, SEEK_SET);
  dataoffset = readword(in);
  loadaddress = readword(in);
  initaddress = readword(in);
  playaddress = readword(in);
  fseek(in, dataoffset, SEEK_SET);
  if (loadaddress == 0)
    loadaddress = readbyte(in) | (readbyte(in) << 8);

  // Load the C64 data
  loadpos = ftell(in);
  fseek(in, 0, SEEK_END);
  loadend = ftell(in);
  fseek(in, loadpos, SEEK_SET);
  loadsize = loadend - loadpos;
  if (loadsize + loadaddress >= 0x10000)
  {
    fprintf(stderr, "Error: SID data continues past end of C64 memory.\n");
    fclose(in);
    return 1;
  }
  fread(&mem[loadaddress], loadsize, 1, in);
  fclose(in);

  if (playaddress == 0)
  {
    fprintf(stderr, "Warning: SID has play address 0, reading from interrupt vector instead\n");
    if ((mem[0x01] & 0x07) == 0x5)
      playaddress = mem[0xfffe] | (mem[0xffff] << 8);
    else
      playaddress = mem[0x314] | (mem[0x315] << 8);
    fprintf(stderr, "New play address is $%04X\n", playaddress);
  }

  // Clear channelstructures in preparation & print first time info
  memset(&chn, 0, sizeof chn);
  memset(&filt, 0, sizeof filt);
  memset(&prevchn, 0, sizeof prevchn);
  memset(&prevchn2, 0, sizeof prevchn2);
  memset(&prevfilt, 0, sizeof prevfilt);
  fprintf(stderr, "Calling playroutine until frame %d, starting output from frame %d\n", terminalframe, firstframe);
  fprintf(stderr, "Middle C frequency is $%04X\n\n", freqtbllo[48] | (freqtblhi[48] << 8));

  if (binary_out == 0) {
    if (csv_output) {
      printf("frame,freq_1,note/abs_1,wf_1,adsr_1,pulse_1,freq_2,note/abs_2,wf_2,adsr_2,pulse_2,freq_3,note/abs_3,wf_3,adsr_3,pulse_3,fcut,res,ftype,vol");
    } else {
      printf("| Frame | Freq Note/Abs WF ADSR Pul | Freq Note/Abs WF ADSR Pul | Freq Note/Abs WF ADSR Pul | FCut RC Typ V |");
    }

    if (profiling)
    { // CPU cycles, Raster lines, Raster lines with badlines on every 8th line, first line included
      if (csv_output) {
        printf(",cycles,rl,rb");
      } else {
        printf(" Cycl RL RB |");
      }
    }
    printf("\n");

    if (!csv_output) {
      printf("+-------+---------------------------+---------------------------+---------------------------+---------------+");
      if (profiling)
      {
        printf("------------+");
      }
      printf("\n");
    }
  }



  // Data collection & display loop
  int initializing = 1;
  do
  {

    if (initializing) {
        // Print info & run initroutine
        fprintf(stderr, "Load address: $%04X Init address: $%04X Play address: $%04X\n", loadaddress, initaddress, playaddress);
        fprintf(stderr, "Calling initroutine with subtune %d\n", subtune);
        mem[0x01] = 0x37;
        initcpu(initaddress, subtune, 0, 0);
    }

    // Inner cycle data
    int instr = 0;
    int last_cpu_cycle = cpucycles = 0;
    int bogus_frame_cycles = 0;

    while (runcpu())
    {
      instr++;

      int delta_cpu_cycles = cpucycles - last_cpu_cycle;

      // Emit NOP so that cycle counter overflow is prevented
      // "Register" 25
      if ((delta_cpu_cycles - CYCLE_OVERFLOW_MARGIN) >= CYCLE_COUNTER_MAX)
      {
        if (binary_out)
        {
          unsigned int delta_m = (delta_cpu_cycles & CYCLE_COUNTER_MAX) << 16;
          unsigned int out = delta_m | 25 << 8;
          write(1, &out, 4);
        }
        else fprintf(stdout, "%06x: Cycle counter overflow prevented at delta:%04x\n", instr, delta_cpu_cycles);
        fprintf(stderr, "%06x: Warning: Cycle counter overflow prevented at delta:%04x\n", instr, delta_cpu_cycles);

        last_cpu_cycle = cpucycles;
        delta_cpu_cycles = 0;
      }

      // SID register write dumps
      int reg = last_mem_write - 0xd400;
      if ((reg >= 0) && (reg < 25))
      {
        unsigned char v = mem[last_mem_write];

        unsigned int delta_m = ((unsigned)delta_cpu_cycles & CYCLE_COUNTER_MAX) << 16;
        unsigned int out = delta_m | reg << 8 | v;
       
        if (binary_out) write(1, &out, 4);
        else fprintf(stdout, "%06x: INSTR %04x, cycle delta:%04x\n", instr, reg << 8 | v, delta_cpu_cycles);

        last_cpu_cycle = cpucycles;
        delta_cpu_cycles = 0;

        //reset mem_write so that if next instructions don't write to mem, nothing is emitted more than once
        last_mem_write = 0;
      }

      // Test for jump into Kernal interrupt handler exit
      // Todo, should initializing also respect these exits?
      if ((initializing == 0) && ((mem[0x01] & 0x07) != 0x5 && (pc == 0xea31 || pc == 0xea81)))
        break;

      // Test for artificial frame so that FPS are maintained
      if ((cpucycles - bogus_frame_cycles) >= CYCLE_SCREEN_REFRESH)
      {
        // Frame out because no other suspend function
        if (binary_out)
        {
          unsigned int delta_m = (delta_cpu_cycles & CYCLE_COUNTER_MAX) << 16;
          unsigned int out = 1 << 31 | delta_m | (frames & 0xFFFF); //| initializing | (1 << 1);
          write(1, &out, 4);
        }
        else fprintf(stdout, "%06x, FRAME %04x, cycle delta:%04x, init=%d [irregular]\n", instr, frames, delta_cpu_cycles, initializing);
        // do not re-init cpu nor break but wait for hard stop
        frames++;
        
        if (frames >= terminalframe){
          fprintf(stderr, "Terminal frame reached after bogus FRAME emit, exiting\n");
          return 0;
        }
        bogus_frame_cycles = cpucycles;
      }
      // Test for hard stop
      // Todo: don't exit if it was outputting still audio
      if (instr > MAX_INSTR)
      {
        fprintf(stderr, "Error: CPU executed abnormally high amount of instructions, exiting\n");
        return 1;
      }
    }

  {
      // Regular Frame out (interrupt or cpu suspended)
      int delta_cpu_cycles = cpucycles - last_cpu_cycle;
      if (binary_out)
      {
        unsigned int delta_m = (delta_cpu_cycles & CYCLE_COUNTER_MAX) << 16;
        unsigned int out = 1 << 31 | delta_m | (frames & 0xFFFF); //| initializing;
        write(1, &out, 4);
      }
      else
      {
        printf("%06x, FRAME %04x, cycle delta:%04x, init=%d\n", instr, frames, delta_cpu_cycles, initializing);
      }

  }

  // Advance state for re-entry
  initializing = 0;
  // Init cpu normally with
  // Playroutine
  initcpu(playaddress, 0, 0, 0);
  frames++;

  }
  while (frames < terminalframe);
  
  return 0;
}

unsigned char readbyte(FILE *f)
{
  unsigned char res;

  fread(&res, 1, 1, f);
  return res;
}

unsigned short readword(FILE *f)
{
  unsigned char res[2];

  fread(&res, 2, 1, f);
  return (res[0] << 8) | res[1];
}
