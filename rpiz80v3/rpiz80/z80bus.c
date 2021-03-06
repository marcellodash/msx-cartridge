//
//  How to access GPIO registers from C-code on the Raspberry-Pi
//  Example program
//  15-January-2012
//  Dom and Gert
//  Revised: 15-Feb-2013
 
// Access from ARM Running Linux
 
#define BCM2708_PERI_BASE        0x3F000000 // Raspberry Pi 2
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
  
#include <stdio.h>
#include <stdlib.h>  
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <bcm2835.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>
#include <pthread.h>

#include "barrier.h"
 
#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)
 
int  mem_fd;
void *gpio_map;
 
// I/O access
volatile unsigned *gpio;
volatile unsigned *gpio10;
volatile unsigned *gpio7;
volatile unsigned *gpio13;
volatile unsigned *gpio1;
 
 
// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))
 
#define GPIO_SET *(gpio7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio10) // clears bits which are 1 ignores bits which are 0
 
#define GET_GPIO(g) (*(gpio13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH
#define GPIO (*(gpio13))
 
#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock

#include "z80.h"

void setup_io();
void reset();
int msxread(int slot, unsigned short addr);
void msxwrite(int slot, unsigned short addr, unsigned char byte);
int msxreadio(unsigned short addr);
void msxwriteio(unsigned short addr, unsigned char byte);
void clear_io();
void setDataIn();
void setDataOut();
void spi_clear();
void spi_set(int addr, int rd, int mreq, int slt1);

void setDataIn()
{
#if 0
	INP_GPIO(MD00_PIN);
	INP_GPIO(MD01_PIN);
	INP_GPIO(MD02_PIN);
	INP_GPIO(MD03_PIN);
	INP_GPIO(MD04_PIN);
	INP_GPIO(MD05_PIN);
	INP_GPIO(MD06_PIN);
	INP_GPIO(MD07_PIN);
#else
	*(gpio+1) &= 0x03f;
#endif	
}

/*
0099 9888 7776 6655 5444 3332 2211 1000

0099 9888 7776 6655 5444 3332 2211 1000
  00 1001 0010 0100 1001 0010 01xx xxxx
   0    9    2    4    9    2    7    f
0099 9888 7776 6655 5444 3332 2211 1000
*/

void inline setDataOut()
{
#if 0
	OUT_GPIO(MD00_PIN);
	OUT_GPIO(MD01_PIN);
	OUT_GPIO(MD02_PIN);
	OUT_GPIO(MD03_PIN);
	OUT_GPIO(MD04_PIN);
	OUT_GPIO(MD05_PIN);
	OUT_GPIO(MD06_PIN);
	OUT_GPIO(MD07_PIN);
#else
	*(gpio1) &= 0x0924927f;
	*(gpio1) |= 0x09249240;
	GPIO_SET = 0xff << MD00_PIN;	
#endif	
}

void inline spi_clear()
{
	int x;
#ifdef RPMC_V5
	GPIO_SET = LE_B;
#else
#ifdef RPMC_V4
	 GPIO_SET = SPI_SCLK | SPI_MOSI0 | SPI_MOSI1 | SPI_MOSI2;
	 GPIO_CLR = SPI_CS | SPI_SCLK;
#else	 
	 GPIO_CLR = SPI_SCLK;
	 GPIO_SET = SPI_MOSI;
	 for(x = 0; x < 24; x++)
	 {
		 GPIO_CLR = SPI_SCLK;
		 GPIO_SET = SPI_SCLK;
	 }
#endif	 
	 GPIO_SET = SPI_CS;
#endif	 
}	

