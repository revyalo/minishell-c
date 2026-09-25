# Minishell en C

Implementación de una shell sencilla en C para practicar llamadas al sistema Unix/Linux, gestión de procesos, pipes, redirecciones, señales y permisos.

El proyecto nace como una práctica académica y está organizado como proyecto de portfolio centrado en programación de sistemas.

## Funcionalidades

- Ejecución de comandos externos con `fork` y `execvp`.
- Pipes entre varios comandos.
- Redirección de entrada, salida y error.
- Ejecución de procesos en background con `&`.
- Gestión básica de trabajos con `jobs` y `fg`.
- Comandos internos:
  - `cd`
  - `umask`
  - `jobs`
  - `fg [id]`
  - `exit`
- Limpieza de procesos zombie con `waitpid` y `WNOHANG`.
- Tratamiento de `SIGINT` para evitar cerrar la shell con `Ctrl+C`.

## Conceptos trabajados

Este proyecto está orientado principalmente a programación de sistemas en entornos Unix/Linux:

- Creación y control de procesos.
- Uso de `fork`, `execvp` y `waitpid`.
- Manejo de descriptores de fichero.
- Comunicación entre procesos mediante pipes.
- Redirección de entrada, salida y error.
- Gestión de señales.
- Procesos en foreground y background.
- Gestión básica de trabajos.
- Permisos y máscaras con `umask`.
- Tratamiento de errores al interactuar con el sistema operativo.

En conjunto, la práctica ayuda a entender cómo una shell coordina procesos y recursos del sistema operativo a bajo nivel.

## Requisitos

- Compilador C (`gcc` o `clang`).
- Sistema compatible con alguna de las librerías `parser` incluidas.
- Entorno Unix/Linux o macOS.

El parser se distribuye como librería estática porque la práctica original proporcionaba esa dependencia ya compilada.

## Librerías de parser incluidas

| Archivo | Plataforma esperada |
| --- | --- |
| `libparser.a` | Linux i386 / 32-bit |
| `libparser_64.a` | Linux x86_64 |
| `libparserARMLinux.a` | Linux ARM64 |
| `libparserARMMac.a` | macOS ARM64 / Apple Silicon |

Nota: este paquete no incluye una librería `parser` para macOS x86_64/Intel. En ese caso hace falta conseguir una versión compatible o compilar/probar el proyecto en Linux x86_64.

## Compilación

Usa:

```bash
make
```

El `Makefile` detecta la plataforma y selecciona la librería adecuada cuando existe una compatible.

También se puede indicar una librería manualmente:

```bash
make LIBPARSER=libparser_64.a
```

Comandos útiles:

```bash
make parser-info
make clean
```

## Uso

Modo interactivo:

```bash
./mymsh
```

Ejecutar una línea directamente:

```bash
./mymsh "ls -l | wc -l"
```

Ejemplos:

```bash
msh> pwd
msh> cd /tmp
msh> ls -l | grep txt
msh> cat entrada.txt > salida.txt
msh> sleep 10 &
msh> jobs
msh> fg
msh> fg 1
msh> umask
msh> exit
```

## Estructura

```text
.
|-- mymsh.c
|-- parser.h
|-- test.c
|-- libparser.a
|-- libparser_64.a
|-- libparserARMLinux.a
|-- libparserARMMac.a
|-- Makefile
`-- README.md
```

## Estado

Proyecto académico funcional con parser externo. El siguiente paso natural sería sustituir la librería estática por una implementación propia del parser para que el repositorio sea completamente portable.
