// src/tamagotchi/MessageManager.h
#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <SD_MMC.h>
#include <vector>
#include <memory>
#include "PetDef.h"

class MessageManager {
public:
    static MessageManager& getInstance() {
        static MessageManager instance;
        return instance;
    }

    void init() {
        SD_MMC.mkdir("/config");
        SD_MMC.mkdir("/config/dialogues");
    }

    void startPreload() {
        if (isPreloading || WiFi.status() != WL_CONNECTED) return;
        isPreloading = true;

        xTaskCreatePinnedToCore([](void* param) {
            MessageManager* mgr = (MessageManager*)param;
            mgr->preloadAllEmotionsWorker();
            mgr->isPreloading = false;
            vTaskDelete(NULL);
        }, "AiPreloadTask", 8192, this, 1, NULL, 0);
    }

    String getDialogue(const String& emotion) {
        String path = "/config/dialogues/" + emotion + ".txt";
        if (SD_MMC.exists(path)) {
            File f = SD_MMC.open(path, "r");
            if (f) {
                std::vector<String> lines;
                while (f.available()) {
                    String line = f.readStringUntil('\n');
                    line.trim();
                    if (line.length() > 0 && line.indexOf("<html") == -1) {
                        lines.push_back(line);
                    }
                }
                f.close();
                if (!lines.empty()) {
                    return sanitizeToAscii(lines[random(0, lines.size())]);
                }
            }
        }
        return getFallbackOffline(emotion);
    }

    void requestAiMessage(const String& mood, const String& reason) {
        lastMood = mood;
        lastReason = reason;

        Serial.println("\n--------------------------------------------------");
        Serial.printf("[AI-TRIGGER] Disparando pensamiento emocional: %s\n", lastMood.c_str());
        Serial.printf("[AI-TRIGGER] Motivo neuronal: %s\n", lastReason.c_str());

        if (WiFi.status() == WL_CONNECTED && !isPreloading) {
            xTaskCreatePinnedToCore([](void* param) {
                MessageManager* mgr = (MessageManager*)param;
                String fetched = mgr->fetchSinglePrompt(mgr->lastMood);
                fetched = mgr->sanitizeToAscii(fetched);
                if (fetched.length() > 0) {
                    mgr->pendingMessage = fetched;
                    mgr->saveToEmotionFile(mgr->lastMood, fetched);
                } else {
                    mgr->pendingMessage = mgr->getDialogue(mgr->lastMood);
                }
                mgr->pendingIsDream = false;
                mgr->messageReady = true;
                vTaskDelete(NULL);
            }, "AiSingleTask", 6144, this, 1, NULL, 0);
        } else {
            pendingMessage = getDialogue(mood);
            pendingIsDream = false;
            messageReady = true;
            Serial.printf("[AI-TRIGGER] (Modo SD) Frase seleccionada: \"%s\"\n", pendingMessage.c_str());
        }
        Serial.println("--------------------------------------------------");
    }

    void requestDreamMessage(PetStage stage, bool isRestless, bool hasAffection) {
        Serial.println("\n--------------------------------------------------");
        Serial.printf("[DREAM-TRIGGER] Disparando sueno subconsciente (Etapa: %s, Inquieto: %s, Afecto: %s)\n",
                      stageToString(stage).c_str(), isRestless ? "SI" : "NO", hasAffection ? "SI" : "NO");

        if (WiFi.status() == WL_CONNECTED && !isPreloading) {
            struct DreamTaskParams {
                MessageManager* mgr;
                PetStage stage;
                bool isRestless;
                bool hasAffection;
            };
            DreamTaskParams* params = new DreamTaskParams{this, stage, isRestless, hasAffection};

            xTaskCreatePinnedToCore([](void* p) {
                DreamTaskParams* dp = (DreamTaskParams*)p;
                String prompt = dp->mgr->buildDreamPrompt(dp->stage, dp->isRestless, dp->hasAffection);
                String fetched = dp->mgr->fetchPromptDirect(prompt);
                fetched = dp->mgr->sanitizeToAscii(fetched);

                if (fetched.length() > 0 && fetched.length() <= 80) {
                    dp->mgr->pendingMessage = fetched;
                } else {
                    dp->mgr->pendingMessage = dp->mgr->getDreamFallback(dp->stage, dp->isRestless, dp->hasAffection);
                }
                dp->mgr->pendingIsDream = true;
                dp->mgr->messageReady = true;
                Serial.printf("[DREAM-TRIGGER] Sueno sintetizado: \"%s\"\n", dp->mgr->pendingMessage.c_str());
                delete dp;
                vTaskDelete(NULL);
            }, "AiDreamTask", 6144, params, 1, NULL, 0);
        } else {
            pendingMessage = getDreamFallback(stage, isRestless, hasAffection);
            pendingIsDream = true;
            messageReady = true;
            Serial.printf("[DREAM-TRIGGER] (Modo Offline) Sueno: \"%s\"\n", pendingMessage.c_str());
        }
        Serial.println("--------------------------------------------------");
    }

    bool hasPendingMessage() const { return messageReady; }
    bool isPendingDream() const { return pendingIsDream; }