void inline spi_set(int addr, int rd, int mreq, int slt1)
{
	int cs1, cs12, cs2, wr, iorq, x, spi_data, spi_result;
	int set, clr;
	if (!mreq) 
	{
		cs1 = !((addr & 0xc000) == 0x4000);
		cs2 = !((addr & 0xc000) == 0x8000);
		cs12 = cs1 && cs2;
	} 
	else
	{
		cs1 = cs2 = cs12 = 1;
	}
#ifdef RPMC_V5
#else
#if 1
	if (slt1 == 2)
		GPIO_CLR = MSX_SLTSL1;
	else
		GPIO_SET = MSX_SLTSL1;
#endif	
	GPIO_CLR = SPI_CS;
#if 0	
	spi_data = cs12 << 23 | cs1 << 22 | cs2 << 21 | mreq << 20 | !mreq << 19 | rd << 18 | !rd << 17 | slt1 << 16 | (addr & 0xffff);
	for(x = 23; x >= 0; x--)
	{
		GPIO_CLR = SPI_SCLK;
		if (spi_data & (1<<x))
		{
			GPIO_SET = SPI_MOSI;
		}
		else
		{
			GPIO_CLR = SPI_MOSI;
		}
		GPIO_SET = SPI_SCLK;
	}
#else
#ifdef RPMC_V4	
#if 1
	clr = SPI_SCLK | SPI_MOSI0 | SPI_MOSI1 | SPI_MOSI2;
	GPIO_CLR = clr;
	set = SPI_SCLK;
	if (cs12)			set |= SPI_MOSI0;
	if (addr & 1<<15)	set |= SPI_MOSI1;
	if (addr & 1<<7)	set |= SPI_MOSI2;
	GPIO_SET = set;
	GPIO_CLR = clr;
	set = SPI_SCLK;
	if (cs1)			set |= SPI_MOSI0; 
	if (addr & 1<<14)	set |= SPI_MOSI1;
	if (addr & 1<<6)	set |= SPI_MOSI2;
	GPIO_SET = set;
	GPIO_CLR = clr;
	set = SPI_SCLK;
	if (cs2)			set |= SPI_MOSI0;
	if (addr & 1<<13)	set |= SPI_MOSI1;
	if (addr & 1<<5)	set |= SPI_MOSI2;
	GPIO_SET = set;
	GPIO_CLR = clr;
	set = SPI_SCLK;
	if (mreq)			set |= SPI_MOSI0;
	if (addr & 1<<12)	set |= SPI_MOSI1;
	if (addr & 1<<4)	set |= SPI_MOSI2;
	GPIO_SET = set;
	GPIO_CLR = clr;
	set = SPI_SCLK;
	if (!mreq)			set |= SPI_MOSI0;
	if (addr & 1<<11)	set |= SPI_MOSI1;
	if (addr & 1<<3)	set |= SPI_MOSI2;
	GPIO_SET = set;
	GPIO_CLR = clr;
	set = SPI_SCLK;
	if (rd)				set |= SPI_MOSI0;
	if (addr & 1<<10)	set |= SPI_MOSI1;
	if (addr & 1<<2)	set |= SPI_MOSI2;
	GPIO_SET = set;
	GPIO_CLR = clr;
	set = SPI_SCLK;
	if (!rd)			set |= SPI_MOSI0; 
	if (addr & 1<<9)	set |= SPI_MOSI1;
	if (addr & 1<<1)	set |= SPI_MOSI2;
	GPIO_SET = set;
	GPIO_CLR = clr;
	set = SPI_SCLK;
	if (slt1 != 1 && !mreq)	set |= SPI_MOSI0;
	if (addr & 1<<8)	set |= SPI_MOSI1;
	if (addr & 1<<0)	set |= SPI_MOSI2;
	GPIO_SET = set;
	GPIO_CLR = clr;
#else
	GPIO_CLR = SPI_SCLK; 
	set = SPI_SCLK;
	if (cs12)			set |= SPI_MOSI0; else clr |= SPI_MOSI0; 
	if (addr & 1<<15)	set |= SPI_MOSI1; else clr |= SPI_MOSI1; 
	if (addr & 1<<7)	set |= SPI_MOSI2; else clr |= SPI_MOSI2; 
	if (cs1)			set |= SPI_MOSI0; else clr |= SPI_MOSI0; 
	GPIO_CLR = clr;
	GPIO_SET = set;
	GPIO_CLR = SPI_SCLK; 
	set = SPI_SCLK;
	clr = 0;
	if (addr & 1<<14)	set |= SPI_MOSI1; else clr |= SPI_MOSI1; 
	if (addr & 1<<6)	set |= SPI_MOSI2; else clr |= SPI_MOSI2; 
	if (cs2)			set |= SPI_MOSI0; else clr |= SPI_MOSI0; 
	if (addr & 1<<13)	set |= SPI_MOSI1; else clr |= SPI_MOSI1; 
	GPIO_CLR = clr;
	GPIO_SET = set;
	GPIO_CLR = SPI_SCLK; 
	set = SPI_SCLK;
	clr = 0;
	if (addr & 1<<5)	set |= SPI_MOSI2; else clr |= SPI_MOSI2; 
	if (mreq)			set |= SPI_MOSI0; else clr |= SPI_MOSI0; 
	if (addr & 1<<12)	set |= SPI_MOSI1; else clr |= SPI_MOSI1; 
	if (addr & 1<<4)	set |= SPI_MOSI2; else clr |= SPI_MOSI2; 
	GPIO_CLR = clr;
	GPIO_SET = set;
	GPIO_CLR = SPI_SCLK; 
	set = SPI_SCLK;
	clr = 0;
	if (!mreq)			set |= SPI_MOSI0; else clr |= SPI_MOSI0; 
	if (addr & 1<<11)	set |= SPI_MOSI1; else clr |= SPI_MOSI1; 
	if (addr & 1<<3)	set |= SPI_MOSI2; else clr |= SPI_MOSI2; 
	if (rd)				set |= SPI_MOSI0; else clr |= SPI_MOSI0; 
	GPIO_CLR = clr;
	GPIO_SET = set;
	GPIO_CLR = SPI_SCLK; 
	set = SPI_SCLK;
	clr = 0;
	if (addr & 1<<10)	set |= SPI_MOSI1; else clr |= SPI_MOSI1; 
	if (addr & 1<<2)	set |= SPI_MOSI2; else clr |= SPI_MOSI2; 
	if (!rd)			set |= SPI_MOSI0; else clr |= SPI_MOSI0; 
	if (addr & 1<<9)	set |= SPI_MOSI1; else clr |= SPI_MOSI1; 
	GPIO_CLR = clr;
	GPIO_SET = set;
	GPIO_CLR = SPI_SCLK; 
	set = SPI_SCLK;
	clr = 0;
	if (addr & 1<<1)	set |= SPI_MOSI2; else clr |= SPI_MOSI2; 
	if (slt1 != 1)		set |= SPI_MOSI0; else clr |= SPI_MOSI0; 
	if (addr & 1<<8)	set |= SPI_MOSI1; else clr |= SPI_MOSI1; 
	if (addr & 1<<0)	set |= SPI_MOSI2; else clr |= SPI_MOSI2; 
	GPIO_CLR = clr;
	GPIO_SET = set;
#endif	
#else
	GPIO_CLR = SPI_SCLK; if (cs12)	GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (cs1)	GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (cs2)	GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (mreq)	GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (!mreq)	GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (rd)	GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (!rd)	GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (slt1 != 1)	GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;	
	GPIO_CLR = SPI_SCLK; if (addr & 1<<15) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<14) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<13) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<12) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<11) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<10) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<9) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<8) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<7) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<6) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<5) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<4) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<3) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<2) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<1) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
