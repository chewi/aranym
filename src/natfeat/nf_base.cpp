/**
 * Host<->Atari mem & str functions
 **/
#include "cpu_emulation.h"
#include "nf_base.h"

void NF_Base::a2fmemcpy( char *dest, memptr source, size_t count )
{
	while ( count-- )
		*dest++ = (char)ReadInt8( (uint32)source++ );
}

void NF_Base::a2fstrcpy( char *dest, memptr source )
{
	while ( (*dest++ = (char)ReadInt8( (uint32)source++ )) != 0 );
}


void NF_Base::f2amemcpy( memptr dest, char *source, size_t count )
{
	while ( count-- )
		WriteInt8( dest++, (uint8)*source++ );
}

void NF_Base::f2astrcpy( memptr dest, char *source )
{
	while ( *source )
		WriteInt8( dest++, (uint8)*source++ );
	WriteInt8( dest, 0 );
}
