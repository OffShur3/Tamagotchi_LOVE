// src/tamagotchi/ParkScene.h
#pragma once
#include "../render/Scene.h"
#include "../render/RenderObject.h"
#include "../render/Sprite.h"
#include "../render/TextUtil.h"
#include "../render/Blend.h"
#include "../assets/AssetManager.h"
#include "../ColorTuner.h"
#include "Pet.h"
#include "OrphanageManager.h" 
#include <vector>
#include <memory>
#include <functional>
#include <math.h>
#include <SD_MMC.h>
#include <ArduinoJson.h>

class ParkScene : public Scene {
public:
    using SelectPetCallback  = std::function<void(const String& petId)>;
    using RenamePetCallback  = std::function<void(const String& petId, const String& currentName)>;
    using BackCallback       = std::function<void()>;

    struct PlazaActor {
        String id;
        String name;
        String species;
        PetStage stage = PetStage::Baby;
        uint32_t dnaSeed = 0;
        String personality = "Normal";
        String brainType = "STD";
        float hunger = 100.0f;
        float trust = 5.0f;

        float x = 60.0f;
        float y = 110.0f;
        float targetX = 60.0f;
        float targetY = 110.0f;
        float waitTimer = 0.0f;
        float walkSpeed = 12.0f; // Asíncrona por cada Tama
        bool isMoving = false;
        bool flipX = false;
        bool arrivedInFormation = false;

        float animTimer = 0.0f;
        float stepInterval = 0.35f; // Asíncrono
        uint8_t currentFrame = 0;
        uint8_t idleFrames = 1;
        uint8_t walkFrames = 1;

        std::shared_ptr<Texture> idleTex = nullptr;
        std::shared_ptr<Texture> walkTex = nullptr;
    };

    ParkScene(SelectPetCallback onSelect, RenamePetCallback onRename, BackCallback onBack)
        : onSelectPet(onSelect), onRenamePet(onRename), onBackToMain(onBack) {}

    void enter() override {
        formationMode = false;
        selectedIndex = -1;
        activePetId = Pet::getActivePetId();
        // Cargar texturas dedicadas de la Plaza si existen en la SD
        bgPlazaTex = AssetManager::getInstance().getTexture("/tama/ui/Plaza/bg_plaza.png");
        iconsPlazaTex = AssetManager::getInstance().getTexture("/tama/ui/Plaza/icons_plaza.png");
        camX = targetCamX = 74.0f;
        loadActorsFromSD();

        plazaRenderer = std::make_shared<PlazaRenderObject>(*this);
        addObject(plazaRenderer);
        Serial.printf("[PLAZA] Abriendo Plaza Mii con %d Tamas conviviendo.\n", (int)actors.size());
    }

    void exit() override {
        clearObjects();
        actors.clear();
        bgPlazaTex = nullptr;
        iconsPlazaTex = nullptr;
        plazaRenderer = nullptr;
        AssetManager::getInstance().clearUnused();
    }

