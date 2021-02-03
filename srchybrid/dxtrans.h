//На сайте вместо правки непосредственно файла qedit.h рекомендуют в рабочем каталоге проекта создать файл dxtrans.h и прописать в него следующие строки:
#define __IDxtCompositor_INTERFACE_DEFINED__
#define __IDxtAlphaSetter_INTERFACE_DEFINED__
#define __IDxtJpeg_INTERFACE_DEFINED__
#define __IDxtKey_INTERFACE_DEFINED__

/*
 * http://jaewon.mine.nu/jaewon/2009/06/17/a-workaround-for-a-missing-file-dxtrans-h-in-directx-sdk/
 * #pragma include_alias( "dxtrans.h", "qedit.h" )
 * #define __IDxtCompositor_INTERFACE_DEFINED__
 * #define __IDxtAlphaSetter_INTERFACE_DEFINED__
 * #define __IDxtJpeg_INTERFACE_DEFINED__
 * #define __IDxtKey_INTERFACE_DEFINED__
 * #include <qedit.h>
*/