# TAMA

## Contexto General del Proyecto

TAMA no es un Tamagotchi.

TAMA es una plataforma modular para dispositivos embebidos basada en ESP32 cuyo primer caso de uso es una mascota virtual.

La filosofía del proyecto toma inspiración de:

- Linux
- NixOS
- Godot
- Unity
- Bevy
- Home Assistant
- Flipper Zero

El objetivo no es crear un firmware.

El objetivo es construir un ecosistema.

El firmware es solamente el kernel.

Todo lo demás debe poder cambiar sin recompilar el núcleo.

------------------------------------------------------------

# Filosofía

Todo debe ser:

- modular
- reproducible
- desacoplado
- descubrible
- actualizable
- reutilizable
- extensible

Cada componente debe poder reemplazarse sin modificar el resto.

El sistema debe evolucionar durante años sin necesidad de rehacer su arquitectura.

------------------------------------------------------------

# Principio Fundamental

La lógica pertenece a quien posee el conocimiento.

Nunca al coordinador.

El ESP32 principal nunca debe conocer la implementación de un módulo.

Sólo conoce interfaces.

El módulo posee la inteligencia.

El Main solamente coordina.

Ejemplo:

Main

↓

"¿Quién sos?"

↓

Módulo

↓

"Soy un lector NFC"

↓

"Estas son mis capacidades"

↓

"Estos son mis eventos"

↓

"Estos son mis comandos"

↓

Main registra automáticamente esos servicios.

------------------------------------------------------------

# Arquitectura

El sistema está dividido en varias capas.

Hardware

↓

HAL (Hardware Abstraction Layer)

↓

Drivers

↓

Kernel

↓

Services

↓

Event Bus

↓

Renderer

↓

Scene Manager

↓

UI Framework

↓

Application Layer

↓

Games

↓

Tamagotchi

Cada capa sólo conoce la inmediatamente inferior.

Nunca la superior.

------------------------------------------------------------

# Filosofía Linux

El kernel nunca implementa aplicaciones.

Sólo expone servicios.

El juego nunca habla directamente con:

- I2C
- SPI
- SD
- WiFi
- MQTT
- Home Assistant

Siempre utiliza servicios.

Ejemplo

Pet

↓

WeatherService

↓

No sabe si la información viene de:

- Home Assistant
- Internet
- Sensor
- ESP32 externo

Eso es transparente.

------------------------------------------------------------

# Filosofía Nix

Todo el sistema debe ser reproducible.

Cada versión del dispositivo puede reconstruirse únicamente con:

Firmware

+

Assets

+

Manifest

+

Configuración

Nada debe depender del estado interno del dispositivo.

Todo debe poder reinstalarse desde cero.

------------------------------------------------------------

# Assets

Los assets nunca viven dentro del firmware.

Siempre en almacenamiento externo.

Ejemplo

/tama/

sprites/

backgrounds/

fonts/

audio/

animations/

locales/

themes/

mods/

ui/

icons/

config/

Los assets son datos.

Nunca código.

------------------------------------------------------------

# Motor 2D

El renderer nunca conoce el juego.

Sólo dibuja objetos.

Todo objeto visible hereda de RenderObject.

Ejemplos

Sprite

Text

Button

ProgressBar

Popup

Window

Icon

Particle

Cursor

TileMap

Animation

Todos poseen:

posición

escala

rotación

visibilidad

layer

dirty flag

bounding box

------------------------------------------------------------

# Render

El renderer funciona igual que un motor de videojuegos.

Background Cache

↓

FrameBuffer

↓

Background Layer

↓

World Layer

↓

Characters

↓

Effects

↓

UI

↓

Popup

↓

Debug

↓

Display

Nunca se dibuja directamente sobre la pantalla.

Siempre sobre un framebuffer.

El background solamente se vuelve a renderizar cuando cambia.

Todo lo demás utiliza dirty rectangles cuando sea posible.

------------------------------------------------------------

# Asset Manager

Existe un administrador de recursos.

Nunca se abre un PNG dos veces.

SD

↓

AssetManager

↓

Texture Cache

↓

Renderer

Cuando un recurso deja de utilizarse puede liberarse automáticamente.