    void loadActorsFromSD() {
        actors.clear();
        auto ids = Pet::listSavedPetIds();

        for (size_t i = 0; i < ids.size() && i < 6; ++i) {
            String path = "/config/pets/" + ids[i] + ".json";
            File f = SD_MMC.open(path, "r");
            if (!f) continue;

            StaticJsonDocument<256> filter;
            filter["id"]         = true;
            filter["name"]       = true;
            filter["species"]    = true;
            filter["stage"]      = true;
            filter["dna"]        = true;
            filter["g_tag"]      = true;
            filter["brain_type"] = true;
            filter["hunger"]     = true;
            filter["trust"]      = true;

            DynamicJsonDocument doc(2048);
            DeserializationError err = deserializeJson(doc, f, DeserializationOption::Filter(filter));
            f.close();
            if (err) {
                Serial.printf("[PLAZA] Error parseando %s: %s\n", path.c_str(), err.c_str());
                continue;
            }

            PlazaActor act;
            act.id          = ids[i];
            act.species     = doc["species"]    | "tiernito";
            String defPlazaName = "Tama #" + String((int)(i + 1));
            act.name        = doc["name"]       | defPlazaName.c_str();
            act.stage       = static_cast<PetStage>(doc["stage"] | 1);
            act.dnaSeed     = doc["dna"]        | 0;
            act.personality = doc["g_tag"]      | "Incubando...";
            act.brainType   = doc["brain_type"] | "STD";
            act.hunger      = doc["hunger"]     | 100.0f;
            act.trust       = doc["trust"]      | 5.0f;

            // Posición inicial aleatoria en la plaza
            int16_t slotX, slotY;
            getFormationSlot(i, slotX, slotY);
            act.x = (float)random(12, 116);
            act.y = (float)random(52, 188);
            act.targetX = (float)random(12, 116);
            act.targetY = (float)random(52, 188);
            act.waitTimer = (float)random(10, 45) / 10.0f;
            act.walkSpeed = 10.0f + (float)random(0, 6); // Entre 10 y 16 px/s (caminata suave)
            act.stepInterval = 0.30f + (float)random(0, 15) / 100.0f;
            act.arrivedInFormation = false;

            // Cargar texturas con su ADN visual único
            AssetManager::getInstance().setActiveDnaSeed(act.dnaSeed);
            String idlePath = Pet::resolveSpritePathFor(act.species, act.stage, "idle");
            String walkPath = Pet::resolveSpritePathFor(act.species, act.stage, "walk");

            act.idleTex = AssetManager::getInstance().getTexture(idlePath.c_str());
            act.walkTex = AssetManager::getInstance().getTexture(walkPath.c_str());

            // Si aun no dibujaste walk.png, usa el idle de su etapa correspondiente
            if (!act.walkTex && act.idleTex) {
                act.walkTex = act.idleTex;
            }

            if (act.idleTex) act.idleFrames = max(1, act.idleTex->width / 48);
            act.walkFrames = act.walkTex ? max(1, act.walkTex->width / 48) : act.idleFrames;

            actors.push_back(act);
        }
    }

    void getFormationSlot(size_t index, int16_t& outX, int16_t& outY) const {
        // Centrados en la plaza completa (X: 80 a 240)
        int col = index % 3;
        int row = index / 3;
        outX = 82 + (col * 54);
        outY = 72 + (row * 90);
    }

    void toggleWhistleFormation() {
        formationMode = !formationMode;
        if (formationMode) {
            targetCamX = 74.0f; // Centrar cámara en la formación automáticamente
            Serial.println("[PLAZA] ¡PRIIII! Silbato activado: Centrando camara y alineando Tamas.");
            for (size_t i = 0; i < actors.size(); ++i) {
                int16_t sx, sy;
                getFormationSlot(i, sx, sy);
                actors[i].targetX = (float)sx;
                actors[i].targetY = (float)sy;
                actors[i].isMoving = true;
                actors[i].arrivedInFormation = false;
            }
        } else {
            Serial.println("[PLAZA] Rompan filas: Caminata libre por la Plaza.");
            for (size_t i = 0; i < actors.size(); ++i) {
                actors[i].targetX = (float)random(10, 116);
                actors[i].targetY = (float)random(48, 190);
                actors[i].waitTimer = (float)random(2, 15) / 10.0f;
            }
        }
    }

