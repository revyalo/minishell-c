# Minishell en C

Implementacion de una shell sencilla en C para practicar llamadas al sistema Unix/Linux, gestion de procesos, pipes, redirecciones, senales y permisos.

El proyecto nace como una practica academica, pero esta preparado para poder revisarse como proyecto de portfolio tecnico.

## Funcionalidades

- Ejecucion de comandos externos con `fork` y `execvp`.
- Pipes entre varios comandos.
- Redireccion de entrada, salida y error.
- Ejecucion de procesos en background con `&`.
- Gestion basica de trabajos con `jobs` y `fg`.
- Comandos internos:
  - `cd`
  - `umask`
  - `jobs`
  - `fg [id]`
  - `exit`
- Limpieza de procesos zombie con `waitpid` y `WNOHANG`.
- Tratamiento de `SIGINT` para evitar cerrar la shell con `Ctrl+C`.

## Relacion con ciberseguridad

Aunque no es una herramienta ofensiva, este proyecto demuestra fundamentos importantes para ciberseguridad y administracion de sistemas:

- Comprension de procesos en Unix/Linux.
- Manejo de descriptores de fichero.
- Permisos y mascaras con `umask`.
- Comunicacion entre procesos mediante pipes.
- Control de senales.
- Ejecucion de programas desde una shell.
- Gestion de errores al interactuar con el sistema operativo.

Estos conceptos son base para entender hardening, analisis de comportamiento de procesos, scripting seguro, sandboxes, privilegios y funcionamiento interno de herramientas de seguridad.

## Requisitos

- Compilador C (`gcc` o `clang`).
- Sistema compatible con alguna de las librerias `parser` incluidas.
- Entorno Unix/Linux o macOS.

El parser se distribuye como libreria estatica porque la practica original proporcionaba esa dependencia ya compilada.

## Librerias de parser incluidas

| Archivo | Plataforma esperada |
| --- | --- |
| `libparser.a` | Linux i386 / 32-bit |
| `libparser_64.a` | Linux x86_64 |
| `libparserARMLinux.a` | Linux ARM64 |
| `libparserARMMac.a` | macOS ARM64 / Apple Silicon |

Nota: este paquete no incluye una libreria `parser` para macOS x86_64/Intel. En ese caso hace falta conseguir una version compatible o compilar/probar el proyecto en Linux x86_64.

## Compilacion

Usa:

```bash
make
```

El `Makefile` detecta la plataforma y selecciona la libreria adecuada cuando existe una compatible.

Tambien se puede indicar una libreria manualmente:

```bash
make LIBPARSER=libparser_64.a
```

Comandos utiles:

```bash
make parser-info
make clean
```

## Uso

Modo interactivo:

```bash
./mymsh
```

Ejecutar una linea directamente:

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

Proyecto academico funcional con parser externo. El siguiente paso natural seria sustituir la libreria estatica por una implementacion propia del parser para que el repositorio sea completamente portable.