#endif	
#endif	
	GPIO_SET = SPI_CS;
#endif
	return;
}
	

 int msxread(int slot, unsigned short addr)
 {
	unsigned char byte;
	int i, j;
	struct timespec tv, tr;
#ifdef RPMC_V5
	int cs1, cs2, cs12;
	cs1 = (addr & 0xc000) == 0x4000 ? MSX_CS1 : 0;
	cs2 = (addr & 0xc000) == 0x8000 ? MSX_CS2 : 0;
	cs12 = (cs1 || cs2) ? MSX_CS12 : 0;	
	GPIO_CLR = (0xffff) << MD00_PIN;
	GPIO_SET = LE_A | (addr & 0xffff) << MD00_PIN;
//	asm volatile ("nop;");	
	GPIO_CLR = LE_A;
	GPIO_SET = (slot == 2 ? MSX_SLTSL1 : slot == 1 ? MSX_SLTSL3 : 0) | MSX_IORQ | MSX_WR;
	GPIO_CLR = LE_B | (slot == 1 ? MSX_SLTSL1 : slot == 2 ? MSX_SLTSL3 : 0) | MSX_MREQ | MSX_RD | cs1 | cs2 | cs12 | 0xff << MD00_PIN;
	GET_DATA(byte);
	GET_DATA(byte);
	GET_DATA(byte);
	GET_DATA(byte);
	GET_DATA(byte);
	GET_DATA(byte);
	GET_DATA(byte);
	GET_DATA(byte);
	//printf("\n");
	GPIO_SET = LE_B;
#else	
	spi_set(addr, 0, 0, slot);
	GET_DATA(byte);
	GET_DATA(byte);
	GET_DATA(byte);
	GET_DATA(byte);
	spi_clear();
#endif
	return byte;	 
 }
 
 void msxwrite(int slot, unsigned short addr, unsigned char byte)
 {
	struct timespec tv, tr;
#ifdef RPMC_V5
	int cs1, cs2, cs12;
	cs1 = (addr & 0xc000) == 0x4000 ? MSX_CS1 : 0;
	cs2 = (addr & 0xc000) == 0x8000 ? MSX_CS2 : 0;
	cs12 = (cs1 || cs2) ? MSX_CS12 : 0;	
	GPIO_CLR = LE_A | 0xffff << MD00_PIN;
	GPIO_SET = LE_B | (addr & 0xffff) << MD00_PIN;
	asm volatile ("nop;");	
	GPIO_SET = LE_A;
	GPIO_SET = 0xff00 << MD00_PIN;
	GPIO_CLR = 0xff << MD00_PIN;
 	GPIO_SET = byte << MD00_PIN;
	GPIO_SET = (slot == 2 ? MSX_SLTSL1 : slot == 1 ? MSX_SLTSL3 : 0) | MSX_IORQ | MSX_RD;
	GPIO_CLR = LE_B | (slot == 1 ? MSX_SLTSL1 : slot == 2 ? MSX_SLTSL3 : 0) | MSX_MREQ | MSX_WR;
 	GPIO_SET = byte << MD00_PIN;
 	GPIO_SET = byte << MD00_PIN;
 	GPIO_SET = byte << MD00_PIN;
 	GPIO_SET = byte << MD00_PIN;
 	GPIO_SET = byte << MD00_PIN;
 	GPIO_SET = byte << MD00_PIN;
 	GPIO_SET = byte << MD00_PIN;
 	GPIO_SET = byte << MD00_PIN;
 	GPIO_SET = byte << MD00_PIN;
 	GPIO_SET = byte << MD00_PIN;
 	GPIO_SET = byte << MD00_PIN;
 	GPIO_SET = byte << MD00_PIN;
	GPIO_SET = LE_B;
#else	
	tv.tv_sec = 0;
	tv.tv_nsec = 20;
	spi_set(addr, 1, 0, slot);
	nanosleep(&tv, &tr);
	spi_clear();
	GPIO_SET = 0xff << MD00_PIN;
#endif	
	return;
 }
 
 int msxreadio(unsigned short addr)
 {
	unsigned char byte;
	struct timespec tv, tr;
	tv.tv_sec = 0;
	tv.tv_nsec = 10;
	spi_set(addr, 0, 1, 0);
	nanosleep(&tv, &tr);
	GET_DATA(byte);
	spi_clear();
	return byte;	 
 }
 
 void msxwriteio(unsigned short addr, unsigned char byte)
 {
	 struct timespec tv, tr;
	 tv.tv_sec = 0;
	 tv.tv_nsec = 400;
 	 SET_DATA(byte);
	 spi_set(addr, 1, 1, 0);
	 nanosleep(&tv, &tr);
	 spi_clear();
 	 GPIO_SET = 0xff << MD00_PIN;
	 return;
 }
 
