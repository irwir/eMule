#pragma once

#define	MP_IMPORTPARTS			13000	/* Rowaa[SR13]: Import parts to file */

struct ImportPart_Struct
{
	uint64	start;
	uint64	end;
	BYTE	*data;
};