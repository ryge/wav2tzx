/*
 * -------------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE":
 * <andreasryge@gmail.com> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.           Andreas Ryge
 * --------------------------------------------------------------------------------
 */

#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h> // for memset

#include "tzx.h"

void usage(const char* name)
{
	printf("Usage\n\t%s <input file> <output file>\n", name);
}

uint32_t ChangeEndianness(uint32_t value)
{
    uint32_t result = 0;
    result |= (value & 0x000000FF) << 24;
    result |= (value & 0x0000FF00) << 8;
    result |= (value & 0x00FF0000) >> 8;
    result |= (value & 0xFF000000) >> 24;
    return result;
}

// Stuff to decode binary values read from audio file
uint8_t val = 0;
static int b = 0;
struct ID10 block;

void push(uint8_t v)
{
	val = (val << 1) | v;
	b++ ;
	if(b==8)
	{
		block.data[block.size++] = val;
		//printf("{%c,%d}",val, data[block.size-1]);
		val=0;
		b=0;
	}
}

void pause(float pause_ms, FILE* f) 
{ 
	if(b!=0) printf("{%c}",val);
	val=0; 
	b=0;
	block.pause=pause_ms;
	
	uint8_t type = 0x10;
	fwrite(&type, sizeof(uint8_t), 1, f);
	fwrite(&block.pause, sizeof(uint16_t), 1, f);
	fwrite(&block.size, sizeof(uint16_t), 1, f);
	fwrite(&block.data, sizeof(uint8_t), block.size, f);
	
	block.size=0; 
};	

// Header of a wave file
struct wav {
	struct {
		char id[4]; 
		uint32_t size; 
		char format[4];
	} riff;
	struct {
		char id[4]; 
		uint32_t size;
		uint16_t format;
		uint16_t channels;
		uint32_t sample_rate;
		uint32_t byte_rate;
		uint16_t block_align;
		uint16_t bits_per_sample;
	} fmt;
	struct {
		char id[4]; 
		uint32_t size;
	} data;
};

// Initialise a TZX file with a header
FILE* createFile(const char* fname) {
	FILE* f = fopen(fname, "w");
	if(NULL==f) return f;
	struct TZXheader header;
	strncpy(header.signature, "ZXTape!", 7);
	uint8_t magic = 0x1a;
	header.major = 1;
	header.minor = 10;
	fwrite(&header.signature, sizeof(char), 7, f);
	fwrite(&magic, sizeof(uint8_t), 1, f);
	fwrite(&header.major, sizeof(char), 1, f);
	fwrite(&header.signature, sizeof(char), 1, f);
	
	//Write Archive info
	uint8_t len = (1+strlen(fname));

	uint8_t type = 0x32;
	uint16_t size = 1+len;
	uint8_t number = 1;

	fwrite(&type, sizeof(uint8_t), 1, f);
	fwrite(&size, sizeof(uint16_t), 1, f);
	fwrite(&number, sizeof(uint8_t), 1, f);

	uint8_t stype = 0x00;
	fwrite(&stype, sizeof(uint8_t), 1, f);
	fwrite(&len, sizeof(uint8_t), 1, f);
	fwrite(fname, sizeof(char), len, f);
	
	return f;
}

unsigned int ground = 127;
enum states {NOISE, PILOT, SYNC, DATA, ONE, ZERO, END};
enum pulse {PULSE_LOW=95, PULSE_HIGH=159, PULSE_UNDEF=0};
enum pulse last_pulse = PULSE_UNDEF;

int get_pulsewidth(FILE* file)
{
	int count = 0;
	uint8_t b;
	enum pulse current_pulse;
	last_pulse = PULSE_UNDEF;
	do{
		int n= fread(&b, sizeof(b), 1, file);
		if(n==0) return 0;
		current_pulse = b > ground? PULSE_HIGH : PULSE_LOW;
		//printf("%c", current_pulse==PULSE_HIGH? '^':'_');
		if(last_pulse==PULSE_UNDEF) last_pulse = current_pulse;
		count++;
	} while(last_pulse == current_pulse);
	//printf(" ");
	return count;
}

