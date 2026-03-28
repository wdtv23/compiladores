/*
 * Analizador Lexico - JSON Simplificado
 * Curso: Compiladores y Lenguajes de Bajo Nivel
 * Practica de Programacion Nro. 1
 *
 * Descripcion:
 * Lee un archivo fuente JSON y por cada linea genera una linea en el
 * archivo de salida con la secuencia de componentes lexicos separados
 * por espacios. En caso de error lexico imprime un mensaje de error y
 * continua con la siguiente linea.
 *
 * Uso: ./anlex <archivo_fuente> <archivo_salida>
 */

#include "anlex.h"

/*
 * Variables globales:
 * - numLinea: lleva la cuenta de que linea del archivo fuente se esta procesando.
 *             Se usa para mostrar en que linea ocurrio un error.
 * - fout:     puntero al archivo de salida donde se escriben los tokens encontrados.
 */
int numLinea = 0;
FILE *fout;

/*
 * nombreToken - Convierte un codigo numerico de token a su nombre en texto.
 *
 * Cada token esta representado internamente por un numero (definido en anlex.h).
 * Esta funcion devuelve el string que se escribe en el archivo de salida.
 *
 * Ejemplo: nombreToken(L_CORCHETE) retorna "L_CORCHETE"
 *          nombreToken(LITERAL_NUM) retorna "LITERAL_NUM"
 */
const char *nombreToken(int codigo) {
    switch (codigo) {
        case L_CORCHETE:     return "L_CORCHETE";
        case R_CORCHETE:     return "R_CORCHETE";
        case L_LLAVE:        return "L_LLAVE";
        case R_LLAVE:        return "R_LLAVE";
        case COMA:           return "COMA";
        case DOS_PUNTOS:     return "DOS_PUNTOS";
        case LITERAL_CADENA: return "LITERAL_CADENA";
        case LITERAL_NUM:    return "LITERAL_NUM";
        case PR_TRUE:        return "PR_TRUE";
        case PR_FALSE:       return "PR_FALSE";
        case PR_NULL:        return "PR_NULL";
        case EOF:            return "EOF";
        default:             return "DESCONOCIDO";
    }
}

/*
 * errorLexico - Imprime un mensaje de error lexico en la salida estandar.
 *
 * Formato del mensaje: "Lin N: Error Lexico. <mensaje>."
 * Se usa numLinea para indicar en que linea del archivo fuente ocurrio el error.
 *
 * Parametro:
 *   mensaje - descripcion del error encontrado
 */
void errorLexico(const char *mensaje) {
    printf("Lin %d: Error Lexico. %s.\n", numLinea, mensaje);
}

/*
 * parsearNumero - Reconoce un token LITERAL_NUM a partir de la posicion actual.
 *
 * Implementa un automata finito determinista (AFD) que reconoce numeros con
 * el siguiente formato (expresion regular):
 *
 *   -?[0-9]+(\.[0-9]+)?((e|E)(+|-)?[0-9]+)?
 *
 * Ejemplos validos: 42, -7, 3.14, 1.5e3, 2E-10, 100
 * Ejemplos invalidos: 1., 1e, 1e+, .5
 *
 * El automata tiene 7 estados:
 *
 *   Estado 0: Leyendo digitos de la parte entera.
 *             Puede continuar con '.', 'e'/'E', o terminar (aceptar).
 *
 *   Estado 1: Se leyo un punto '.'. OBLIGATORIO que siga un digito.
 *             Si no viene digito -> ERROR.
 *
 *   Estado 2: Leyendo digitos de la parte decimal.
 *             Puede continuar con 'e'/'E', o terminar (aceptar).
 *
 *   Estado 3: Se leyo 'e' o 'E'. Puede seguir '+', '-' o digito.
 *             Si no viene ninguno -> ERROR.
 *
 *   Estado 4: Se leyo el signo del exponente. OBLIGATORIO que siga un digito.
 *             Si no viene digito -> ERROR.
 *
 *   Estado 5: Leyendo digitos del exponente.
 *             Puede continuar con mas digitos, o terminar (aceptar).
 *
 *   Estado 6: Estado de ACEPTACION. El numero fue reconocido correctamente.
 *
 * Parametros:
 *   line - la linea de texto actual
 *   pos  - puntero a la posicion actual dentro de la linea (se avanza al leer)
 *
 * Retorna: LITERAL_NUM si reconocio el numero, -1 si hay error lexico
 */
