// cria um MPQ e adiciona arquivos: mkpatch <saida.mpq> <arquivo_local> <nome_interno> [...]
#include <stdio.h>
#include <string.h>
#include "StormLib.h"

int main(int argc, char** argv)
{
    if (argc < 4 || (argc % 2) != 0) { printf("uso: mkpatch saida.mpq local interno [local interno ...]\n"); return 1; }

    HANDLE mpq = NULL;
    int nfiles = (argc - 2) / 2;

    if (!SFileCreateArchive(argv[1], MPQ_CREATE_ARCHIVE_V2 | MPQ_CREATE_LISTFILE, nfiles + 8, &mpq)) {
        printf("erro criando MPQ: %d\n", SErrGetLastError()); return 2;
    }

    for (int i = 2; i + 1 < argc; i += 2) {
        if (!SFileAddFileEx(mpq, argv[i], argv[i+1],
                MPQ_FILE_COMPRESS | MPQ_FILE_REPLACEEXISTING,
                MPQ_COMPRESSION_ZLIB, MPQ_COMPRESSION_ZLIB)) {
            printf("erro adicionando %s: %d\n", argv[i], SErrGetLastError());
            SFileCloseArchive(mpq); return 3;
        }
        printf("adicionado: %s -> %s\n", argv[i], argv[i+1]);
    }

    SFileCloseArchive(mpq);
    printf("pronto: %s\n", argv[1]);
    return 0;
}