// The main 
int main( int argc, const char** argv )
{
	if(argc != 3)
	{
		usage(argv[0]);
		return 1;
	}

	// Init data struct
	memset(&block.data, 65536, sizeof(uint8_t));
	block.size = 0;

	// Handle files
	const char* infname = argv[1];
	const char* outfname = argv[2];
	printf("files: %s, %s \n", infname,outfname);

	FILE *infile, *outfile;
	if( (infile = fopen(infname, "r")) == 0) {
		printf("Could not open file \"%s\"\n", infname);
	}
	
	outfile = createFile(outfname);
	if(NULL==outfile) 
	{
		printf("Could not open file \"%s\"\n", outfname);
	}

   if(NULL == infile || NULL == outfile)
   {
     fclose(infile);
     fclose(outfile);
     exit(1);
   }

	struct wav header;
	int32_t num;
	fread(&header, sizeof(header), 1, infile);
	
	printf("riff.id: %.4s\n", header.riff.id);
	printf("riff.size: %d\n", header.riff.size);

	printf("fmt.format: %d\n", header.fmt.format);
	printf("fmt.channels: %d\n", header.fmt.channels);
	printf("fmt.sample_rate: %d\n", header.fmt.sample_rate);
	printf("fmt.byte_rate: %d\n", header.fmt.byte_rate);
	printf("fmt.blocks_align: %d\n", header.fmt.block_align);
	printf("fmt.bits_per_sample: %d\n", header.fmt.bits_per_sample);
	printf("data.size: %d\n", header.data.size);

   ground = ( (1 << header.fmt.bits_per_sample) - 1)/2;
	float t_state_duration = 1/3500000.0;
	int pilot = round(2168 * t_state_duration * header.fmt.sample_rate);
	int sync1 = round( 667 * t_state_duration * header.fmt.sample_rate);
	int sync2 = round( 735 * t_state_duration * header.fmt.sample_rate);
	int zero  = round( 855 * t_state_duration * header.fmt.sample_rate);
	int one   = round(1710 * t_state_duration * header.fmt.sample_rate);
	int pilot_l = round(header.fmt.sample_rate / (float)(2 * pilot));

	unsigned char c;
	int cnt = 0;
	int pilot_n=0;
	
	enum state {LOW, HIGH, UNDEF};
	enum state read, current=UNDEF;
	
	uint8_t slack = 6;
	
	enum states state = NOISE;
	int pulsewidth;
	while(0 != (pulsewidth = get_pulsewidth(infile))) {
		switch(state) {
			case NOISE: 
			{
				if( abs(pulsewidth-pilot) < slack ) 
				{ 
					pilot_n++;
					if(pilot_n>pilot_l){
						state = PILOT;
						pilot_n=0;
					}
				}
				else pilot_n=0;
				break;
			}
			case PILOT: 
			{
				if(abs(pulsewidth-sync1) < slack) 
				{
					state = SYNC;
				}
				else if(abs(pulsewidth-pilot) < slack) 
				{
					pilot_n++;
					state = PILOT; 
				}
				else {
					state=NOISE;
				}
				break;
			}
			case SYNC: 
			{
				if(abs(pulsewidth-sync2) < slack) 
				{
					state = DATA;
				}
				else {
					state=NOISE;
				}
				break;
			}
			case DATA: 
			{
				if(abs(pulsewidth-one) < slack) 
				{
					state = ONE;
				}
				else if(abs(pulsewidth-zero)< 2*slack) 
				{
					state = ZERO;
				}
				else {
					pause(1000, outfile);
					state=NOISE;
				}
				break;
			}
			case ONE: 
			{
				if(abs(pulsewidth-one) < slack) 
				{
					push(1);
					state = DATA;
				}
				else {
					pause(1000, outfile);
					state=NOISE;
				}
				break;
			}
			case ZERO: 
			{
				if(abs(pulsewidth-zero) < slack) 
				{
					push(0);
					state = DATA;
				}
				else {
					pause(1000, outfile);
					state=NOISE;
				}
				break;
			}
			default: 
			{
				printf("can't handle pulsewidth: %d\n", pulsewidth);
			}
		}
	}
	pause(0, outfile);
	
	fclose(infile);
	fclose(outfile);
	return 0;
}
