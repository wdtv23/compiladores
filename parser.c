/*
 * Analizador Sintactico Descendente Recursivo - JSON Simplificado
 * Curso: Compiladores y Lenguajes de Bajo Nivel
 * Practica de Programacion Nro. 2
 *
 * Gramatica:
 *   json            => element EOF
 *   element         => object | array
 *   array           => '[' element-list ']' | '[' ']'
 *   element-list    => element { ',' element }
 *   object          => '{' attributes-list '}' | '{' '}'
 *   attributes-list => attribute { ',' attribute }
 *   attribute       => attribute-name ':' attribute-value
 *   attribute-name  => string
 *   attribute-value => element | string | number | true | false | null
 *
 * Reutiliza los codigos de tokens definidos en anlex.h (Tarea 1).
 * El lexer incluido aqui adapta la logica de anlex.c a una API de
 * token-a-token con seguimiento de linea y columna.
 *
 * Uso: ./parser <archivo_fuente.json>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "anlex.h"

/* ================================================================
 * LEXER ADAPTADO (streaming con linea/columna)
 *
 * Reutiliza la logica de anlex.c (mismo DFA para numeros, misma
 * deteccion de cadenas y palabras reservadas), pero en lugar de
 * procesar una linea entera hacia un archivo, expone una funcion
 * lexer_siguiente() que devuelve un token por llamada con su
 * posicion exacta (linea y columna).
 * ================================================================ */

typedef struct {
    int  codigo;          /* codigo del token (de anlex.h) o -1 si error lexico */
    int  linea;           /* linea del primer caracter del token (base 1)       */
    int  columna;         /* columna del primer caracter del token (base 1)     */
    char lexema[TAMLEX];  /* texto exacto reconocido                            */
} Token;

static FILE *src_file  = NULL;
static char  linea_buf[TAMBUFF];
static int   linea_pos = 0;
static int   linea_len = 0;
static int   num_linea = 0;
static int   en_eof    = 0;

/* Errores totales (lexicos + sintacticos) */
static int num_errores = 0;

/*
 * TOKEN_ERROR - Centinela interno para tokens lexicamente invalidos.
 * Distinto de EOF (que vale -1 en <stdio.h>) para que avanzar() no
 * confunda un error lexico con fin de archivo.
 */
#define TOKEN_ERROR (-2)

/* Cargar la siguiente linea del archivo fuente */
static int cargar_linea(void) {
    if (en_eof) return 0;
    if (fgets(linea_buf, TAMBUFF, src_file) == NULL) {
        en_eof    = 1;
        linea_len = 0;
        return 0;
    }
    num_linea++;
    linea_pos = 0;
    linea_len = (int)strlen(linea_buf);
    return 1;
}

/*
 * skip_whitespace - Avanza sobre espacios/tabuladores/saltos de linea.
 * Carga nuevas lineas del archivo segun sea necesario.
 * Retorna 1 si encontro un caracter no-blanco, 0 en EOF del archivo.
 */
static int skip_whitespace(void) {
    while (1) {
        /* Cargar linea si la actual esta agotada */
        while (linea_pos >= linea_len) {
            if (!cargar_linea()) return 0;
        }
        char c = linea_buf[linea_pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            linea_pos++;
        } else {
            return 1;
        }
    }
}

/* Imprimir error lexico y contabilizarlo */
static void error_lexico(int lin, int col, const char *msg) {
    printf("Lin %d, Col %d: Error Lexico. %s.\n", lin, col, msg);
    num_errores++;
}

/*
 * lexer_siguiente - Devuelve el proximo token del archivo fuente.
 *
 * Implementa el mismo automata que anlex.c pero sobre el buffer
 * persistente linea_buf, cruzando saltos de linea cuando es necesario.
 * Los tokens de un solo caracter, LITERAL_CADENA, LITERAL_NUM y las
 * palabras reservadas se reconocen con la misma logica que en anlex.c.
 */