int parsearNumero(const char *line, int *pos) {
    int estado = 0;
    int acepto = 0;
    char msg[64];

    /* Consumir el signo negativo si existe.
     * Se llega aqui solo si el caracter siguiente es un digito (verificado
     * en siguienteToken), asi que '-' solo es valido si le sigue un digito. */
    if (line[*pos] == '-') (*pos)++;

    while (!acepto) {
        char c = line[*pos]; /* caracter actual SIN avanzar todavia */

        switch (estado) {

            case 0: /* parte entera: consumir digitos */
                if (isdigit(c)) {
                    (*pos)++;           /* avanzar y seguir en estado 0 */
                } else if (c == '.') {
                    (*pos)++;
                    estado = 1;         /* puede haber parte decimal */
                } else if (c == 'e' || c == 'E') {
                    (*pos)++;
                    estado = 3;         /* puede haber exponente */
                } else {
                    estado = 6;         /* ningun otro caracter pertenece al numero: aceptar */
                }
                break;

            case 1: /* se leyo '.', obligatorio un digito a continuacion */
                if (isdigit(c)) {
                    (*pos)++;
                    estado = 2;
                } else {
                    /* '1.' sin digito decimal es invalido */
                    sprintf(msg, "Se esperaba digito despues del punto, se encontro '%c'", c);
                    errorLexico(msg);
                    return -1;
                }
                break;

            case 2: /* parte decimal: consumir digitos */
                if (isdigit(c)) {
                    (*pos)++;           /* seguir leyendo decimales */
                } else if (c == 'e' || c == 'E') {
                    (*pos)++;
                    estado = 3;         /* puede haber exponente */
                } else {
                    estado = 6;         /* aceptar */
                }
                break;

            case 3: /* se leyo 'e'/'E', puede seguir signo o digito */
                if (c == '+' || c == '-') {
                    (*pos)++;
                    estado = 4;         /* leyo el signo, ahora obligatorio un digito */
                } else if (isdigit(c)) {
                    (*pos)++;
                    estado = 5;         /* exponente sin signo explicito */
                } else {
                    /* '1e' o '1e.' sin digito de exponente es invalido */
                    sprintf(msg, "Se esperaba digito o signo en exponente, se encontro '%c'", c);
                    errorLexico(msg);
                    return -1;
                }
                break;

            case 4: /* se leyo el signo, obligatorio un digito */
                if (isdigit(c)) {
                    (*pos)++;
                    estado = 5;
                } else {
                    /* '1e+' sin digito es invalido */
                    sprintf(msg, "Se esperaba digito en exponente, se encontro '%c'", c);
                    errorLexico(msg);
                    return -1;
                }
                break;

            case 5: /* digitos del exponente */
                if (isdigit(c)) {
                    (*pos)++;           /* seguir leyendo digitos del exponente */
                } else {
                    estado = 6;         /* aceptar */
                }
                break;

            case 6: /* ACEPTACION: el numero termino, no avanzar (el caracter actual
                       pertenece al proximo token) */
                acepto = 1;
                break;
        }
    }
    return LITERAL_NUM;
}

/*
 * parsearCadena - Reconoce un token LITERAL_CADENA a partir de la posicion actual.
 *
 * Una cadena JSON comienza y termina con comillas dobles '"'.
 * Dentro puede tener cualquier caracter, incluidas secuencias de escape
 * como \" (comilla literal), \\ (barra invertida), \n, \t, \uXXXX, etc.
 *
 * La funcion consume caracter a caracter hasta encontrar la '"' de cierre.
 * Si llega al final de la linea sin cerrar la cadena, es un error lexico.
 *
 * Parametros:
 *   line - la linea de texto actual
 *   pos  - puntero a la posicion actual (apunta a la '"' inicial al entrar)
 *
 * Retorna: LITERAL_CADENA si reconocio la cadena, -1 si hay error lexico
 */