    void update(float dt) override {
        // Desplazamiento suave de la cámara hacia la posición objetivo
        camX += (targetCamX - camX) * (1.0f - expf(-14.0f * dt));
        camX = constrain(camX, 0.0f, 148.0f); // 320 - 172 = 148 máx scroll

        for (size_t i = 0; i < actors.size(); ++i) {
            auto& a = actors[i];

            float dx = a.targetX - a.x;
            float dy = a.targetY - a.y;
            float dist = sqrtf(dx * dx + dy * dy);

            if (formationMode) {
                if (!a.arrivedInFormation && dist > 1.5f) {
                    a.isMoving = true;
                    float runSpeed = 40.0f; // Paso constante y ordenado hacia su lugar
                    a.x += (dx / dist) * runSpeed * dt;
                    a.y += (dy / dist) * runSpeed * dt;
                    a.flipX = (dx > 0.5f);
                } else {
                    // Posición clavada exacta: Cero movimientos por código, solo frame animation
                    a.x = a.targetX;
                    a.y = a.targetY;
                    a.isMoving = false;
                    a.flipX = false;
                    a.arrivedInFormation = true;
                }
            } else {
                a.arrivedInFormation = false;
                if (a.waitTimer > 0.0f) {
                    a.waitTimer -= dt;
                    a.isMoving = false;
                } else if (dist > 2.0f) {
                    a.isMoving = true;
                    float speed = (a.stage == PetStage::Egg) ? 0.0f : a.walkSpeed;
                    if (speed > 0.0f) {
                        a.x += (dx / dist) * speed * dt;
                        a.y += (dy / dist) * speed * dt;
                        a.flipX = (dx > 0.5f);
                    } else {
                        a.isMoving = false;
                        a.waitTimer = 4.0f;
                    }
                } else {
                    a.isMoving = false;
                    a.flipX = false;
                    a.waitTimer = 2.0f + (float)random(0, 40) / 10.0f; // Pausas de 2 a 6 segundos
                    a.targetX = (float)random(16, 256); // Paseo a lo largo de los 320 px del parque
                    a.targetY = (float)random(54, 200);
                }
            }

            // Animación por fotogramas pausada y natural
            float frameInterval = a.isMoving ? a.stepInterval : 0.40f;
            a.animTimer += dt;
            if (a.animTimer >= frameInterval) {
                a.animTimer = 0.0f;
                uint8_t maxF = a.isMoving ? a.walkFrames : a.idleFrames;
                a.currentFrame = (a.currentFrame + 1) % max(1, (int)maxF);
            }
        }
    }

