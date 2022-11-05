#include "encode.h"
#include "compress.h"
#include "decode.h"

#if 0
	/* log literal en pointer len combi's */
	static unsigned long log_pos_counter=0;
	#define LOG_LITERAL(lit)  {printf("%lX Literal: %02X\n", log_pos_counter, lit); log_pos_counter++;}
	#define LOG_PTR_LEN(len, ptr) {printf("%lX Len: %u, ptr: %u\n", log_pos_counter ,len, ptr); log_pos_counter+=len;}
	#define LOG_BIT(bit) // printf("bit = %i\n",bit);
  	#define LOG_RUN(run) printf("Run = %lu\n", run);
	#define LOG_COUNTER_RESET log_pos_counter=0;
	#define LOG_TEXT(string) printf(string);
#else
	#define LOG_LITERAL(lit) /* */
	#define LOG_PTR_LEN(len, ptr) /* */
	#define LOG_BIT(bit) /* */
	#define LOG_RUN(run) /* */
 	#define LOG_COUNTER_RESET
	#define LOG_TEXT(string) /* */
#endif


int n9_lit_len(uint32 val);
int n9_len_len(uint32 val);
int n9_ptr_len(uint32 val);
void store_n9_val(uint32 val, packstruct *com);
void store_n9_len_val(uint32 val, packstruct *com);
void store_n9_literal_val(uint32 val, packstruct *com);
void store_n9_ptr_val(int32_t val, packstruct *com);

int n9_len_len(uint32 val)
{ /* bereken de code lengte voor val, 2 <= val <= 2^32 */
	return 2*(first_bit_set32(val-1)-1);
}

int n9_ptr_len(uint32 val)
{ /* bereken de code lengte voor val, 0 <= val <= 65536 */
	if(val<256)
	{
		return 9;
	}
	else
	{
		return 17;
	}
}

#define ST_BIT_N9(bit)												\
{ /* store a 1 or a 0 */											\
	int val=bit;			                                 \
	LOG_BIT(val);														\
	if(com->bits_in_bitbuf==0)										\
	{ /* reserveer plek in bytestream */						\
		com->command_byte_ptr=com->rbuf_current++;			\
		com->bitbuf=0;													\
	}																		\
	com->bits_in_bitbuf++;											\
	com->bitbuf+=com->bitbuf+val;									\
	if (com->bits_in_bitbuf >= 8)									\
	{																		\
		com->bits_in_bitbuf=0;										\
		*com->command_byte_ptr=(uint8)com->bitbuf;			\
	}																		\
}

void store_n9_val(uint32 val, packstruct *com)
{ /* waarde val >=2 */
	int bits_to_do=first_bit_set32(val)-1;
	uint32 mask=1<<bits_to_do;
	mask>>=1;
	do
	{
		if((val&mask)==0)
		{
			ST_BIT_N9(0);
		}
		else
		{
			ST_BIT_N9(1);
		}
		mask>>=1;
		if(mask==0)
		{
			ST_BIT_N9(1);
		}
		else
		{
			ST_BIT_N9(0);
		}
	}while(mask!=0);
}

void store_n9_len_val(uint32 val, packstruct *com)
{ /* waarde val >=3 */
	store_n9_val(val-1, com);
}

void store_n9_ptr_val(int32_t val, packstruct *com)
{ /* waarde val >=0 <=65535 */
	val++;
	if(val<=256)
	{
		val=-val;
		*com->rbuf_current++ = (uint8) (val&0xff);
		ST_BIT_N9(0);
	}
	else
	{
		val=-val;
		*com->rbuf_current++ = (uint8) ~((val>>8)&0xff);
		ST_BIT_N9(1);
		*com->rbuf_current++ = (uint8) (val&0xff);
	}
}