    String consumeMessage() {
        messageReady = false;
        return pendingMessage;
    }

    String getFallbackOffline(const String& mood) {
        if (mood == "happy" || mood == "LOVE") {
            static const char* f[] = {
                "gracias por cuidarme! te quiero mucho.", 
                "tu compania me alegra siempre el corazon!", 
                "siempre estoy muy contento de estar con vos!"
            };
            return sanitizeToAscii(f[random(0, 3)]);
        } else if (mood == "hungry" || mood == "eating" || mood == "HUNGRY") {
            static const char* f[] = {
                "mi pancita hace ruidos, tenia mucha hambre...", 
                "que rica estaba la comida! muchas gracias.", 
                "me llenaste la pancita, ahora estoy feliz!"
            };
            return sanitizeToAscii(f[random(0, 3)]);
        } else if (mood == "sick") {
            static const char* f[] = {
                "me duele la pancita y el cuerpo...", 
                "me siento debil, por favor dame medicina.", 
                "no me dejes solito sintiendome tan mal..."
            };
            return sanitizeToAscii(f[random(0, 3)]);
        } else if (mood == "dead") {
            static const char* f[] = {
                "por que me has descuidado tanto tiempo?", 
                "esperaba con ilusion que volvieras a cuidarme...", 
                "me quede sin fuerzas. siempre te extranare..."
            };
            return sanitizeToAscii(f[random(0, 3)]);
        } else if (mood == "sleeping") {
            static const char* f[] = {
                "tengo mucho sueno... que descanses bien zzz", 
                "buenas noches, que suenes lindo hasta manana!", 
                "a descansar un ratito largo y calentito..."
            };
            return sanitizeToAscii(f[random(0, 3)]);
        }
        return "explorando mi entorno con tranquilidad y calma.";
    }

    String getDreamFallback(PetStage stage, bool isRestless, bool hasAffection) {
        if (isRestless) {
            static const char* nightmares[] = {
                "mucho frio aqui... zzz",
                "la pancita suena... zzz",
                "demasiada luz... ay...",
                "sombras raras... mmh",
                "pesadez oscura... zzz"
            };
            return nightmares[random(0, 5)];
        }

        if (hasAffection && random(0, 100) < 50) {
            static const char* affection[] = {
                "manos tibias... en calma...",
                "no estoy solito... zzz",
                "caricias suaves... zzz",
                "siento tu calor... mmh"
            };
            return affection[random(0, 4)];
        }

        switch (stage) {
            case PetStage::Baby: {
                static const char* babyDreams[] = {
                    "tibio como el nido... zzz",
                    "cascaron suave... zzz",
                    "rodando en calma... zzz",
                    "rumores del huevo... mmh"
                };
                return babyDreams[random(0, 4)];
            }
            case PetStage::Child: {
                static const char* childDreams[] = {
                    "pasos chiquitos... zzz",
                    "era tan pequeno... zzz",
                    "un biberon grande... zzz",
                    "gateando lento... mmh"
                };
                return childDreams[random(0, 4)];
            }
            case PetStage::Adult: {
                static const char* adultDreams[] = {
                    "corriendo sin parar... zzz",
                    "saltando muy alto... zzz",
                    "jugando todo el dia... zzz",
                    "viejos paseos... mmh"
                };
                return adultDreams[random(0, 4)];
            }
            case PetStage::Senior:
            default: {
                static const char* seniorDreams[] = {
                    "tiempos lejanos... zzz",
                    "un recuerdo tibio... zzz",
                    "memorias doradas... mmh",
                    "descansando en paz... zzz"
                };
                return seniorDreams[random(0, 4)];
            }
        }
    }

    String sanitizeToAscii(const String& in) {
        String out = "";
        for (size_t i = 0; i < in.length(); i++) {
            unsigned char c = (unsigned char)in[i];
            if (c == 0xC3 && i + 1 < in.length()) {
                unsigned char next = (unsigned char)in[i + 1];
                i++;
                switch (next) {
                    case 0xA1: out += 'a'; break; // á
                    case 0xA9: out += 'e'; break; // é
                    case 0xAD: out += 'i'; break; // í
                    case 0xB3: out += 'o'; break; // ó
                    case 0xBA: out += 'u'; break; // ú
                    case 0xBC: out += 'u'; break; // ü
                    case 0xB1: out += 'n'; break; // ñ
                    case 0x81: out += 'a'; break; // Á
                    case 0x89: out += 'e'; break; // É
                    case 0x8D: out += 'i'; break; // Í
                    case 0x93: out += 'o'; break; // Ó
                    case 0x9A: out += 'u'; break; // Ú
                    case 0x91: out += 'n'; break; // Ñ
                    default: break;
                }
            } else if (c >= 32 && c <= 126) {
                if (c >= 'A' && c <= 'Z') {
                    out += (char)(c + 32);
                } else if (c != '"' && c != '\\') {
                    out += (char)c;
                }
            }
        }
        out.trim();

        // Límite ampliado: Permite oraciones completas acordes a las 5 líneas de MessagePopup (160x68px)
        if (out.length() > 95) {
            int cutIdx = 90;
            int lastSpace = out.lastIndexOf(' ', cutIdx);
            if (lastSpace > 45) {
                out = out.substring(0, lastSpace) + "...";
            } else {
                out = out.substring(0, cutIdx) + "...";
            }
        }
        return out;
    }

private:
    MessageManager() = default;
    bool isPreloading = false;
    bool messageReady = false;
    bool pendingIsDream = false;
    String pendingMessage = "";
    String lastMood = "happy";
    String lastReason = "Ninguno";