#if 0 
int rtapi_open_as_root(const char *filename, int mode) {
	fprintf (stderr, "euid: %d uid %d\n", geteuid(), getuid());
	seteuid(0);
	fprintf (stderr, "euid: %d uid %d\n", geteuid(), getuid());
	setfsuid(geteuid());
	int r = open(filename, mode);
	setfsuid(getuid());
	return r;
}
#endif
//
// Set up a memory regions to access GPIO
//
void setup_io()
{
   int i ;	
   /* open /dev/mem */
   if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/mem \n");
      exit(-1);
   }
 
   /* mmap GPIO */
   gpio_map = mmap(
      NULL,             //Any adddress in our space will do
      BLOCK_SIZE,       //Map length
      PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
      MAP_SHARED,       //Shared with other processes
      mem_fd,           //File to map
      GPIO_BASE         //Offset to GPIO peripheral
   );
 
   close(mem_fd); //No need to keep mem_fd open after mmap
 
   if (gpio_map == MAP_FAILED) {
      printf("mmap error %d\n", (int)gpio_map);//errno also set!
      exit(-1);
   }
 
   // Always use volatile pointer!
   gpio = (volatile unsigned *)gpio_map;
   gpio10 = gpio+10;
   gpio7 = gpio+7;
   gpio13 = gpio+13;
   gpio1 = gpio+1;
   
} // setup_io

