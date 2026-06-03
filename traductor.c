/*
 * Traductor JSON → XML (Tarea 3)
 * Curso: Compiladores y Lenguajes de Bajo Nivel
 *
 * Implementa un Traductor Dirigido por Sintaxis (TDS) que convierte
 * JSON simplificado a XML. Reutiliza el lexico y el parser descendente
 * recursivo de la Tarea 2 sin modificarlos.
 *
 * Estrategia de dos pases:
 *   Pase 1 — analisis sintactico puro (deteccion de errores, sin emitir XML)
 *   Pase 2 — si no hubo errores, se relanza el lexer y se ejecutan las
 *             funciones trad_* que integran las acciones semanticas de
 *             traduccion directamente en cada regla gramatical.
 *
 * Uso: ./traductor <fuente.json> <output.xml>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "anlex.h"

/* ================================================================
 * LEXER — identico al de parser.c (Tarea 2)
 *
 * Se adapta la logica de anlex.c a una API token-a-token con
 * seguimiento de linea y columna. Se agrega lexer_reinit() para
 * poder reiniciar el estado entre el pase 1 y el pase 2.
 * ================================================================ */

typedef struct {
    int  codigo;
    int  linea;
    int  columna;
    char lexema[TAMLEX];
} Token;

static FILE *src_file  = NULL;
static char  linea_buf[TAMBUFF];
static int   linea_pos = 0;
static int   linea_len = 0;
static int   num_linea = 0;
static int   en_eof    = 0;

static int num_errores = 0;

#define TOKEN_ERROR (-2)

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