    String buildPromptForEmotion(const String& emotion) {
        String p = "Responde en espanol en minusculas como mascota virtual tamagotchi. Maximo 12 palabras. Estrictamente sin tildes ni enies. Sin comillas. ";
        if (emotion == "happy") p += "Di algo muy alegre y carinoso hacia tu dueno.";
        else if (emotion == "idle") p += "Di algo tranquilo, neutro y agradecido de estar con tu dueno.";
        else if (emotion == "sick") p += "Quejate con ternura de que te duele el cuerpo y necesitas medicina.";
        else if (emotion == "dead") p += "Di una frase triste preguntando con pena por que me has descuidado.";
        else if (emotion == "eating") p += "Agradece por la comida rica que acabas de comer.";
        else if (emotion == "sleeping") p += "Di que tienes mucho sueno y te vas a dormir.";
        return p;
    }

    String buildDreamPrompt(PetStage stage, bool isRestless, bool hasAffection) {
        String p = "Responde en espanol en minusculas como mascota sonando dormida. Maximo 5 palabras telegraficas con puntos suspensivos terminando en zzz. Estrictamente sin tildes ni enies. Sin comillas. ";
        if (isRestless) {
            p += "Expresa pesadilla o malestar por hambre o frio.";
        } else if (hasAffection) {
            p += "Expresa recuerdo calido de caricias y afecto.";
        } else if (stage == PetStage::Baby) {
            p += "Recuerda cuando eras un huevo tibio y protegido.";
        } else if (stage == PetStage::Child) {
            p += "Recuerda cuando eras un bebe chiquito.";
        } else {
            p += "Recuerda correr y jugar en tu infancia.";
        }
        return p;
    }

    String urlEncode(const String& str) {
        String encoded = "";
        char hex[4];
        for (size_t i = 0; i < str.length(); i++) {
            char c = str[i];
            if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded += c;
            } else if (c == ' ') {
                encoded += "%20";
            } else {
                snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
                encoded += hex;
            }
        }
        return encoded;
    }

    String fetchPromptDirect(const String& prompt) {
        String encoded = urlEncode(prompt);
        String url = "https://text.pollinations.ai/" + encoded;

        for (int intento = 1; intento <= 2; intento++) {
            std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure);
            client->setInsecure();

            HTTPClient http;
            http.setTimeout(10000); 
            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64)");

            if (http.begin(*client, url)) {
                int code = http.GET();
                if (code == 200) {
                    String body = http.getString();
                    body.trim();
                    http.end();

                    if (body.indexOf("<html") != -1 || body.indexOf("<!DOCTYPE") != -1) {
                        return "";
                    }

                    if (body.startsWith("\"") && body.endsWith("\"")) {
                        body = body.substring(1, body.length() - 1);
                    }
                    return body;
                }
                http.end();
            }
            if (intento < 2) delay(1000);
        }
        return "";
    }

    String fetchSinglePrompt(const String& emotion) {
        String prompt = buildPromptForEmotion(emotion);
        return fetchPromptDirect(prompt);
    }

    void saveToEmotionFile(const String& emotion, const String& text) {
        String path = "/config/dialogues/" + emotion + ".txt";
        File f = SD_MMC.open(path, "a");
        if (f) {
            f.println(text);
            f.close();
            Serial.printf("[AI-CACHE] Frase guardada en %s: \"%s\"\n", path.c_str(), text.c_str());
        }
    }

    void preloadAllEmotionsWorker() {
        Serial.println("\n==================================================");
        Serial.println("[AI-CACHE] Iniciando descarga de frases por emocion...");
        Serial.println("==================================================");

        const char* emociones[] = { "happy", "idle", "sick", "dead", "eating", "sleeping" };

        for (int i = 0; i < 6; i++) {
            const char* emo = emociones[i];
            Serial.printf("[AI-CACHE] [%d/6] Solicitando a Pollinations emocion: '%s'...\n", i + 1, emo);
            
            String res = fetchSinglePrompt(emo);
            res = sanitizeToAscii(res);
            if (res.length() > 0) {
                saveToEmotionFile(emo, res);
            } else {
                Serial.printf("[AI-CACHE] Fallo al recibir '%s'. Se mantendra fallback en SD.\n", emo);
            }
            delay(1200);
        }

        Serial.println("==================================================");
        Serial.println("[AI-CACHE] Descarga finalizada. Guardado en SD.");
        Serial.println("[AI-CACHE] Apagando antena WiFi para maximo ahorro de bateria.");
        Serial.println("==================================================\n");

        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
    }
};