Token lexer_siguiente(void) {
    Token tok;
    char  msg[TAMLEX + 64];

    /* Saltar espacios en blanco */
    if (!skip_whitespace()) {
        tok.codigo  = EOF;
        tok.linea   = num_linea;
        tok.columna = linea_pos + 1;
        strcpy(tok.lexema, "EOF");
        return tok;
    }

    /* Guardar posicion inicial del token */
    tok.linea   = num_linea;
    tok.columna = linea_pos + 1;

    char c = linea_buf[linea_pos];

    /* ---- Tokens de un solo caracter ---- */
    switch (c) {
        case '[': linea_pos++; tok.codigo = L_CORCHETE; strcpy(tok.lexema,"["); return tok;
        case ']': linea_pos++; tok.codigo = R_CORCHETE; strcpy(tok.lexema,"]"); return tok;
        case '{': linea_pos++; tok.codigo = L_LLAVE;    strcpy(tok.lexema,"{"); return tok;
        case '}': linea_pos++; tok.codigo = R_LLAVE;    strcpy(tok.lexema,"}"); return tok;
        case ',': linea_pos++; tok.codigo = COMA;       strcpy(tok.lexema,","); return tok;
        case ':': linea_pos++; tok.codigo = DOS_PUNTOS; strcpy(tok.lexema,":"); return tok;
    }

    /* ---- LITERAL_CADENA: comienza con '"' ---- */
    if (c == '"') {
        int ini = linea_pos;
        linea_pos++; /* consumir '"' inicial */
        while (1) {
            if (linea_pos >= linea_len ||
                linea_buf[linea_pos] == '\0' ||
                linea_buf[linea_pos] == '\n' ||
                linea_buf[linea_pos] == '\r') {
                error_lexico(tok.linea, tok.columna, "Cadena sin cerrar");
                tok.codigo = TOKEN_ERROR;
                int len = linea_pos - ini;
                if (len >= TAMLEX) len = TAMLEX - 1;
                strncpy(tok.lexema, linea_buf + ini, len);
                tok.lexema[len] = '\0';
                return tok;
            }
            if (linea_buf[linea_pos] == '"') {
                linea_pos++; /* consumir '"' de cierre */
                break;
            }
            if (linea_buf[linea_pos] == '\\') {
                linea_pos++; /* saltar '\' */
                if (linea_pos >= linea_len ||
                    linea_buf[linea_pos] == '\0' ||
                    linea_buf[linea_pos] == '\n' ||
                    linea_buf[linea_pos] == '\r') {
                    error_lexico(tok.linea, tok.columna, "Secuencia de escape incompleta");
                    tok.codigo = TOKEN_ERROR;
                    return tok;
                }
            }
            linea_pos++;
        }
        int len = linea_pos - ini;
        if (len >= TAMLEX) len = TAMLEX - 1;
        strncpy(tok.lexema, linea_buf + ini, len);
        tok.lexema[len] = '\0';
        tok.codigo = LITERAL_CADENA;
        return tok;
    }

    /* ---- LITERAL_NUM: digito o '-' seguido de digito ---- */
    if (isdigit(c) || (c == '-' && isdigit(linea_buf[linea_pos + 1]))) {
        int ini    = linea_pos;
        int estado = 0;
        int acepto = 0;
        tok.codigo = LITERAL_NUM;

        if (c == '-') linea_pos++; /* consumir signo negativo */

        /* Mismo DFA de 7 estados que anlex.c::parsearNumero */
        while (!acepto) {
            char ch = linea_buf[linea_pos];
            switch (estado) {
                case 0: /* parte entera */
                    if (isdigit(ch))            linea_pos++;
                    else if (ch == '.')        { linea_pos++; estado = 1; }
                    else if (ch=='e'||ch=='E') { linea_pos++; estado = 3; }
                    else                         estado = 6;
                    break;
                case 1: /* despues del punto: obligatorio un digito */
                    if (isdigit(ch)) { linea_pos++; estado = 2; }
                    else {
                        sprintf(msg, "Se esperaba digito despues del punto, se encontro '%c'", ch);
                        error_lexico(tok.linea, tok.columna, msg);
                        tok.codigo = TOKEN_ERROR; acepto = 1;
                    }
                    break;
                case 2: /* parte decimal */
                    if (isdigit(ch))            linea_pos++;
                    else if (ch=='e'||ch=='E') { linea_pos++; estado = 3; }
                    else                         estado = 6;
                    break;
                case 3: /* despues de 'e'/'E' */
                    if (ch=='+' || ch=='-')    { linea_pos++; estado = 4; }
                    else if (isdigit(ch))       { linea_pos++; estado = 5; }
                    else {
                        sprintf(msg, "Se esperaba digito o signo en exponente, se encontro '%c'", ch);
                        error_lexico(tok.linea, tok.columna, msg);
                        tok.codigo = TOKEN_ERROR; acepto = 1;
                    }
                    break;
                case 4: /* despues del signo del exponente: obligatorio un digito */
                    if (isdigit(ch)) { linea_pos++; estado = 5; }
                    else {
                        sprintf(msg, "Se esperaba digito en exponente, se encontro '%c'", ch);
                        error_lexico(tok.linea, tok.columna, msg);
                        tok.codigo = TOKEN_ERROR; acepto = 1;
                    }
                    break;
                case 5: /* digitos del exponente */
                    if (isdigit(ch)) linea_pos++;
                    else             estado = 6;
                    break;
                case 6: /* ACEPTACION */
                    acepto = 1;
                    break;
            }
        }
        int len = linea_pos - ini;
        if (len >= TAMLEX) len = TAMLEX - 1;
        strncpy(tok.lexema, linea_buf + ini, len);
        tok.lexema[len] = '\0';
        return tok;
    }

    /* ---- Palabras reservadas: true, false, null ---- */
    if (isalpha((unsigned char)c)) {
        char palabra[TAMLEX];
        int  i = 0;
        while (isalpha((unsigned char)linea_buf[linea_pos]) && i < TAMLEX - 1)
            palabra[i++] = linea_buf[linea_pos++];
        palabra[i] = '\0';
        strncpy(tok.lexema, palabra, sizeof(tok.lexema) - 1);
        tok.lexema[sizeof(tok.lexema) - 1] = '\0';

        if (strcmp(palabra,"true") ==0 || strcmp(palabra,"TRUE") ==0) { tok.codigo = PR_TRUE;  return tok; }
        if (strcmp(palabra,"false")==0 || strcmp(palabra,"FALSE")==0) { tok.codigo = PR_FALSE; return tok; }
        if (strcmp(palabra,"null") ==0 || strcmp(palabra,"NULL") ==0) { tok.codigo = PR_NULL;  return tok; }

        sprintf(msg, "Palabra no reconocida '%s'", palabra);
        error_lexico(tok.linea, tok.columna, msg);
        tok.codigo = TOKEN_ERROR;
        return tok;
    }

    /* ---- Caracter no reconocido ---- */
    sprintf(msg, "Caracter no reconocido '%c'", c);
    error_lexico(tok.linea, tok.columna, msg);
    linea_pos++;
    tok.codigo = TOKEN_ERROR;
    tok.lexema[0] = c; tok.lexema[1] = '\0';
    return tok;
}

