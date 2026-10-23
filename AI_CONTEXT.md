# TAMA OS — Guía de Contexto e Invariantes para IAs (`AI_CONTEXT.md`)

> **LECTURA OBLIGATORIA PARA AGENTES E IAs DE CÓDIGO:**  
> Antes de modificar cualquier fórmula en `Pet.cpp`, `Brain.cpp` o el motor gráfico, debes respetar estrictamente las invariantes biológicas, cognitivas y de memoria documentadas aquí.

---

## 1. Filosofía del Ritmo Biológico ("Human-Centric Pacing")

El Tamagotchi acompaña la vida real de una persona que trabaja, estudia y duerme. **Prohibido introducir tasas de drenaje que exijan atención cada pocos minutos o que congelen las estadísticas al dormir.**

### A. Ritmo Diurno (Vigilia: `lightsOn == true`)
- **Hambre**: Baja de `100%` a `0%` en **5.5 horas** (`19,800 s` / `~18.18% por hora`).
  - A las ~3.3 horas (`< 40%`) empieza a pensar en su pancho (`THOUGHT_FOOD`).
  - A las 5 horas está en zona crítica (`~9%`), exigiendo alimento.
- **Felicidad**: Baja de `100%` a `0%` en **8.0 horas** (`28,800 s` / `12.5% por hora`).
- **Energía**: Baja de `100%` a `0%` en **15.0 horas** (`54,000 s` / `~6.67% por hora`).
- **Tránsito Digestivo**: Tras comer (`feed()`), digiere entre `15 y 25 minutos` y genera 1 caca (máximo 3). Si el hambre cae por debajo de `15%`, el tránsito se detiene.

### B. Hábito Circadiano (22:00 hs a 04:00 AM)
- Si el reloj NTP está sincronizado y la hora local está entre las **22:00 y las 04:00 AM** (`Pet::isBedtimeHours()`), el Tamagotchi siente ganas de ir a dormir por costumbre (`THOUGHT_SLEEP` 🌙 y `driveSleep >= 75%`), **incluso si su barra de energía es alta**.
- **Regla al Apagar la Luz**: Siempre que el usuario apague la luz (`lightsOn = false`), la mascota entra en `PetState::Sleeping` sin importar cuánta energía tenga.
- **Despertar Brusco**: Encender la luz cuando estaba durmiendo con `< 75%` de energía la pone triste y penaliza el Apego (`trust - 10%`).

### C. Ventana Nocturna de 10 Horas vs. Abandono a las 12 Horas (`lightsOn == false`)
- **Sueño Continuo (Opción A)**: Mientras `lightsOn == false`, la mascota **permanece dormida (`PetState::Sleeping`)** aunque su energía ya haya llegado al `100%`. No debe despertarse sola a mitad de la noche.
- **Recuperación de Energía**: Sube de `0%` a `100%` en **6.5 horas** (`+15.38% por hora`).
- **La Foto a las 10 Horas de Sueño (Desayuno Saludable)**:
  Si se acostó al `100%`, tras **10 horas** con la luz apagada debe encontrarse con:
  - **Energía**: `100%`
  - **Hambre**: `~25%` (drena a `7.5% por hora` durante las primeras 10h).
  - **Felicidad**: `~45%` (drena a `5.5% por hora` durante las primeras 10h).
  - **Salud y Apego (`trust`)**: **`100%` intactos** (cero penalización durante las primeras 10 horas).
- **La Foto a las 12 Horas (Descuido y Miedo al Abandono)**:
  Pasadas las 10 horas (`hunger < 25%` o `unattendedSleepTimer > 10.5h`):
  - El drenaje de hambre sube a `10.0% por hora`, llegando a **`~5%` a las 12 horas**.
  - Superadas las ~11 horas sin atención (o `hunger <= 8%`), sufre **miedo de que su dueño no vuelva**: se hace una **caca por tiempo/susto** (`abandonmentPoopTriggered`), activa `THOUGHT_STRESS`, y pierde **Salud (`-15% por hora`)** y **Apego (`-6% por hora`)**.

### D. Paridad Total en `Deep Sleep` (`Pet::catchUpTime()`)
- El modo `Deep Sleep` (botón físico `BOOT_PIN`) ahorra batería pero **debe simular exactamente el mismo paso del tiempo** que si hubiera quedado encendido (ya sea con luz prendida o con luz apagada).
- `catchUpTime()` soporta hasta **48 horas (`172,800 s`)** y funciona tanto con hora NTP (`>= 1700000000`) como con el reloj RTC interno del ESP32 en modo **100% Offline**.

---

## 2. Arquitectura Cognitiva (`IBrain` y Polimorfismo)

1. **Prohibido `static_cast<Brain*>`**:
   `Pet` y `Game` solo se comunican mediante la interfaz `IBrain` (`src/tamagotchi/brain/IBrain.h`). Cualquier método nuevo de interacción neuronal debe declararse como `virtual` en `IBrain`.
2. **Separación de Conceptos (Ortogonalidad)**:
   - **ADN Visual (`dnaSeed`)**: Solo rota el matiz (Hue) de los sprites en `AssetManager.cpp`.
   - **Genoma (`PetGenome`)**: Rasgos biológicos innatos al salir del huevo (`metabolismRate`, `socialNeed`, `resilience`, `sleepPacing`).
   - **Arquetipo de Cerebro (`BrainArchetypes.h`)**: Topología y plasticidad de la red neuronal (`STD`, `LEARNER`, `DUMB`, `HYPER`, `FSM`), intercambiable en caliente vía `pet.selectBrainArchetype("...")` o comando Serial `BRAIN <TIPO>`.

---

## 3. Invariantes de Hardware y Memoria (ESP32-S3)

- **Pantalla**: ST7789 `172x320`, renderizada siempre sobre un `framebuffer` en RAM antes de volcarse por SPI (`game->flush(gfx)`).
- **Protección del Kernel Space (SRAM)**:
  - Las texturas PNG (`Texture::pixels`, `Texture::alpha`), el `backgroundBuffer` y los documentos JSON grandes (`SpiRamJsonDocument`) **deben alojarse siempre en PSRAM** usando `heap_caps_malloc(..., MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`.
  - Nunca usar `seek()` al descomprimir `.tar` en `SD_MMC`; avanzar siempre leyendo bloques para evitar corrupción del stream de la tarjeta SD.
