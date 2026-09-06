# Alternativa descartada (por ahora): Markdown como fuente + pandoc

Explorada y **descartada** en sesión de 2026-09-06 al comparar el flujo
actual (edición directa del `.docx`) con la idea de invertir la fuente
de verdad a Markdown y generar el `.docx` con pandoc. Se deja documentada
aquí por si en el futuro compensa retomarla (p. ej. si pandoc mejora la
conversión de tablas a `.docx`, o si el recorte de fidelidad visual deja
de importar).

## La idea

Editar `MANUAL/ES/SD81_Manual_ES.md` directamente (mucho más barato que
tocar XML), y regenerar el `.docx` con:

```bash
pandoc SD81_Manual_ES.md --reference-doc=SD81_Manual_ES.docx --toc \
  --resource-path=. -o SD81_Manual_ES_regenerado.docx
```

(`--reference-doc` apunta al propio `.docx` existente para heredar sus
estilos; las imágenes referenciadas por el `.md` como `media/imageN.png`
hay que extraerlas antes del `.docx` original a una carpeta `media/`
junto al `.md`, si no pandoc no las encuentra al reconvertir.)

## Lo que se probó y el resultado (verificado con captura, no solo teoría)

- **Portada**: desaparece como página propia. Todo el contenido de la
  carátula (logo, título, subtítulo) queda apilado en una sola página
  junto con el índice, sin la maquetación centrada actual. El título del
  índice además sale fijo en inglés ("Table of Contents"), sin respetar
  el idioma del documento.
- **Tablas**: esto fue lo decisivo. Las tablas pipe de Markdown, al
  reconvertir a `.docx`, no salen como tabla — salen como una lista
  plana de líneas de texto sueltas, sin rejilla, sin bordes, sin
  columnas alineadas. No es una pérdida de estilo (colores, sombreado),
  es una rotura estructural: la mitad del contenido técnico del manual
  (tablas de POKEs, comandos MCU, registros AY...) deja de ser legible
  como tabla.
- Lo que sí sobrevivía razonablemente: colores de encabezado, negrita,
  el fondo oscuro de los bloques de código (si se mapea bien el estilo
  en la plantilla de referencia).

## Por qué se descartó

El problema de las tablas no es un matiz estético asumible, es una
regresión real y grave. Arreglarlo habría exigido depurar por qué el
conversor de pandoc a `.docx` rompe estas tablas concretas (¿retorno de
carro dentro de celda? ¿demasiadas columnas? ¿algo del wrap con
`--wrap=none`?) sin garantía de que la solución sea estable frente a la
siguiente tabla con una forma distinta.

## Qué se adoptó en su lugar

`MANUAL/tools/docx_helpers.py`: helpers de `python-docx` que editan el
`.docx` existente por objetos (párrafos, tablas) en vez de por texto XML
crudo, manteniendo exactamente el `.docx` y su fidelidad visual actuales,
solo reduciendo el código repetitivo de cada edición.
