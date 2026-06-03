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
traductor.c   — Lexer + parser + traductor TDS Tarea 3
fuente.txt    — JSON de prueba Tarea 1
fuente.json   — JSON de prueba Tareas 2 y 3
Makefile      — Compila los tres programas
```

---

## Tarea 3 — Traductor Dirigido por Sintaxis JSON → XML

Extiende el parser de la Tarea 2 con **acciones semánticas** integradas
directamente en las funciones recursivas del parser para emitir XML equivalente.
No construye un AST intermedio: la traducción se produce en un único recorrido
descendente (segundo pase).

### Estrategia de dos pases

| Pase | Objetivo | Salida |
|------|----------|--------|
| 1 | Análisis sintáctico puro (idéntico a Tarea 2) | Mensajes de error en consola |
| 2 | Traducción con acciones semánticas `trad_*` | Archivo XML |

Si el pase 1 detecta errores, **el pase 2 no se ejecuta** y no se genera ningún
archivo XML.

### Reglas de traducción implementadas

| Construcción JSON | Salida XML |
|---|---|
| Objeto raíz `{k:v, …}` | Las claves se emiten como tags de nivel raíz (sin wrapper extra) |
| Array raíz `[e, …]` | Cada elemento envuelto en `<item>…</item>` |
| Atributo con valor primitivo | `<nombre>valor</nombre>` |
| Atributo con valor objeto | `<nombre>` + atributos del objeto + `</nombre>` |
| Atributo con array vacío | `<nombre></nombre>` |
| Atributo con array no vacío | `<nombre>` + `<item>…</item>` por elemento + `</nombre>` |
| Elemento de array que es objeto | `<item>` + atributos del objeto + `</item>` |
| Elemento de array que es array | `<item>` + items anidados + `</item>` |
| Elemento de array primitivo | `<item>valor</item>` |
| Strings | Se conservan las comillas dobles: `"Julio Pérez"` |

### Compilar

```
make traductor
```
o directamente:
```
gcc -Wall -Wextra -std=c11 -o traductor traductor.c
```

### Uso

```
./traductor <fuente.json> <output.xml>
```

### Ejemplo

**Entrada (`fuente.json`):**
```json
{
  "personas": [
    { "ci": 1234567, "nombre": "Julio Pérez", "casado": false, "hijos": [] },
    { "ci": 7654321, "nombre": "Juan Gómez", "casado": true,
      "hijos": [
        { "nombre": "Jorge", "edad": 18 },
        { "nombre": "Valeria", "edad": 16 }
      ]
    }
  ]
}
```

**Salida (`output.xml`):**
```xml
<personas>
	<item>
		<ci>1234567</ci>
		<nombre>"Julio Pérez"</nombre>
		<casado>false</casado>
		<hijos></hijos>
	</item>
	<item>
		<ci>7654321</ci>
		<nombre>"Juan Gómez"</nombre>
		<casado>true</casado>
		<hijos>
			<item>
				<nombre>"Jorge"</nombre>
				<edad>18</edad>
			</item>
			<item>
				<nombre>"Valeria"</nombre>
				<edad>16</edad>
			</item>
		</hijos>
	</item>
</personas>
```

### Salida ante errores

Si el JSON tiene errores sintácticos, el traductor los reporta (igual que el
parser de la Tarea 2) y **no genera el archivo XML**:

```
Lin 1, Col 9: Error Sintactico. Se esperaba ':' pero se encontro cadena ('"b"').
Se encontraron 1 error(es). No se genero el archivo XML.
```

### Estructura de archivos

```
anlex.h       — Códigos de tokens (compartido, sin modificar)
anlex.c       — Analizador léxico Tarea 1 (sin modificar)
parser.c      — Lexer adaptado + parser sintáctico Tarea 2
traductor.c   — Lexer + parser + traductor TDS Tarea 3
fuente.txt    — JSON de prueba Tarea 1
fuente.json   — JSON de prueba Tareas 2 y 3
Makefile      — Compila los tres programas
```