int parsearCadena(const char *line, int *pos) {
    (*pos)++; /* saltar la comilla inicial '"' */

    while (1) {
        char c = line[*pos];

        /* fin de linea sin cerrar la cadena: error */
        if (c == '\0' || c == '\n' || c == '\r') {
            errorLexico("Cadena sin cerrar");
            return -1;
        }

        /* comilla de cierre: cadena completa */
        if (c == '"') {
            (*pos)++; /* consumir la '"' de cierre */
            return LITERAL_CADENA;
        }

        /* secuencia de escape: el caracter siguiente al '\' no termina la cadena,
         * se consume junto con el '\' sin importar cual sea (\" no cierra la cadena) */
        if (c == '\\') {
            (*pos)++; /* saltar el '\' */
            c = line[*pos];
            if (c == '\0' || c == '\n' || c == '\r') {
                errorLexico("Secuencia de escape incompleta");
                return -1;
            }
            /* el caracter escapado se consume en el (*pos)++ del final del while */
        }

        (*pos)++; /* avanzar al siguiente caracter de la cadena */
    }
}

/*
 * parsearPalabraReservada - Reconoce true/TRUE, false/FALSE o null/NULL.
 *
 * Lee todos los caracteres alfabeticos consecutivos formando una palabra,
 * luego verifica si coincide exactamente con alguna de las palabras reservadas
 * del lenguaje JSON. Solo se aceptan las formas totalmente en minusculas o
 * totalmente en mayusculas (ej: 'True' o 'tRuE' son errores lexicos).
 *
 * Parametros:
 *   line - la linea de texto actual
 *   pos  - puntero a la posicion actual (apunta al primer caracter alfabetico)
 *
 * Retorna: PR_TRUE, PR_FALSE o PR_NULL segun corresponda, -1 si no es valida
 */
int parsearPalabraReservada(const char *line, int *pos) {
    char palabra[TAMLEX];
    int i = 0;
    char msg[TAMLEX + 30];

    /* leer todos los caracteres alfabeticos consecutivos */
    while (isalpha(line[*pos])) {
        palabra[i++] = line[(*pos)++];
        if (i >= TAMLEX - 1) break; /* evitar desbordamiento del buffer */
    }
    palabra[i] = '\0';

    /* comparar con las palabras reservadas validas */
    if (strcmp(palabra, "true")  == 0 || strcmp(palabra, "TRUE")  == 0) return PR_TRUE;
    if (strcmp(palabra, "false") == 0 || strcmp(palabra, "FALSE") == 0) return PR_FALSE;
    if (strcmp(palabra, "null")  == 0 || strcmp(palabra, "NULL")  == 0) return PR_NULL;

    /* la palabra no corresponde a ningun token valido */
    sprintf(msg, "Palabra no reconocida '%s'", palabra);
    errorLexico(msg);
    return -1;
}

/*
 * siguienteToken - Obtiene el siguiente token de la linea a partir de *pos.
 *
 * Es el corazon del analizador lexico. Mira el primer caracter no-blanco
 * y decide que tipo de token es, delegando el trabajo a las funciones
 * especializadas (parsearNumero, parsearCadena, parsearPalabraReservada)
 * cuando el token requiere leer mas de un caracter.
 *
 * Parametros:
 *   line - la linea de texto actual
 *   pos  - puntero a la posicion actual dentro de la linea
 *
 * Retorna:
 *    codigo del token reconocido (valor positivo)
 *    0  si llego al final de la linea (no hay mas tokens en esta linea)
 *   -1  si encontro un error lexico
 */
