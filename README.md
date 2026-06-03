compiladores
============

Compiladores FPUNA  
Autor: Walter Torales

---

## Tarea 1 — Analizador Léxico (JSON simplificado)

Lee un archivo fuente JSON y genera una secuencia de tokens por línea.

**Compilar:**
```
gcc anlex.c -o anlex
```
o con el Makefile:
```
make anlex
```

**Uso:**
```
./anlex <archivo_fuente> <archivo_salida>
```

**Ejemplo:**
```
./anlex fuente.txt output.txt
```

**Tokens reconocidos:**  
`L_CORCHETE` `R_CORCHETE` `L_LLAVE` `R_LLAVE` `COMA` `DOS_PUNTOS`  
`LITERAL_CADENA` `LITERAL_NUM` `PR_TRUE` `PR_FALSE` `PR_NULL` `EOF`

---

## Tarea 2 — Analizador Sintáctico Descendente Recursivo

Parser descendente recursivo para JSON simplificado que reutiliza los
códigos de tokens de la Tarea 1. Incluye recuperación de errores en
**Modo Pánico** con sincronización por conjuntos FOLLOW.

### Gramática implementada

```
json            => element EOF
element         => object | array
array           => '[' element-list ']' | '[' ']'
element-list    => element { ',' element }
object          => '{' attributes-list '}' | '{' '}'
attributes-list => attribute { ',' attribute }
attribute       => attribute-name ':' attribute-value
attribute-name  => string
attribute-value => element | string | number | true | false | null
```

> La izquierda-recursión de `element-list` y `attributes-list` se
> eliminó usando bucles `while`, equivalentes a las producciones
> originales.

### Compilar

```
make parser
```
o directamente:
```
gcc -Wall -Wextra -std=c11 -o parser parser.c
```

> `parser.c` se compila de forma independiente de `anlex.c` (ambos
> tienen `main()`). Incluye `anlex.h` sólo para reutilizar los códigos
> de tokens.

### Uso

```
./parser <archivo_fuente.json>
```

### Salida

**Si el JSON es válido:**
```
Analisis sintactico exitoso.
```

**Si hay errores:**
```
Lin <l>, Col <c>: Error Sintactico. Se esperaba <X> pero se encontro <Y> ('<lexema>').
...
Se encontraron N error(es).
```

### Ejemplo con JSON válido

```
./parser fuente.json
```
```
Analisis sintactico exitoso.
```

### Ejemplos con errores

```bash
# Coma faltante entre atributos
echo '{"a": 1 "b": 2}' > test.json && ./parser test.json
# Lin 1, Col 9: Error Sintactico. Se esperaba '}' pero se encontro cadena ('"b"').

# Dos puntos faltante
echo '{"nombre" "Juan"}' > test.json && ./parser test.json
# Lin 1, Col 11: Error Sintactico. Se esperaba ':' pero se encontro cadena ('"Juan"').

# Múltiples errores en una pasada
echo '{"a": , "b": 2, "c":}' > test.json && ./parser test.json
# Lin 1, Col 7: Error Sintactico. Se esperaba valor de atributo ...
# Lin 1, Col 21: Error Sintactico. Se esperaba valor de atributo ...
# Se encontraron 2 error(es).
```

### Modo Pánico — estrategia de recuperación

Cuando el parser encuentra un token inesperado:

1. Imprime el error con línea, columna, token esperado y token encontrado.
2. Descarta tokens del stream hasta encontrar uno en el conjunto FOLLOW
   del no-terminal en error (sincronización).
3. Continúa el análisis desde ese punto, permitiendo detectar todos los
   errores en una sola pasada.

**Conjuntos FOLLOW usados para sincronización:**

| No-terminal        | FOLLOW                           |
|--------------------|----------------------------------|
| `element`          | `EOF  ','  ']'  '}'`             |
| `array`            | `EOF  ','  ']'  '}'`             |
| `object`           | `EOF  ','  ']'  '}'`             |
| `element-list`     | `']'`                            |
| `attributes-list`  | `'}'`                            |
| `attribute`        | `','  '}'`                       |
| `attribute-name`   | `':'`                            |
| `attribute-value`  | `','  '}'`                       |

### Estructura de archivos

```
anlex.h       — Códigos de tokens (compartido, sin modificar)
anlex.c       — Analizador léxico Tarea 1 (sin modificar)
parser.c      — Lexer adaptado + parser sintáctico Tarea 2
fuente.txt    — JSON de prueba Tarea 1
fuente.json   — JSON de prueba Tarea 2
Makefile      — Compila ambos programas
```
