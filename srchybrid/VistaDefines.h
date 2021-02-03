// Vista defines from the Vista SDK, to make eMule compile without having the Vista SDK installed only for some #defines

#pragma once

#ifdef DEFINE_KNOWN_FOLDER
#undef DEFINE_KNOWN_FOLDER
#endif

typedef GUID KNOWNFOLDERID;
#define REFKNOWNFOLDERID const KNOWNFOLDERID &

#define DEFINE_KNOWN_FOLDER(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
		EXTERN_C const GUID DECLSPEC_SELECTANY name \
				= { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

// {62AB5D82-FDC1-4DC3-A9DD-070D1D495D97}
DEFINE_KNOWN_FOLDER(FOLDERID_ProgramData,		0x62AB5D82, 0xFDC1, 0x4DC3, 0xA9, 0xDD, 0x07, 0x0D, 0x1D, 0x49, 0x5D, 0x97);

// {F1B32785-6FBA-4FCF-9D55-7B8E7F157091}
DEFINE_KNOWN_FOLDER(FOLDERID_LocalAppData,		0xF1B32785, 0x6FBA, 0x4FCF, 0x9D, 0x55, 0x7B, 0x8E, 0x7F, 0x15, 0x70, 0x91);

// {374DE290-123F-4565-9164-39C4925E467B}
DEFINE_KNOWN_FOLDER(FOLDERID_Downloads,			0x374de290, 0x123f, 0x4565, 0x91, 0x64, 0x39, 0xc4, 0x92, 0x5e, 0x46, 0x7b);

// {3D644C9B-1FB8-4f30-9B45-F670235F79C0}
DEFINE_KNOWN_FOLDER(FOLDERID_PublicDownloads,	0x3d644c9b, 0x1fb8, 0x4f30, 0x9b, 0x45, 0xf6, 0x70, 0x23, 0x5f, 0x79, 0xc0);