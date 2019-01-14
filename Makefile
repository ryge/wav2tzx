vpath %.h ../headers

all: wav2tzx

clean:
	rm wav2tzx

wav2tzx:
	cc -lm -I inc src/decode_wav.c -o wav2tzx