/* ================================================================
 * ANALIZADOR SINTACTICO DESCENDENTE RECURSIVO
 *
 * Una funcion por no-terminal.  El lookahead es tok_actual.
 * Manejo de errores: Panic Mode con conjuntos FOLLOW para sincronizar.
 * ================================================================ */

static Token tok_actual; /* lookahead */

/* Nombre legible de un token para mensajes de error */
static const char *nombre_token(int codigo) {
    switch (codigo) {
        case L_CORCHETE:     return "'['";
        case R_CORCHETE:     return "']'";
        case L_LLAVE:        return "'{'";
        case R_LLAVE:        return "'}'";
        case COMA:           return "','";
        case DOS_PUNTOS:     return "':'";
        case LITERAL_CADENA: return "cadena";
        case LITERAL_NUM:    return "numero";
        case PR_TRUE:        return "true";
        case PR_FALSE:       return "false";
        case PR_NULL:        return "null";
        case EOF:            return "fin de archivo";
        default:             return "token desconocido";
    }
}

/*
 * avanzar - Pide el siguiente token al lexer, saltando tokens invalidos (-1).
 * Los tokens invalidos ya fueron reportados por error_lexico().
 */
static void avanzar(void) {
    do {
        tok_actual = lexer_siguiente();
    } while (tok_actual.codigo == TOKEN_ERROR);
}