    void onTouch(uint16_t tx, uint16_t ty) override {
        if (millis() - lastTouchTime < 220) return;
        lastTouchTime = millis();

        // 1. Si hay una ficha de inspección de Tama abierta
        // 0. Interacción con el Modal del Orfanato
        if (showOrphanageModal) {
            // Botón [X] cerrar (X: [134, 164], Y: [52, 82])
            if (tx >= 130 && tx <= 164 && ty >= 50 && ty <= 82) {
                showOrphanageModal = false;
                orphanCandidateSelected = -1;
                return;
            }

            if (!OrphanageManager::getInstance().isReady()) {
                // Tocó el botón [ ENTENDIDO ]
                if (ty >= 180 && ty <= 225 && tx >= 20 && tx <= 152) {
                    showOrphanageModal = false;
                }
                return;
            } else {
                // Si ya llegaron los niños: tocar un candidato
                const auto& list = OrphanageManager::getInstance().getCandidates();
                int16_t startY = 94;
                for (size_t i = 0; i < list.size() && i < 3; ++i) {
                    int16_t cy = startY + (i * 38);
                    if (ty >= cy && ty <= cy + 34 && tx >= 16 && tx <= 156) {
                        orphanCandidateSelected = (int)i;
                        return;
                    }
                }
                // Botón [ ADOPTAR ]
                if (orphanCandidateSelected >= 0 && ty >= 210 && ty <= 248 && tx >= 18 && tx <= 154) {
                    if (actors.size() < 6) {
                        String newId;
                        if (OrphanageManager::getInstance().adoptCandidate(orphanCandidateSelected, newId)) {
                            showOrphanageModal = false;
                            orphanCandidateSelected = -1;
                            loadActorsFromSD();
                            Serial.printf("[ORFANATO] ¡Adoptado con exito! ID: %s\n", newId.c_str());
                        }
                    }
                    return;
                }
            }
            return;
        }

        if (selectedIndex >= 0 && selectedIndex < (int)actors.size()) {
            // Botón [X] cerrar ficha (esquina sup derecha del modal)
            if (tx >= 134 && tx <= 164 && ty >= 56 && ty <= 86) {
                selectedIndex = -1;
                return;
            }
            // Botón 1: [ LLEVAR DE PASEO ] Y: [176, 208]
            if (tx >= 20 && tx <= 152 && ty >= 176 && ty <= 208) {
                String chosenId = actors[selectedIndex].id;
                selectedIndex = -1;
                if (onSelectPet) onSelectPet(chosenId);
                return;
            }
            // Botón 2: [ CAMBIAR NOMBRE ] Y: [214, 246]
            if (tx >= 20 && tx <= 152 && ty >= 214 && ty <= 246) {
                String chosenId = actors[selectedIndex].id;
                String curName  = actors[selectedIndex].name;
                selectedIndex = -1;
                if (onRenamePet) onRenamePet(chosenId, curName);
                return;
            }
            return;
        }

        // 2. Iconos Minimalistas Superiores:
        // A. Flecha Volver estilo Wii (Arriba a la izquierda: X: [6, 36], Y: [6, 36])
        if (tx <= 40 && ty <= 40) {
            if (onBackToMain) onBackToMain();
            return;
        }

        // B. Silbato de la Wii (Arriba a la derecha: X: [132, 168], Y: [6, 40])
        if (tx >= 128 && ty <= 42) {
            toggleWhistleFormation();
            return;
        }

        // 3. Icono Inferior: Botón Orfanato / Adopción (Abajo a la derecha: X: [126, 168], Y: [276, 316])
        if (ty >= 270 && tx >= 120) {
            showOrphanageModal = true;
            return;
        }

        // 4. Toque sobre cualquiera de los Tamas en la Plaza (compensando la cámara camX)
        for (int i = (int)actors.size() - 1; i >= 0; --i) {
            int16_t screenX = (int16_t)(actors[i].x - camX);
            int16_t screenY = (int16_t)actors[i].y;
            if (tx >= screenX && tx <= screenX + 48 && ty >= screenY && ty <= screenY + 48) {
                selectedIndex = i;
                Serial.printf("[PLAZA] Inspeccionando a %s (%s)\n", actors[i].name.c_str(), actors[i].id.c_str());
                return;
            }
        }

        // 5. Desplazamiento horizontal de cámara con un toque/arrastre
        if (tx < 50) {
            targetCamX = max(0.0f, targetCamX - 45.0f);
        } else if (tx > 122) {
            targetCamX = min(148.0f, targetCamX + 45.0f);
        }
    }

private:
    SelectPetCallback onSelectPet = nullptr;
    RenamePetCallback onRenamePet = nullptr;
    BackCallback onBackToMain = nullptr;

    std::vector<PlazaActor> actors;
    String activePetId = "pet_001";
    bool formationMode = false;
    int selectedIndex = -1;
    bool showOrphanageModal = false;
    int orphanCandidateSelected = -1;
    uint32_t lastTouchTime = 0;

    // Desplazamiento horizontal de cámara (Fondo de 320x320 en pantalla de 172x320)
    float camX = 74.0f; // Centrado por defecto (320 - 172) / 2
    float targetCamX = 74.0f;
    bool isDraggingCam = false;
    int16_t lastDragX = 0;

    std::shared_ptr<Texture> bgPlazaTex = nullptr;
    std::shared_ptr<Texture> iconsPlazaTex = nullptr;

    class PlazaRenderObject : public RenderObject {
    public:
        PlazaRenderObject(ParkScene& p) : parent(p) {
            layer = RenderLayer::UI;
        }

        Size2 getSize() const override { return {172, 320}; }