gup_result compress_n9(packstruct *com)
{
	/*
	** pointer lengte codering:
	** 8 xxxxxxxx0              0 -   255   9 bits
	** 16 xxxxxxxx1xxxxxxxx   256 - 65536  17 bits
	**
	** len codering:
	** 1 x0              :  2 -   3   2 bits
	** 2 x1x0            :  4 -   7   4 bits
	** 3 x1x1x0          :  8 -  15   6 bits
	** 4 x1x1x1x0        : 16 -  31   8 bits
	** 5 x1x1x1x1x0      : 32 -  63  10 bits
	** 6 x1x1x1x1x1x0    : 64 - 127  12 bits
	** 7 x1x1x1x1x1x1x0  :128 - 255  14 bits
	** enz...
	**   
	*/
	index_t current_pos = DICTIONARY_START_OFFSET; /* wijst de te packen byte aan */
	unsigned long bytes_to_do=com->origsize;
	com->rbuf_current=com->compressed_data;
	com->bits_in_bitbuf=0;
	while(bytes_to_do>0)
	{
		match_t match;
		match=com->match_len[current_pos];
		if(match==0)
		{ /* store literal */
			match_t kar;
			ST_BIT_N9(0);
			kar=com->dictionary[current_pos];
			LOG_LITERAL(kar);
			*com->rbuf_current++=(uint8)kar;
			bytes_to_do--;
			current_pos++;
		}
		else
      {
      	ptr_t ptr;
			ST_BIT_N9(1);
			ptr=com->ptr_len[current_pos];
         store_n9_ptr_val(ptr, com);
         store_n9_len_val(match, com);
         LOG_PTR_LEN(kar, ptr+1);
         bytes_to_do-=match;
         current_pos+=match;
		}
	}
	return GUP_OK;
}

gup_result close_n9_stream(packstruct *com)
{
	long bits_comming=10; /* end of archive marker */
	long bytes_extra=0;
	gup_result res;
	if(com->command_byte_ptr!=NULL)
	{ /* command byte pointer is gebruikt, is hij nu in gebruik? */
		if(com->bits_in_bitbuf!=0)
		{ /* er zitten bits in de bitbuf, we mogen wegschrijven tot de command_byte_ptr */
			bytes_extra=com->rbuf_current-com->command_byte_ptr;
			com->rbuf_current=com->command_byte_ptr;
		}
	}
	if((res=announce(bytes_extra+((bits_comming+7)>>3), com))!=GUP_OK)
	{
		return res;
	}
	if(com->command_byte_ptr!=NULL)
	{
		memcpy(com->rbuf_current, com->command_byte_ptr, bytes_extra);
		com->command_byte_ptr=com->rbuf_current;
		com->rbuf_current+=bytes_extra;
   }
   bits_comming+=com->bits_rest;
   com->bits_rest=(int16)(bits_comming&7);
   com->packed_size += bits_comming>>3;
	{ /* schrijf n9 end of stream marker, een speciaal geformateerde ptr */
		ST_BIT_N9(1); /* pointer comming */
		*com->rbuf_current++ = 0; 
		ST_BIT_N9(1); /* deze combi kan niet voorkomen */
	}
	if (com->bits_in_bitbuf>0)
	{
		com->bitbuf=com->bitbuf<<(8-com->bits_in_bitbuf);
		*com->command_byte_ptr=(uint8)com->bitbuf;
		com->bits_in_bitbuf=0;
	}
	com->command_byte_ptr=NULL;
	if(com->bits_rest!=0)
	{
		com->packed_size++; /* corrigeer packed_size */
	}
	return GUP_OK;
}


#define GET_N9_BIT(bit)								\
{ /* get a bit from the data stream */			\
 	if(bits_in_bitbuf==0)							\
 	{ /* fill bitbuf */								\
  		if(com->rbuf_current>com->rbuf_tail)	\
		{													\
			gup_result res;							\
			if((res=read_data(com))!=GUP_OK)		\
			{												\
				return res;								\
			}												\
		}													\
  		bitbuf=*com->rbuf_current++;				\
  		bits_in_bitbuf=8;								\
	}														\
	bit=(bitbuf&0x80)>>7;							\
	bitbuf+=bitbuf;									\
	bits_in_bitbuf--;									\
}