/* Reportar error sintactico */
static void error_sintactico(const char *esperado) {
    printf("Lin %d, Col %d: Error Sintactico. Se esperaba %s pero se encontro %s ('%s').\n",
           tok_actual.linea, tok_actual.columna,
           esperado,
           nombre_token(tok_actual.codigo),
           tok_actual.lexema);
    num_errores++;
}

/*
 * sincronizar - Modo panico: descarta tokens hasta encontrar uno
 * que pertenezca al conjunto de sincronizacion (FOLLOW del no-terminal
 * en error). Siempre se detiene en EOF para evitar bucles infinitos.
 */
static void sincronizar(const int *follow, int n) {
    while (tok_actual.codigo != EOF) {
        for (int i = 0; i < n; i++) {
            if (tok_actual.codigo == follow[i]) return;
        }
        avanzar();
    }
}

/*
 * consumir - Verifica que tok_actual sea el token esperado y avanza.
 * Si no coincide, reporta error, sincroniza al conjunto follow y
 * retorna 0 para que el llamador pueda abortar la produccion.
 */
static int consumir(int esperado, const int *follow, int n) {
    if (tok_actual.codigo == esperado) {
        avanzar();
        return 1;
    }
    error_sintactico(nombre_token(esperado));
    sincronizar(follow, n);
    /* Si la sincronizacion aterrizo justo en el token esperado, consumirlo
     * para evitar que el llamador lo vea y genere un error en cascada.    */
    if (tok_actual.codigo == esperado) avanzar();
    return 0;
}

/* ---- Forward declarations ---- */
static void parse_element(void);
static void parse_element_list(void);
static void parse_object(void);
static void parse_array(void);
static void parse_attributes_list(void);
static void parse_attribute(void);
static void parse_attribute_name(void);
static void parse_attribute_value(void);

/*
 * Conjuntos FOLLOW reutilizados en multiples no-terminales.
 *
 * FOLLOW(element) = { EOF, ',', ']', '}' }
 *   - EOF  : aparece como raiz del JSON
 *   - ','  : dentro de element-list o attributes-list
 *   - ']'  : fin de array
 *   - '}'  : fin de objeto
 */
static const int FOLLOW_ELEMENT[] = { EOF, COMA, R_CORCHETE, R_LLAVE };
#define N_FOLLOW_ELEMENT 4

/* FOLLOW(attribute) = FOLLOW(attribute-value) = { ',', '}' } */
static const int FOLLOW_ATTRIBUTE[] = { COMA, R_LLAVE };
#define N_FOLLOW_ATTRIBUTE 2

/*
 * parse_element => object | array
 *
 * FIRST = { '{', '[' }
 */
static void parse_element(void) {
    if (tok_actual.codigo == L_LLAVE) {
        parse_object();
    } else if (tok_actual.codigo == L_CORCHETE) {
        parse_array();
    } else {
        error_sintactico("'{' o '['");
        sincronizar(FOLLOW_ELEMENT, N_FOLLOW_ELEMENT);
    }
}

/*
 * parse_array => '[' element-list ']'  |  '[' ']'
 *
 * FOLLOW = FOLLOW(element)
 */