static int skip_whitespace(void) {
    while (1) {
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

static void error_lexico(int lin, int col, const char *msg) {
    printf("Lin %d, Col %d: Error Lexico. %s.\n", lin, col, msg);
    num_errores++;
}

static Token lexer_siguiente(void) {
    Token tok;
    char  msg[TAMLEX + 64];

    if (!skip_whitespace()) {
        tok.codigo  = EOF;
        tok.linea   = num_linea;
        tok.columna = linea_pos + 1;
        strcpy(tok.lexema, "EOF");
        return tok;
    }

    tok.linea   = num_linea;
    tok.columna = linea_pos + 1;
    char c      = linea_buf[linea_pos];

    switch (c) {
        case '[': linea_pos++; tok.codigo = L_CORCHETE; strcpy(tok.lexema,"["); return tok;
        case ']': linea_pos++; tok.codigo = R_CORCHETE; strcpy(tok.lexema,"]"); return tok;
        case '{': linea_pos++; tok.codigo = L_LLAVE;    strcpy(tok.lexema,"{"); return tok;
        case '}': linea_pos++; tok.codigo = R_LLAVE;    strcpy(tok.lexema,"}"); return tok;
        case ',': linea_pos++; tok.codigo = COMA;       strcpy(tok.lexema,","); return tok;
        case ':': linea_pos++; tok.codigo = DOS_PUNTOS; strcpy(tok.lexema,":"); return tok;
    }

    /* LITERAL_CADENA */
    if (c == '"') {
        int ini = linea_pos;
        linea_pos++;
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
                linea_pos++;
                break;
            }
            if (linea_buf[linea_pos] == '\\') {
                linea_pos++;
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

    /* LITERAL_NUM — mismo DFA de 7 estados que anlex.c */
    if (isdigit(c) || (c == '-' && isdigit(linea_buf[linea_pos + 1]))) {
        int ini    = linea_pos;
        int estado = 0;
        int acepto = 0;
        tok.codigo = LITERAL_NUM;

        if (c == '-') linea_pos++;

        while (!acepto) {
            char ch = linea_buf[linea_pos];
            switch (estado) {
                case 0:
                    if (isdigit(ch))            linea_pos++;
                    else if (ch == '.')        { linea_pos++; estado = 1; }
                    else if (ch=='e'||ch=='E') { linea_pos++; estado = 3; }
                    else                         estado = 6;
                    break;
                case 1:
                    if (isdigit(ch)) { linea_pos++; estado = 2; }
                    else {
                        sprintf(msg, "Se esperaba digito despues del punto, se encontro '%c'", ch);
                        error_lexico(tok.linea, tok.columna, msg);
                        tok.codigo = TOKEN_ERROR; acepto = 1;
                    }
                    break;
                case 2:
                    if (isdigit(ch))            linea_pos++;
                    else if (ch=='e'||ch=='E') { linea_pos++; estado = 3; }
                    else                         estado = 6;
                    break;
                case 3:
                    if (ch=='+' || ch=='-')    { linea_pos++; estado = 4; }
                    else if (isdigit(ch))       { linea_pos++; estado = 5; }
                    else {
                        sprintf(msg, "Se esperaba digito o signo en exponente, se encontro '%c'", ch);
                        error_lexico(tok.linea, tok.columna, msg);
                        tok.codigo = TOKEN_ERROR; acepto = 1;
                    }
                    break;
                case 4:
                    if (isdigit(ch)) { linea_pos++; estado = 5; }
                    else {
                        sprintf(msg, "Se esperaba digito en exponente, se encontro '%c'", ch);
                        error_lexico(tok.linea, tok.columna, msg);
                        tok.codigo = TOKEN_ERROR; acepto = 1;
                    }
                    break;
                case 5:
                    if (isdigit(ch)) linea_pos++;
                    else             estado = 6;
                    break;
                case 6:
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

    /* Palabras reservadas: true, false, null */
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

    sprintf(msg, "Caracter no reconocido '%c'", c);
    error_lexico(tok.linea, tok.columna, msg);
    linea_pos++;
    tok.codigo = TOKEN_ERROR;
    tok.lexema[0] = c; tok.lexema[1] = '\0';
    return tok;
}

/* Reinicia el estado del lexer para el pase 2 (el archivo ya fue reabierto) */
static void lexer_reinit(void) {
    linea_pos = 0;
    linea_len = 0;
    num_linea = 0;
    en_eof    = 0;
    linea_buf[0] = '\0';
}

/* ================================================================
 * INFRAESTRUCTURA DEL PARSER (compartida entre pase 1 y pase 2)
 * ================================================================ */

static Token tok_actual;

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

static void avanzar(void) {
    do {
        tok_actual = lexer_siguiente();
    } while (tok_actual.codigo == TOKEN_ERROR);
}

static void error_sintactico(const char *esperado) {
    printf("Lin %d, Col %d: Error Sintactico. Se esperaba %s pero se encontro %s ('%s').\n",
           tok_actual.linea, tok_actual.columna,
           esperado,
           nombre_token(tok_actual.codigo),
           tok_actual.lexema);
    num_errores++;
}

static void sincronizar(const int *follow, int n) {
    while (tok_actual.codigo != EOF) {
        for (int i = 0; i < n; i++) {
            if (tok_actual.codigo == follow[i]) return;
        }
        avanzar();
    }
}

static int consumir(int esperado, const int *follow, int n) {
    if (tok_actual.codigo == esperado) {
        avanzar();
        return 1;
    }
    error_sintactico(nombre_token(esperado));
    sincronizar(follow, n);
    if (tok_actual.codigo == esperado) avanzar();
    return 0;
}

/* Conjuntos FOLLOW reutilizados */
static const int FOLLOW_ELEMENT[]   = { EOF, COMA, R_CORCHETE, R_LLAVE };
#define N_FOLLOW_ELEMENT 4

static const int FOLLOW_ATTRIBUTE[] = { COMA, R_LLAVE };
#define N_FOLLOW_ATTRIBUTE 2

/* ================================================================
 * PASE 1 — Analizador sintactico puro (sin emision de XML)
 *
 * Identico al parser de la Tarea 2. Detecta todos los errores
 * lexicos y sintacticos antes de generar cualquier salida.
 * ================================================================ */

static void parse_element(void);
static void parse_element_list(void);
static void parse_object(void);
static void parse_array(void);
static void parse_attributes_list(void);
static void parse_attribute(void);
static void parse_attribute_name(void);
static void parse_attribute_value(void);

static void parse_element(void) {
    if (tok_actual.codigo == L_LLAVE) {
        parse_object();
    } else if (tok_actual.codigo == L_CORCHETE) {
        parse_array();
    } else if (tok_actual.codigo == LITERAL_CADENA ||
               tok_actual.codigo == LITERAL_NUM    ||
               tok_actual.codigo == PR_TRUE        ||
               tok_actual.codigo == PR_FALSE       ||
               tok_actual.codigo == PR_NULL) {
        avanzar();
    } else {
        error_sintactico("'{', '[', cadena, numero, true, false o null");
        sincronizar(FOLLOW_ELEMENT, N_FOLLOW_ELEMENT);
    }
}

static void parse_array(void) {
    if (!consumir(L_CORCHETE, FOLLOW_ELEMENT, N_FOLLOW_ELEMENT)) return;
    if (tok_actual.codigo == R_CORCHETE) { avanzar(); return; }
    parse_element_list();
    consumir(R_CORCHETE, FOLLOW_ELEMENT, N_FOLLOW_ELEMENT);
}

static void parse_element_list(void) {
    parse_element();
    while (tok_actual.codigo == COMA) {
        avanzar();
        parse_element();
    }
}

static void parse_object(void) {
    if (!consumir(L_LLAVE, FOLLOW_ELEMENT, N_FOLLOW_ELEMENT)) return;
    if (tok_actual.codigo == R_LLAVE) { avanzar(); return; }
    parse_attributes_list();
    consumir(R_LLAVE, FOLLOW_ELEMENT, N_FOLLOW_ELEMENT);
}

static void parse_attributes_list(void) {
    parse_attribute();
    while (tok_actual.codigo == COMA) {
        avanzar();
        parse_attribute();
    }
}

static void parse_attribute(void) {
    parse_attribute_name();
    if (!consumir(DOS_PUNTOS, FOLLOW_ATTRIBUTE, N_FOLLOW_ATTRIBUTE)) return;
    parse_attribute_value();
}

static void parse_attribute_name(void) {
    static const int follow_name[] = { DOS_PUNTOS };
    if (tok_actual.codigo == LITERAL_CADENA) {
        avanzar();
    } else {
        error_sintactico("nombre de atributo (cadena entre comillas)");
        sincronizar(follow_name, 1);
    }
}

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

static void parse_json(void) {
    parse_element();
    if (tok_actual.codigo != EOF) error_sintactico("fin de archivo");
}

/* ================================================================
 * PASE 2 — Traductor Dirigido por Sintaxis (TDS)
 *
 * Cada funcion trad_* replica la regla gramatical correspondiente
 * e intercala acciones semanticas que emiten el XML equivalente.
 *
 * Reglas de traduccion:
 *   objeto raiz          => sus atributos se emiten directamente (sin wrapper)
 *   array raiz           => cada elemento envuelto en <item>
 *   atributo primitivo   => <nombre>valor</nombre>
 *   atributo objeto      => <nombre>\n[attrs]\n</nombre>
 *   atributo array vacio => <nombre></nombre>
 *   atributo array lleno => <nombre>\n[<item>...\n</item>]\n</nombre>
 *   elemento array obj.  => <item>\n[attrs]\n</item>
 *   elemento array prim. => <item>valor</item>
 *
 * Los strings conservan las comillas dobles tal como las devuelve el lexer.
 * La indentacion usa un tabulador por nivel de anidamiento.
 * ================================================================ */

static FILE *fout_xml  = NULL;
static int   xml_indent = 0;

static void emit_tabs(void) {
    for (int i = 0; i < xml_indent; i++) fputc('\t', fout_xml);
}

/* Forward declarations del traductor */
static void trad_attributes_list(void);
static void trad_element_list_as_items(void);
static void trad_element_as_item(void);

/*
 * trad_attribute — regla: attribute => attribute-name ':' attribute-value
 *
 * Accion semantica:
 *   Lee el nombre (sin comillas), inspecciona tok_actual tras el ':' para
 *   decidir el formato de salida segun el tipo de valor, y emite el XML.
 */
static void trad_attribute(void) {
    /* --- attribute-name: extraer nombre sin comillas --- */
    char name[TAMLEX] = "";

    if (tok_actual.codigo == LITERAL_CADENA) {
        const char *lex = tok_actual.lexema;
        int len = (int)strlen(lex);
        /* El lexema incluye las comillas delimitadoras: "nombre" */
        if (len >= 2 && lex[0] == '"' && lex[len - 1] == '"') {
            int copy = len - 2;
            if (copy >= TAMLEX) copy = TAMLEX - 1;
            strncpy(name, lex + 1, copy);
            name[copy] = '\0';
        } else {
            strncpy(name, lex, TAMLEX - 1);
            name[TAMLEX - 1] = '\0';
        }
        avanzar();
    } else {
        static const int fol[] = { DOS_PUNTOS };
        error_sintactico("nombre de atributo (cadena entre comillas)");
        sincronizar(fol, 1);
    }

    /* --- Consumir ':' --- */
    if (!consumir(DOS_PUNTOS, FOLLOW_ATTRIBUTE, N_FOLLOW_ATTRIBUTE)) return;

    /* --- attribute-value con accion semantica segun tipo --- */

    if (tok_actual.codigo == L_LLAVE) {
        /*
         * Valor es un objeto: { attrs }
         * Emite:
         *   <name>
         *       <attr1>...</attr1>
         *   </name>
         */
        emit_tabs();
        fprintf(fout_xml, "<%s>\n", name);
        xml_indent++;
        avanzar(); /* consumir '{' */
        if (tok_actual.codigo != R_LLAVE) {
            trad_attributes_list();
        }
        consumir(R_LLAVE, FOLLOW_ATTRIBUTE, N_FOLLOW_ATTRIBUTE);
        xml_indent--;
        emit_tabs();
        fprintf(fout_xml, "</%s>\n", name);

    } else if (tok_actual.codigo == L_CORCHETE) {
        /*
         * Valor es un array: [ ... ]
         * Array vacio  => <name></name>
         * Array lleno  => <name>\n<item>...\n</item>\n</name>
         */
        avanzar(); /* consumir '[' */
        if (tok_actual.codigo == R_CORCHETE) {
            /* Array vacio */
            emit_tabs();
            fprintf(fout_xml, "<%s></%s>\n", name, name);
            avanzar(); /* consumir ']' */
        } else {
            /* Array con elementos */
            emit_tabs();
            fprintf(fout_xml, "<%s>\n", name);
            xml_indent++;
            trad_element_list_as_items();
            consumir(R_CORCHETE, FOLLOW_ATTRIBUTE, N_FOLLOW_ATTRIBUTE);
            xml_indent--;
            emit_tabs();
            fprintf(fout_xml, "</%s>\n", name);
        }

    } else {
        /*
         * Valor primitivo: string, number, true, false, null
         * Emite: <name>lexema</name>
         * Los strings mantienen sus comillas tal como las devuelve el lexer.
         */
        switch (tok_actual.codigo) {
            case LITERAL_CADENA:
            case LITERAL_NUM:
            case PR_TRUE:
            case PR_FALSE:
            case PR_NULL:
                emit_tabs();
                fprintf(fout_xml, "<%s>%s</%s>\n", name, tok_actual.lexema, name);
                avanzar();
                break;
            default:
                error_sintactico("valor de atributo");
                sincronizar(FOLLOW_ATTRIBUTE, N_FOLLOW_ATTRIBUTE);
        }
    }
}

/*
 * trad_attributes_list — regla: attributes-list => attribute { ',' attribute }
 */
static void trad_attributes_list(void) {
    trad_attribute();
    while (tok_actual.codigo == COMA) {
        avanzar(); /* consumir ',' */
        trad_attribute();
    }
}

/*
 * trad_element_as_item — traduce un elemento de array envolviendolo en <item>.
 *
 * Objeto    => <item>\n[attrs]\n</item>
 * Array     => <item>\n[items]\n</item>
 * Primitivo => <item>lexema</item>
 */
static void trad_element_as_item(void) {
    if (tok_actual.codigo == L_LLAVE) {
        emit_tabs();
        fprintf(fout_xml, "<item>\n");
        xml_indent++;
        avanzar(); /* consumir '{' */
        if (tok_actual.codigo != R_LLAVE) {
            trad_attributes_list();
        }
        consumir(R_LLAVE, FOLLOW_ELEMENT, N_FOLLOW_ELEMENT);
        xml_indent--;
        emit_tabs();
        fprintf(fout_xml, "</item>\n");

    } else if (tok_actual.codigo == L_CORCHETE) {
        emit_tabs();
        fprintf(fout_xml, "<item>\n");
        xml_indent++;
        avanzar(); /* consumir '[' */
        if (tok_actual.codigo != R_CORCHETE) {
            trad_element_list_as_items();
        }
        consumir(R_CORCHETE, FOLLOW_ELEMENT, N_FOLLOW_ELEMENT);
        xml_indent--;
        emit_tabs();
        fprintf(fout_xml, "</item>\n");

    } else {
        /* Primitivo */
        emit_tabs();
        switch (tok_actual.codigo) {
            case LITERAL_CADENA:
            case LITERAL_NUM:
            case PR_TRUE:
            case PR_FALSE:
            case PR_NULL:
                fprintf(fout_xml, "<item>%s</item>\n", tok_actual.lexema);
                avanzar();
                break;
            default:
                error_sintactico("elemento del array");
                sincronizar(FOLLOW_ELEMENT, N_FOLLOW_ELEMENT);
        }
    }
}

/*
 * trad_element_list_as_items — regla: element-list => element { ',' element }
 * Cada element se traduce como un <item>.
 */
static void trad_element_list_as_items(void) {
    trad_element_as_item();
    while (tok_actual.codigo == COMA) {
        avanzar(); /* consumir ',' */
        trad_element_as_item();
    }
}

/*
 * trad_json — punto de entrada del traductor.
 *
 * Elemento raiz:
 *   objeto => sus atributos se emiten directamente (las claves son tags raiz)
 *   array  => cada elemento se emite como <item>
 */
static void trad_json(void) {
    if (tok_actual.codigo == L_LLAVE) {
        avanzar(); /* consumir '{' */
        if (tok_actual.codigo != R_LLAVE) {
            trad_attributes_list();
        }
        consumir(R_LLAVE, FOLLOW_ELEMENT, N_FOLLOW_ELEMENT);
    } else if (tok_actual.codigo == L_CORCHETE) {
        avanzar(); /* consumir '[' */
        if (tok_actual.codigo != R_CORCHETE) {
            trad_element_list_as_items();
        }
        consumir(R_CORCHETE, FOLLOW_ELEMENT, N_FOLLOW_ELEMENT);
    } else {
        error_sintactico("'{' o '['");
        sincronizar(FOLLOW_ELEMENT, N_FOLLOW_ELEMENT);
    }
}

/* ================================================================
 * MAIN
 * ================================================================ */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s <fuente.json> <output.xml>\n", argv[0]);
        return 1;
    }

    /* --- Pase 1: analisis sintactico (deteccion de errores) --- */
    src_file = fopen(argv[1], "r");
    if (!src_file) {
        printf("No se puede abrir el archivo fuente: %s\n", argv[1]);
        return 1;
    }

    avanzar();
    parse_json();
    fclose(src_file);

    if (num_errores > 0) {
        printf("Se encontraron %d error(es). No se genero el archivo XML.\n", num_errores);
        return 1;
    }

    /* --- Pase 2: traduccion JSON -> XML --- */
    src_file = fopen(argv[1], "r");
    if (!src_file) {
        printf("Error al reabrir el archivo fuente: %s\n", argv[1]);
        return 1;
    }
    lexer_reinit();

    fout_xml = fopen(argv[2], "w");
    if (!fout_xml) {
        printf("No se puede crear el archivo de salida: %s\n", argv[2]);
        fclose(src_file);
        return 1;
    }

    avanzar();
    trad_json();

    fclose(src_file);
    fclose(fout_xml);

    printf("Traduccion exitosa: %s\n", argv[2]);
    return 0;
}