#define DECODE_N9_LEN(val)							\
{ /* get value 2 - 2^32-1 */						\
	int bit;												\
	val=1;												\
	do														\
	{														\
		GET_N9_BIT(bit);								\
		val+=val+bit;									\
		GET_N9_BIT(bit);								\
	} while(bit==0);									\
}


gup_result decode_n9(decode_struct *com)
{
	uint8* dst=com->buffstart;
	uint8* dstend;
	uint8 bitbuf=0;
	int bits_in_bitbuf=0;
	dstend=com->buffstart+65536L;
	if(com->origsize==0)
	{
		return GUP_OK; /* exit succes? */
	}
	{ /* start met een literal */
		com->origsize--;
  		if(com->rbuf_current > com->rbuf_tail)
		{
			gup_result res;
			if((res=read_data(com))!=GUP_OK)
			{
				return res;
			}
		}
		LOG_LITERAL(*com->rbuf_current);
		*dst++=*com->rbuf_current++;
		if(dst>=dstend)
		{
			gup_result err;
			long bytes=dst-com->buffstart;
			com->print_progres(bytes, com->pp_propagator);
			if ((err = com->write_crc(bytes, com->buffstart, com->wc_propagator))!=GUP_OK)
			{
				return err;
			}
			dst-=bytes;
			memmove(com->buffstart-bytes, com->buffstart, bytes);
		}
	}
	for(;;)
	{
		int bit;
		GET_N9_BIT(bit);
		if(bit==0)
		{ /* literal */
	  		if(com->rbuf_current > com->rbuf_tail)
			{
				gup_result res;
				if((res=read_data(com))!=GUP_OK)
				{
					return res;
				}
			}
			LOG_LITERAL(*com->rbuf_current);
			*dst++=*com->rbuf_current++;
			if(dst>=dstend)
			{
				gup_result err;
				long bytes=dst-com->buffstart;
				com->print_progres(bytes, com->pp_propagator);
				if ((err = com->write_crc(bytes, com->buffstart, com->wc_propagator))!=GUP_OK)
				{
					return err;
				}
				dst-=bytes;
				memmove(com->buffstart-bytes, com->buffstart, bytes);
			}
		}
		else
		{ /* ptr len */
			int32 ptr;
			uint8* src;
			uint8 data;
			int len;
			ptr=-1;
			ptr<<=8;
	  		if(com->rbuf_current > com->rbuf_tail)
			{
				gup_result res;
				if((res=read_data(com))!=GUP_OK)
				{
					return res;
				}
			}
			data=*com->rbuf_current++;
			GET_N9_BIT(bit);
			if(bit==0)
			{
				ptr|=data;
			}
			else
			{ /* 16 bit pointer */
				if(data==0)
				{
					break; /* end of stream */
				}
				ptr|=~data;
				ptr<<=8;
		  		if(com->rbuf_current > com->rbuf_tail)
				{
					gup_result res;
					if((res=read_data(com))!=GUP_OK)
					{
						return res;
					}
				}
				ptr|=*com->rbuf_current++;
			}
			DECODE_N9_LEN(len);
			len++;
			LOG_PTR_LEN(len, -ptr)
			src=dst+ptr;
			do
			{
  				*dst++=*src++;
  				if(dst>=dstend)
  				{
					gup_result err;
					long bytes=dst-com->buffstart;
					com->print_progres(bytes, com->pp_propagator);
  					if ((err = com->write_crc(bytes, com->buffstart, com->wc_propagator))!=GUP_OK)
  					{
  						return err;
  					}
					dst-=bytes;
					src-=bytes;
					memmove(com->buffstart-bytes, com->buffstart, bytes);
				}
			} while(--len!=0);
		}
	}
	{
		unsigned long len;
		if((len=(dst-com->buffstart))!=0)
		{
			gup_result err;
			com->print_progres(len, com->pp_propagator);
			if ((err = com->write_crc(len, com->buffstart, com->wc_propagator))!=GUP_OK)
			{
				return err;
			}
		}
	}
	return GUP_OK; /* exit succes */
}