        void draw(RenderContext& ctx) override {
            // 1. Fondo de la Plaza (bg_plaza.png con scroll horizontal o fallback procedural)
            drawPlazaBackground(ctx);

            // 2. Dibujar todos los Tamas en la Plaza desplazados por camX
            for (size_t i = 0; i < parent.actors.size(); ++i) {
                drawActor(ctx, parent.actors[i]);
            }

            // 3. Iconos Minimalistas Superiores e Inferiores (Flotantes)
            drawMinimalistControls(ctx);

            // 4. Ficha de Inspección si se tocó un Tama
            if (parent.selectedIndex >= 0 && parent.selectedIndex < (int)parent.actors.size()) {
                drawInspectModal(ctx, parent.actors[parent.selectedIndex]);
            }

            // 5. Modal del Orfanato
            if (parent.showOrphanageModal) {
                drawOrphanageModal(ctx);
            }
        }

    private:
        ParkScene& parent;

        void drawMiiPlazaFloor(RenderContext& ctx) {
            // OPTIMIZACIÓN CLAVE: Colores precalculados UNA sola vez fuera del bucle
            static const uint16_t tileColors[4] = { 0xEF7D, 0xE79C, 0xDF7E, 0xF79B };
            uint16_t cGrout = colorTunerCorrect565(DOMAIN_PLAZA, 0xD6BA);
            uint16_t cTiles[4];
            for (int i = 0; i < 4; ++i) cTiles[i] = colorTunerCorrect565(DOMAIN_PLAZA, tileColors[i]);

            // Solo recorre la zona de la plaza (Y: 34 a 274), omitiendo las barras superior e inferior
            for (int16_t y = 34; y < 274; ++y) {
                int16_t yShift = (y >> 3);
                uint16_t* rowPtr = &ctx.framebuffer[y * ctx.width];
                for (int16_t x = 0; x < ctx.width; ++x) {
                    int16_t u = x + yShift;
                    int16_t v = y - (x >> 3);
                    int16_t modU = ((u % 22) + 22) % 22;
                    int16_t modV = ((v % 22) + 22) % 22;

                    uint16_t c = cGrout;
                    if (modU >= 2 && modV >= 2) {
                        int tileIdx = (((u / 22) + (v / 22)) & 3);
                        c = cTiles[tileIdx];
                    }
                    rowPtr[x] = c;
                }
            }
        }

        void drawPlazaBackground(RenderContext& ctx) {
            if (parent.bgPlazaTex && parent.bgPlazaTex->pixels) {
                // Si existe /tama/ui/Plaza/bg_plaza.png, renderizar con scroll horizontal de cámara
                int16_t texW = parent.bgPlazaTex->width;
                int16_t texH = parent.bgPlazaTex->height;
                int16_t startSrcX = (int16_t)parent.camX;

                for (int16_t y = 0; y < ctx.height; ++y) {
                    int16_t srcY = (y * texH) / ctx.height;
                    if (srcY >= texH) srcY = texH - 1;
                    uint16_t* srcRow = &parent.bgPlazaTex->pixels[srcY * texW];
                    uint16_t* dstRow = &ctx.framebuffer[y * ctx.width];

                    for (int16_t x = 0; x < ctx.width; ++x) {
                        int16_t srcX = startSrcX + x;
                        if (srcX >= texW) srcX = texW - 1;
                        dstRow[x] = srcRow[srcX];
                    }
                }
            } else {
                // Fallback optimizado al piso procedural
                drawMiiPlazaFloor(ctx);
            }
        }

        void drawMinimalistControls(RenderContext& ctx) {
            // A. Botón Volver (Flecha estilo Wii retro minimalista)
            drawWiiRoundButton(ctx, 8, 8, 24, 0x4903, 0xFFFF, "<");

            // B. Botón Silbato de la Wii (Arriba a la derecha)
            uint16_t whistleCol = parent.formationMode ? 0x54A8 : 0x4903;
            drawWiiWhistleIcon(ctx, ctx.width - 32, 8, whistleCol);

            // C. Botón Orfanato flotante en la esquina inferior derecha
            drawWiiRoundButton(ctx, ctx.width - 34, ctx.height - 36, 26, 0x54A8, 0xFFFF, "+");
        }