int siguienteToken(const char *line, int *pos) {
    char msg[64];

    /* saltar espacios en blanco y tabulaciones (no son tokens en JSON) */
    while (line[*pos] == ' ' || line[*pos] == '\t') (*pos)++;

    char c = line[*pos]; /* primer caracter significativo */

    /* fin de linea: no hay mas tokens */
    if (c == '\0' || c == '\n' || c == '\r') return 0;

    /* identificar el token segun el primer caracter */
    switch (c) {
        /* tokens de un solo caracter: consumir y retornar directamente */
        case '[': (*pos)++; return L_CORCHETE;
        case ']': (*pos)++; return R_CORCHETE;
        case '{': (*pos)++; return L_LLAVE;
        case '}': (*pos)++; return R_LLAVE;
        case ',': (*pos)++; return COMA;
        case ':': (*pos)++; return DOS_PUNTOS;

        /* cadena: comienza con comilla doble */
        case '"': return parsearCadena(line, pos);

        /* numero negativo: '-' seguido de digito es un numero valido */
        case '-':
            if (isdigit(line[*pos + 1])) return parsearNumero(line, pos);
            /* '-' solo o seguido de no-digito: error */
            sprintf(msg, "Caracter no reconocido '%c'", c);
            errorLexico(msg);
            (*pos)++;
            return -1;

        default:
            /* numero positivo: comienza con digito */
            if (isdigit(c)) return parsearNumero(line, pos);
            /* palabra reservada: comienza con letra */
            if (isalpha(c)) return parsearPalabraReservada(line, pos);
            /* cualquier otro caracter no pertenece al lenguaje */
            sprintf(msg, "Caracter no reconocido '%c'", c);
            errorLexico(msg);
            (*pos)++;
            return -1;
    }
}

/*
 * procesarLinea - Tokeniza una linea completa y escribe los tokens al archivo de salida.
 *
 * Llama repetidamente a siguienteToken hasta que no haya mas tokens en la linea
 * o se encuentre un error lexico. Escribe los nombres de los tokens separados
 * por espacios en fout, seguidos de un salto de linea.
 *
 * En caso de error, se escriben los tokens reconocidos hasta ese punto y
 * se termina la linea (la siguiente llamada procesara la proxima linea del fuente).
 *
 * Parametro:
 *   line - la linea de texto a procesar (tal como la devuelve fgets)
 */
void procesarLinea(const char *line) {
    int pos = 0;    /* posicion actual dentro de la linea */
    int primero = 1; /* flag para no poner espacio antes del primer token */
    int tok;

    while (1) {
        tok = siguienteToken(line, &pos);

        if (tok == 0)  break; /* fin de linea: salir del bucle normalmente */
        if (tok == -1) break; /* error lexico: salir y continuar con la proxima linea */

        /* separar tokens con espacio (excepto antes del primero) */
        if (!primero) fprintf(fout, " ");
        fprintf(fout, "%s", nombreToken(tok));
        primero = 0;
    }

    fprintf(fout, "\n"); /* una linea de salida por cada linea del fuente */
}

/*
 * main - Punto de entrada del programa.
 *
 * Abre el archivo fuente y el archivo de salida, luego procesa el fuente
 * linea por linea con fgets + procesarLinea. Al final escribe el token EOF.
 *
 * Argumentos de linea de comandos:
 *   argv[1] - path al archivo fuente JSON
 *   argv[2] - path al archivo de salida
 */
int main(int argc, char *argv[]) {
    FILE *fin;
    char line[TAMBUFF]; /* buffer para leer una linea completa del fuente */

    if (argc < 3) {
        printf("Uso: %s <archivo_fuente> <archivo_salida>\n", argv[0]);
        return 1;
    }

    fin = fopen(argv[1], "r");
    if (!fin) {
        printf("No se puede abrir el archivo fuente: %s\n", argv[1]);
        return 1;
    }

    fout = fopen(argv[2], "w");
    if (!fout) {
        printf("No se puede crear el archivo de salida: %s\n", argv[2]);
        fclose(fin);
        return 1;
    }

    /* leer el archivo fuente linea por linea */
    while (fgets(line, TAMBUFF, fin)) {
        numLinea++;          /* contar la linea para mensajes de error */
        procesarLinea(line); /* tokenizar y escribir en el archivo de salida */
    }

    /* el token EOF se emite una vez al terminar todo el archivo fuente */
    fprintf(fout, "EOF\n");

    fclose(fin);
    fclose(fout);
    return 0;
}