static void parse_array(void) {
    if (!consumir(L_CORCHETE, FOLLOW_ELEMENT, N_FOLLOW_ELEMENT)) return;

    if (tok_actual.codigo == R_CORCHETE) {
        /* array vacio: '[' ']' */
        avanzar();
        return;
    }

    parse_element_list();
    consumir(R_CORCHETE, FOLLOW_ELEMENT, N_FOLLOW_ELEMENT);
}

/*
 * parse_element_list => element { ',' element }
 *   (izquierda recursiva eliminada; equivale a la produccion original)
 *
 * FOLLOW = { ']' }
 */
static void parse_element_list(void) {
    parse_element();
    while (tok_actual.codigo == COMA) {
        avanzar(); /* consumir ',' */
        parse_element();
    }
}

/*
 * parse_object => '{' attributes-list '}'  |  '{' '}'
 *
 * FOLLOW = FOLLOW(element)
 */
static void parse_object(void) {
    if (!consumir(L_LLAVE, FOLLOW_ELEMENT, N_FOLLOW_ELEMENT)) return;

    if (tok_actual.codigo == R_LLAVE) {
        /* objeto vacio: '{' '}' */
        avanzar();
        return;
    }

    parse_attributes_list();
    consumir(R_LLAVE, FOLLOW_ELEMENT, N_FOLLOW_ELEMENT);
}

/*
 * parse_attributes_list => attribute { ',' attribute }
 *   (izquierda recursiva eliminada)
 *
 * FOLLOW = { '}' }
 */
static void parse_attributes_list(void) {
    parse_attribute();
    while (tok_actual.codigo == COMA) {
        avanzar(); /* consumir ',' */
        parse_attribute();
    }
}

/*
 * parse_attribute => attribute-name ':' attribute-value
 *
 * FOLLOW = { ',', '}' }
 */
static void parse_attribute(void) {
    parse_attribute_name();
    if (!consumir(DOS_PUNTOS, FOLLOW_ATTRIBUTE, N_FOLLOW_ATTRIBUTE)) return;
    parse_attribute_value();
}

/*
 * parse_attribute_name => string
 *
 * FOLLOW = { ':' }
 */
static void parse_attribute_name(void) {
    static const int follow_name[] = { DOS_PUNTOS };

    if (tok_actual.codigo == LITERAL_CADENA) {
        avanzar();
    } else {
        error_sintactico("nombre de atributo (cadena entre comillas)");
        sincronizar(follow_name, 1);
    }
}

/*
 * parse_attribute_value => element | string | number | true | false | null
 *
 * FIRST = { '{', '[', cadena, numero, true, false, null }
 * FOLLOW = { ',', '}' }
 */
static void parse_attribute_value(void) {
    switch (tok_actual.codigo) {
        case L_LLAVE:
        case L_CORCHETE:
            parse_element();
            break;
        case LITERAL_CADENA:
        case LITERAL_NUM:
        case PR_TRUE:
        case PR_FALSE:
        case PR_NULL:
            avanzar();
            break;
        default:
            error_sintactico("valor de atributo (objeto, array, cadena, numero, true, false o null)");
            sincronizar(FOLLOW_ATTRIBUTE, N_FOLLOW_ATTRIBUTE);
    }
}

/*
 * parse_json => element EOF
 */
static void parse_json(void) {
    parse_element();
    if (tok_actual.codigo != EOF) {
        error_sintactico("fin de archivo");
    }
}

/* ================================================================
 * MAIN
 * ================================================================ */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s <archivo_fuente.json>\n", argv[0]);
        return 1;
    }

    src_file = fopen(argv[1], "r");
    if (!src_file) {
        printf("No se puede abrir el archivo: %s\n", argv[1]);
        return 1;
    }

    /* Inicializar lookahead */
    avanzar();

    /* Analizar */
    parse_json();

    fclose(src_file);

    if (num_errores == 0) {
        printf("Analisis sintactico exitoso.\n");
        return 0;
    } else {
        printf("Se encontraron %d error(es).\n", num_errores);
        return 1;
    }
}