        void drawWiiRoundButton(RenderContext& ctx, int16_t bx, int16_t by, int16_t size, uint16_t bgCol, uint16_t fgCol, const char* label) {
            uint16_t cBorder = colorTunerCorrect565(DOMAIN_HUD, 0x2104);
            int16_t r = size / 2;
            int16_t cx = bx + r;
            int16_t cy = by + r;

            for (int16_t y = -r; y <= r; ++y) {
                int16_t py = cy + y;
                if (py < 0 || py >= ctx.height) continue;
                for (int16_t x = -r; x <= r; ++x) {
                    int16_t px = cx + x;
                    if (px < 0 || px >= ctx.width) continue;
                    int d2 = x * x + y * y;
                    if (d2 <= r * r) {
                        uint16_t c = (d2 >= (r - 1) * (r - 1)) ? cBorder : bgCol;
                        ctx.framebuffer[py * ctx.width + px] = c;
                    }
                }
            }
            TextUtil::drawWrappedText(ctx, cx - 3, cy - 4, label, fgCol, 12, 10);
        }

        void drawWiiWhistleIcon(RenderContext& ctx, int16_t bx, int16_t by, uint16_t fillCol) {
            // Ícono vectorial de silbato estilo Canal Mii
            uint16_t cBorder = 0x2104;
            for (int16_t y = 0; y < 24; ++y) {
                for (int16_t x = 0; x < 24; ++x) {
                    // Boquilla y cilindro de aire del silbato
                    bool isBody = ((x >= 8 && x <= 20 && y >= 8 && y <= 20) || (x >= 2 && x <= 10 && y >= 11 && y <= 16));
                    bool isBorder = isBody && (x == 2 || x == 20 || y == 8 || y == 20 || y == 11 || y == 16);
                    if (isBody) {
                        ctx.drawPixel(bx + x, by + y, isBorder ? cBorder : fillCol);
                    }
                }
            }
            // Agujero de sonido
            ctx.drawPixel(bx + 12, by + 13, 0xFFFF);
            ctx.drawPixel(bx + 13, by + 13, 0xFFFF);
        }

        void drawActor(RenderContext& ctx, const PlazaActor& a) {
            std::shared_ptr<Texture> tex = a.isMoving ? a.walkTex : a.idleTex;
            if (!tex) tex = a.idleTex;
            if (!tex || !tex->pixels) return;

            // Coordenada en pantalla compensando el scroll horizontal de la cámara
            int16_t ax = (int16_t)(a.x - parent.camX);
            // Cero rebote en formación, y solo se dibuja si cae dentro de la pantalla visible
            int16_t bounceY = (a.isMoving && !parent.formationMode && (a.currentFrame % 2 == 1)) ? -2 : 0;
            int16_t ay = (int16_t)a.y + bounceY;

            if (ax < -48 || ax >= ctx.width) return; // Frustum culling (ahorra CPU fuera de pantalla)

            // Sombra ovalada ultrarrápida sin operaciones de coma flotante
            uint16_t shadowCol = colorTunerCorrect565(DOMAIN_WORLD, 0x94B2);
            int16_t feetY = (int16_t)a.y + 44;
            for (int16_t sy = -2; sy <= 2; ++sy) {
                int16_t py = feetY + sy;
                if (py < 34 || py >= 274) continue;
                for (int16_t sx = -10; sx <= 10; ++sx) {
                    if ((sx * sx) + (sy * sy * 25) <= 100) {
                        ctx.drawPixel(ax + 24 + sx, py, shadowCol);
                    }
                }
            }

            // Dibujar frame 48x48 (escala 1x para que entren varios en la Plaza)
            int16_t srcOffsetX = a.currentFrame * 48;
            for (int16_t dy = 0; dy < 48; ++dy) {
                int16_t py = ay + dy;
                if (py < 34 || py >= 274) continue;
                for (int16_t dx = 0; dx < 48; ++dx) {
                    int16_t px = ax + dx;
                    if (px < 0 || px >= ctx.width) continue;

                    int16_t srcX = srcOffsetX + (a.flipX ? (47 - dx) : dx);
                    if (srcX >= tex->width || dy >= tex->height) continue;

                    uint32_t idx = dy * tex->width + srcX;
                    uint8_t alpha = tex->alpha ? tex->alpha[idx] : 255;
                    if (alpha == 0) continue;

                    uint16_t col = tex->pixels[idx];
                    if (alpha == 255) {
                        ctx.framebuffer[py * ctx.width + px] = col;
                    } else {
                        ctx.framebuffer[py * ctx.width + px] = blendRGB565(ctx.framebuffer[py * ctx.width + px], col, alpha);
                    }
                }
            }

            // Etiqueta de nombre e indicador si es la mascota activa
            bool isActive = (a.id == parent.activePetId);
            String label = a.name.substring(0, 8);
            int16_t tw = label.length() * 6;
            int16_t tx = ax + (48 - tw) / 2;
            int16_t ty = (int16_t)a.y + 48;

            if (ty + 9 < 274) {
                uint16_t tagBg = isActive ? colorTunerCorrect565(DOMAIN_POPUP_DAY, 0xCE40)
                                          : colorTunerCorrect565(DOMAIN_POPUP_DAY, 0x4903);
                for (int16_t by = -1; by <= 8; ++by) {
                    for (int16_t bx = -2; bx <= tw + 1; ++bx) {
                        ctx.drawPixel(tx + bx, ty + by, tagBg);
                    }
                }
                TextUtil::drawWrappedText(ctx, tx, ty, label.c_str(), 0xFFFF, tw + 6, 10);
            }
        }