------------------------------------------------------------

# Event Bus

Todo el sistema funciona mediante eventos.

Nunca mediante llamadas directas.

Ejemplo

Touch

↓

TouchPressed

↓

Button

↓

Inventory

↓

Renderer

Todo desacoplado.

------------------------------------------------------------

# Services

Los servicios representan capacidades.

No hardware.

Ejemplos

Weather

RTC

Storage

Audio

Notifications

Localization

WiFi

BLE

MQTT

HomeAssistant

OTA

Camera

NFC

GPS

El juego únicamente solicita servicios.

Nunca dispositivos.

------------------------------------------------------------

# Home Assistant

Home Assistant no es una dependencia.

Es un proveedor de servicios.

Puede proveer:

Hora

Clima

Temperatura

Calendario

Luces

Sensores

Automatizaciones

Notificaciones

Escenas

Presencia

El juego simplemente consulta:

WeatherService

No importa quién responda.

------------------------------------------------------------

# MQTT

MQTT representa el EventBus distribuido.

Todo evento puede publicarse.

Ejemplo

PetHungry

↓

MQTT

↓

Home Assistant

↓

Automatización

↓

Se prende una luz naranja.

O

Main

↓

ModuleConnected

↓

MQTT

↓

Dashboard

Todo el dispositivo puede verse desde cualquier lugar.

------------------------------------------------------------

# Arquitectura Modular

El dispositivo principal posee:

CPU

Pantalla

Touch

Batería

SD

I²C

Los módulos contienen inteligencia propia.

Cada módulo posee:

Microcontrolador

Firmware

Manifest

Versión

Servicios

Eventos

Assets opcionales

No existen módulos "tontos".

Todos son computadoras independientes.


## Directorios
tree
.
├── compile.sh
├── objetivo.md
├── platformio.ini
├── resumen_proyecto.txt
├── sd_root -> /home/shur3/Downloads/DriveSyncFiles/Programming FCEIA/Arduino/Tamagotchi_LOVE/sd_root
├── shell.nix
└── src
    ├── assets
    │   ├── AssetManager.cpp
    │   ├── AssetManager.h
    │   └── Texture.h
    ├── colors.h
    ├── Game.h
    ├── main.cpp
    ├── render
    │   ├── Blend.h
    │   ├── RenderContext.h
    │   ├── Renderer.cpp
    │   ├── Renderer.h
    │   ├── RenderLayer.h
    │   ├── RenderObject.h
    │   ├── Scene.cpp
    │   ├── Scene.h
    │   └── Sprite.h
    ├── Tamagotchi.cpp
    ├── Tamagotchi.h
    ├── touch_axs5106.h
    ├── UI.cpp
    └── UI.h

sd_root
├── QR Network.png
├── tama
│   ├── sprites
│   │   ├── accesorios
│   │   │   ├── anillo
│   │   │   ├── collar
│   │   │   └── mochila
│   │   ├── base
│   │   │   └── tiernito
│   │   │       ├── adulto
│   │   │       │   ├── dead
│   │   │       │   ├── eating
│   │   │       │   ├── happy
│   │   │       │   ├── idle
│   │   │       │   ├── sad
│   │   │       │   ├── sick
│   │   │       │   └── sleeping
│   │   │       ├── anciano
│   │   │       │   ├── dead
│   │   │       │   ├── eating
│   │   │       │   ├── happy
│   │   │       │   ├── idle
│   │   │       │   ├── sad
│   │   │       │   ├── sick
│   │   │       │   └── sleeping
│   │   │       ├── bebe
│   │   │       │   ├── dead
│   │   │       │   ├── eating
│   │   │       │   ├── happy
│   │   │       │   ├── idle
│   │   │       │   ├── sad
│   │   │       │   ├── sick
│   │   │       │   └── sleeping
│   │   │       ├── child
│   │   │       │   ├── dead
│   │   │       │   ├── eating
│   │   │       │   ├── happy
│   │   │       │   ├── idle.png
│   │   │       │   ├── sad
│   │   │       │   ├── sick
│   │   │       │   └── sleeping
│   │   │       └── huevo
│   │   │           ├── dead
│   │   │           ├── eating
│   │   │           ├── happy
│   │   │           ├── idle.ooo.png
│   │   │           ├── idle.png
│   │   │           ├── sad
│   │   │           ├── sick
│   │   │           └── sleeping
│   │   ├── sprite.xcf
│   │   └── vestimenta
│   │       ├── anteojos
│   │       ├── pantalon
│   │       ├── remera
│   │       ├── sombrero
│   │       └── zapatos
│   └── ui
│       ├── bg_main.png
│       └── hunger_icon.png
├── Waveshare Image10.png
├── Waveshare Image2.png
├── Waveshare Image3.png
├── Waveshare Image4.png
├── Waveshare Image5.png
├── Waveshare Image6.png
├── Waveshare Image7.png
├── Waveshare Image8.png
├── Waveshare Image9.png
└── WaveshareImage1.png


