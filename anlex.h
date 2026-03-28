#ifndef ANLEX_H
#define ANLEX_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* Codigos de componentes lexicos JSON */
#define L_CORCHETE      256
#define R_CORCHETE      257
#define L_LLAVE         258
#define R_LLAVE         259
#define COMA            260
#define DOS_PUNTOS      261
#define LITERAL_CADENA  262
#define LITERAL_NUM     263
#define PR_TRUE         264
#define PR_FALSE        265
#define PR_NULL         266

#define TAMLEX   512
#define TAMBUFF  4096

#endif