        void drawOrphanageModal(RenderContext& ctx) {
            int16_t mx = 10, my = 52, mw = 152, mh = 206;
            uint16_t cBorder = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0x4903);
            uint16_t cBody   = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0xF6FA);
            uint16_t cAccent = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0x9000);
            uint16_t cGold   = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0xCE40);

            for (int16_t y = 0; y < mh; ++y) {
                for (int16_t x = 0; x < mw; ++x) {
                    uint16_t col = (x < 2 || x >= mw - 2 || y < 2 || y >= mh - 2) ? cBorder : cBody;
                    ctx.drawPixel(mx + x, my + y, col);
                }
            }

            drawPillButton(ctx, mx + mw - 24, my + 6, 18, 18, 0xF800, 0xFFFF, "X");
            TextUtil::drawWrappedText(ctx, mx + 10, my + 10, "ORFANATO TAMA", cAccent, mw - 38, 12);

            if (!OrphanageManager::getInstance().isReady()) {
                int remSec = OrphanageManager::getInstance().getSecondsRemaining();
                String msg = "Los ninos del orfanato\ntodavia estan en camino.\n\nLlegaran en aprox:\n  " +
                             String(remSec) + " segundos.\n\nPor favor espera unos\nmomentos a que lleguen.";
                TextUtil::drawWrappedText(ctx, mx + 10, my + 32, msg.c_str(), cBorder, mw - 20, 110);
                drawPillButton(ctx, mx + 14, my + 148, mw - 28, 30, cGold, 0xFFFF, "ENTENDIDO");
            } else {
                const auto& list = OrphanageManager::getInstance().getCandidates();
                TextUtil::drawWrappedText(ctx, mx + 10, my + 26, "Elige a quien adoptar:", cBorder, mw - 20, 12);

                int16_t startY = my + 42;
                for (size_t i = 0; i < list.size() && i < 3; ++i) {
                    int16_t cy = startY + (i * 38);
                    bool isSel = (parent.orphanCandidateSelected == (int)i);
                    uint16_t rowBorder = isSel ? 0xF800 : cBorder;
                    uint16_t rowBg = isSel ? 0xFFE0 : 0xFFFF;

                    for (int16_t by = 0; by < 34; ++by) {
                        for (int16_t bx = 0; bx < mw - 16; ++bx) {
                            uint16_t c = (bx == 0 || bx == mw - 17 || by == 0 || by == 33) ? rowBorder : rowBg;
                            ctx.drawPixel(mx + 8 + bx, cy + by, c);
                        }
                    }

                    String title = list[i].name + " (" + list[i].species + " " + stageToString(list[i].stage) + ")";
                    String desc  = list[i].genome.personalityTag + " | SNN: " + list[i].brainType;
                    TextUtil::drawWrappedText(ctx, mx + 12, cy + 4, title.c_str(), cAccent, mw - 24, 11);
                    TextUtil::drawWrappedText(ctx, mx + 12, cy + 18, desc.c_str(), cBorder, mw - 24, 11);
                }

                if (parent.orphanCandidateSelected >= 0) {
                    drawPillButton(ctx, mx + 10, my + mh - 36, mw - 20, 28, 0x54A8, 0xFFFF, "ADOPTAR");
                }
            }
        }

        void drawInspectModal(RenderContext& ctx, const PlazaActor& a) {
            int16_t mx = 10, my = 54, mw = 152, mh = 202;
            uint16_t cBorder = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0x4903);
            uint16_t cBody   = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0xF6FA);
            uint16_t cAccent = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0x9000);

            for (int16_t y = 0; y < mh; ++y) {
                for (int16_t x = 0; x < mw; ++x) {
                    uint16_t col = (x < 2 || x >= mw - 2 || y < 2 || y >= mh - 2) ? cBorder : cBody;
                    ctx.drawPixel(mx + x, my + y, col);
                }
            }

            // Botón cerrar [X]
            drawPillButton(ctx, mx + mw - 24, my + 6, 18, 18, 0xF800, 0xFFFF, "X");

            TextUtil::drawWrappedText(ctx, mx + 10, my + 10, a.name.c_str(), cAccent, mw - 38, 12);

            String info = "Raza: " + a.species +
                          "\nEtapa: " + stageToString(a.stage) +
                          "\nADN: " + a.personality +
                          "\nMente: " + a.brainType +
                          "\nHambre: " + String((int)a.hunger) + "%" +
                          "\nApego: " + String((int)a.trust) + "%";
            TextUtil::drawWrappedText(ctx, mx + 10, my + 28, info.c_str(), cBorder, mw - 20, 90);

            bool isAlreadyActive = (a.id == parent.activePetId);
            drawPillButton(ctx, mx + 10, my + 122, mw - 20, 30,
                           isAlreadyActive ? 0x31A6 : 0x54A8, 0xFFFF,
                           isAlreadyActive ? "SEGUIR PASEANDO" : "LLEVAR DE PASEO");

            drawPillButton(ctx, mx + 10, my + 160, mw - 20, 30, 0xCE40, 0xFFFF, "CAMBIAR NOMBRE");
        }

        void drawPillButton(RenderContext& ctx, int16_t bx, int16_t by, int16_t bw, int16_t bh,
                            uint16_t fillRaw, uint16_t textCol, const char* label) {
            uint16_t cBorder = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0x2104);
            uint16_t cFill   = colorTunerCorrect565(DOMAIN_POPUP_DAY, fillRaw);

            for (int16_t y = 0; y < bh; ++y) {
                for (int16_t x = 0; x < bw; ++x) {
                    uint16_t col = (x == 0 || x == bw - 1 || y == 0 || y == bh - 1) ? cBorder : cFill;
                    ctx.drawPixel(bx + x, by + y, col);
                }
            }
            int16_t tw = strlen(label) * 6;
            int16_t tx = bx + (bw - tw) / 2;
            int16_t ty = by + (bh - 7) / 2;
            TextUtil::drawWrappedText(ctx, tx, ty, label, textCol, bw, bh);
        }
    };

    std::shared_ptr<PlazaRenderObject> plazaRenderer = nullptr;
};