------------------------------------------------------------

# Comunicación

El Main siempre es Master.

Todos los módulos son esclavos lógicos.

Los módulos nunca hablan entre ellos.

Todo pasa por el Main.

Main

↓

I²C

↓

Module

↓

Respuesta

El Main construye un mapa completo del sistema.

------------------------------------------------------------

# Descubrimiento

Al iniciar:

Buscar módulos

↓

HELLO

↓

Cada módulo responde

↓

Enviar Manifest

↓

Registrar Servicios

↓

Registrar Eventos

↓

Registrar Assets

↓

Registrar Versiones

↓

Sistema listo

------------------------------------------------------------

# Manifest del módulo

Cada módulo posee un manifest.

Ejemplo

Nombre

Modelo

Firmware

ProtocolVersion

Capabilities

Services

RequiredMainVersion

Assets

OTA Endpoint

Checksum

------------------------------------------------------------

# OTA

El único dispositivo con acceso a Internet es el Main.

Los módulos nunca descargan firmware directamente.

Flujo:

Main consulta GitHub Releases.

↓

Detecta nueva versión.

↓

Descarga firmware del módulo.

↓

Envía metadatos al módulo.

↓

El módulo entra en modo Bootloader.

↓

Main transmite el firmware por I²C en bloques.

↓

CRC.

↓

Reinicio.

↓

Nuevo firmware.

El Main funciona como un gestor centralizado de actualizaciones para todo el ecosistema. Los módulos no conocen GitHub, Wi-Fi ni Internet; sólo implementan un protocolo de actualización seguro.

------------------------------------------------------------

# Reproducibilidad

Todo el dispositivo puede reconstruirse únicamente con:

Firmware Release

Assets Release

Manifest

Configuración

No existen estados ocultos.

No existen configuraciones internas no exportables.

Todo debe poder respaldarse.

Todo debe poder restaurarse.

------------------------------------------------------------

# Escenas

Todo funciona mediante escenas.

Boot

Splash

Desktop

Pet

Inventory

Store

Modules

Developer

Updater

WiFi

Bluetooth

Power

Cada escena administra únicamente sus propios objetos.

------------------------------------------------------------

# Futuro

En el futuro el mismo motor debe permitir desarrollar:

Tamagotchi

Pokédex

Panel IoT

Launcher

Mini juegos

Consola retro

Dashboard

Educación

Robot

Domótica

Todo sin modificar el motor.

------------------------------------------------------------

# Visión Final

El objetivo no es fabricar un juguete.

El objetivo es construir un pequeño sistema operativo para objetos físicos.

Un dispositivo cuya personalidad evoluciona mediante software.

Donde cada actualización agrega capacidades.

Cada módulo agrega nuevas posibilidades.

Y donde el hardware permanece vigente durante muchos años gracias a una arquitectura modular, reproducible y desacoplada.

El Tamagotchi es únicamente la primera aplicación que demuestra todo el potencial del sistema.

> **El hardware es permanente; el software es evolutivo.**
> El dispositivo que recibe una persona el primer día debe seguir siendo el mismo años después, pero con nuevas habilidades, nuevos módulos y nuevas experiencias. No se reemplaza el objeto: se acompaña su crecimiento. Esa continuidad es parte del valor emocional del proyecto, porque el compañero digital evoluciona junto con quien lo usa.