#if 0
#define EINVAL 0
int stick_this_thread_to_core(int core_id) {
   int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
   if (core_id < 0 || core_id >= num_cores)
      return EINVAL;

   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   CPU_SET(core_id, &cpuset);
   return sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}
#endif
void clear_io()
{
	spi_clear();
}

void msxinit()
{
	const struct sched_param priority = {1};
	sched_setscheduler(0, SCHED_FIFO, &priority);  
//	stick_this_thread_to_core(0);
	setup_io();
}

void msxclose()
{
	clear_io();
}



#ifdef _MAIN


int main(int argc, char **argv)
{
  int g,rep,i,addr, page=4, c= 0,addr0;
  char byte, byte0, io;
  int offset = 0x4000;
  int size = 0x8000;
  FILE *fp = 0;
  struct timespec t1, t2;
  double elapsedTime = 0;
  int binary = 0;
  io = 0;
  if (argc > 1)
  {
	 if (strcmp(argv[1], "io"))
		fp = fopen(argv[1], "wb");
	 else
		io = 1;
  }
  if (argc > 2)
  {
	  offset = atoi(argv[2]);
  }
  if (argc > 3)
  {
	  size = atoi(argv[3]);
  }
 
  // Set up gpi pointer for direct register access
  setup_io();
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t1);
	if (io > 0)
	{
		for(i = 0; i < 256; i++)
		{
			if (i % 16 == 0)
				printf("%02x: ", i);
			printf("%02x ", msxreadio(i));
			if (i > 0 && i % 16 == 15)
				printf("\n");
		}
		exit(0);
	}
	msxwrite(1, 0x6000, 3);
	offset = 0x4000;
	for(addr=offset; addr < offset + size; addr ++)
	{
#if 0
		addr0 = 0xffff & (addr + (rand() % 2));//0xffff & (0x4000 + rand());
		printf("%04x:%02x\n", addr0, 0xff & msxread(1, addr0));
		addr0 = 0xffff & (addr + (rand() % 2));//0xffff & (0x4000 + rand());
		printf("%04x:%02x\n", addr0, 0xff & msxread(1, addr0));
#else		
	  if (addr > 0xbfff)
	  {
		 if (!(addr & 0x1fff)) {
			msxwrite(1, 0x6000, page++);
			printf("page:%d, address=0x%04x\n", page-1, addr );
		 }
		 byte = msxread(1, 0x6000 + (addr & 0x1ffff));
	  }
	  else
	  {
		  byte = msxread(1, addr);
	  }
	  if (fp)
		  fwrite(&byte, 1, 1, fp);
	  else
	  {
#if 1		 
		if (addr % 16 == 0)
			 printf("\n%04x:", addr);
#if 1		  
		c = 0;
		for(i=0;i<10;i++)
		{
			byte0 = msxread(1, addr);
			if (byte != byte0)
				c = 1;
		}
		if (c)  
			printf("\e[31m%02x \e[0m", byte);
		else
			printf("%02x ", byte);
#else
		printf("%02x ", byte);
#endif	
#endif
	  }
#endif
	}
	printf("\n");
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t2);
	elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000000000.0;      // sec to ns
	elapsedTime += (t2.tv_nsec - t1.tv_nsec) ;   // us to ns	
	if (!binary) {
		printf("elapsed time: %10.2fs, %10.2fns/i\n", elapsedTime/100000000, elapsedTime / size);
	}	
  clear_io();
  return 0;
 
} // main
#endif
