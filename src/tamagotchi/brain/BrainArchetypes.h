// src/tamagotchi/brain/BrainArchetypes.h
#pragma once
#include "Brain.h"
#include "SimpleReactiveBrain.h"
#include <math.h>

/**
 * 1. SLUGGISH BRAIN ("Cerebro Tonto / Letárgico / Con Lag Mental")
 * - Umbrales altos y mucha fuga de potencial (le cuesta "cargar" una idea).
 * - Pérdida de neurotransmisores (45% de probabilidad de que un spike no cruce la sinapsis).
 * - A veces se queda "colgado" mirando a la nada (THOUGHT_CURIOUS / THOUGHT_NONE) sin reaccionar.
 * - Cero neuroplasticidad: no aprende de la experiencia.
 */
class SluggishBrain : public Brain {
public:
    SluggishBrain() {
        setupConnectome();
    }

    const char* getName() const override { return "SNN Sluggish / Tonto (Lag & Dropout)"; }
    const char* getArchetypeId() const override { return "DUMB"; }

    void setupConnectome() override {
        for (int i = 0; i < 10; ++i) {
            neurons[i].threshold = 2.2f;
            neurons[i].leak = 1.35f; // Pierde potencial rapidísimo
            neurons[i].potential = 0.0f;
        }
        neurons[S_TOUCH].threshold = 1.2f;
        neurons[M_IDLE].threshold  = 0.9f;

        synapses.clear();
        // Conectoma precario: solo conexiones básicas y débiles, sin inhibición lateral fina
        synapses.push_back(Synapse(S_FATIGUE, M_SLEEP, 0.75f));
        synapses.push_back(Synapse(S_PAIN,    M_SICK,  0.60f));
        synapses.push_back(Synapse(S_TOUCH,   M_IDLE,  0.50f));
        synapses.push_back(Synapse(S_HUNGER,  M_IDLE,  0.45f));
        synapses.push_back(Synapse(M_IDLE,    M_IDLE,  0.45f)); // Inercia fuerte de quedarse pasmado
    }

    void applyGenome(const PetGenome& genome) override {
        // Independiente de la personalidad genética para pruebas puras
        setupConnectome();
    }

    void processPlasticity(float dt, float trust, PetStage stage) override {
        // No aprende nada: la dopamina se disipa rápido sin reforzar sinapsis
        float safeDt = min(dt, 0.5f);
        dopamine = 0.1f + (dopamine - 0.1f) * expf(-0.4f * safeDt);
    }

    void update(float dt, const PetSensoryInput& input) override {
        float safeDt = min(dt, 0.5f);
        isEggStage = input.isEgg;
        lastTrust = input.trust;

        if (isEggStage || input.health <= 0.0f) {
            Brain::update(safeDt, input);
            return;
        }

        // Lag cognitivo: 35% de los ticks atenúa la entrada sensorial ("distraído")
        if (random(0, 100) < 35) {
            for (int i = 0; i < 5; ++i) {
                neurons[i].potential *= 0.85f;
            }
        }

        //Alimentación lenta
        neurons[S_HUNGER].feed((100.0f - constrain(input.hunger, 0.0f, 100.0f)) * 0.025f * safeDt);
        neurons[S_FATIGUE].feed((100.0f - constrain(input.energy, 0.0f, 100.0f)) * 0.025f * safeDt);
        neurons[S_PAIN].feed(((100.0f - constrain(input.health, 0.0f, 100.0f)) * 0.03f) * safeDt);

        if (input.touched) {
            neurons[S_TOUCH].feed(1.8f * safeDt);
        }

        // Propagación con "Dropout Sináptico" (45% de fallo al transmitir pulso)
        for (int i = 0; i < 10; ++i) {
            if (neurons[i].update(safeDt)) {
                globalSpikeCount++;
                for (size_t s = 0; s < synapses.size(); ++s) {
                    if (synapses[s].preIndex == i && random(0, 100) >= 45) {
                        neurons[synapses[s].postIndex].feed(synapses[s].weight);
                    }
                }
            }
        }

        processPlasticity(safeDt, input.trust, input.stage);

        spikeTimer += safeDt;
        if (spikeTimer >= 1.0f) {
            spikeRate = (float)globalSpikeCount / spikeTimer;
            globalSpikeCount = 0;
            spikeTimer = 0.0f;
        }

        if (forcedThoughtTimer > 0.0f) {
            forcedThoughtTimer -= safeDt;
        } else {
            evaluateThoughts(input);
        }

        computePsychosomaticMultipliers();

        if (!input.lightsOn) {
            currentDecision = PetState::Sleeping;
            return;
        }
        if (input.health < 15.0f) {
            currentDecision = PetState::Sick;
        } else {
            currentDecision = PetState::Idle; // Casi siempre tranquilo/inmutable
        }
    }

    void evaluateThoughts(const PetSensoryInput& input) override {
        driveHunger   = 100.0f - constrain(input.hunger, 0.0f, 100.0f);
        driveSleep    = 100.0f - constrain(input.energy, 0.0f, 100.0f);
        driveSocial   = (100.0f - constrain(input.happiness, 0.0f, 100.0f)) * 0.4f;
        driveDistress = (100.0f - constrain(input.health, 0.0f, 100.0f)) * 0.5f;

        // Reacciona muy tarde (solo cuando el hambre o daño son extremos)
        if (driveHunger > 78.0f) {
            currentThought = THOUGHT_FOOD;
            lastThoughtReason = "Recien nota que su panza hace ruido...";
        } else if (input.health < 25.0f) {
            currentThought = THOUGHT_MED;
            lastThoughtReason = "Procesando dolor con retraso...";
        } else if (random(0, 100) < 12) {
            currentThought = THOUGHT_CURIOUS;
            lastThoughtReason = "Lag mental: Mirando un punto fijo sin pensar en nada.";
        } else {
            currentThought = THOUGHT_NONE;
            lastThoughtReason = "Mente en blanco (Sin actividad sinaptica relevante).";
        }
        ruminationLevel = 0.05f;
    }
};

/**
 * 2. PLASTIC LEARNER BRAIN ("Cerebro Neuroplástico / Pavloviano STDP")
 * - Arranca "Tabula Rasa" con pesos bajos y aprende en vivo según cómo lo trates.
 * - STDP (Spike-Timing-Dependent Plasticity): Si la neurona A dispara poco antes que B, el cable A->B se engrosa.
 * - Sinaptogénesis Asociativa (Pavlov): Si tiene hambre/aburrimiento y recibe caricias o comida,
 *   crea nuevas conexiones asociativas en tiempo real (visibles en el Dashboard).
 * - Homeostasis Intrínseca: Ajusta los umbrales de las neuronas según su uso.
 */
class PlasticLearnerBrain : public Brain {
public:
    PlasticLearnerBrain() {
        setupConnectome();
    }

    const char* getName() const override { return "SNN Pavlovian STDP Learner (Plasticity+)"; }
    const char* getArchetypeId() const override { return "LEARNER"; }

    void setupConnectome() override {
        for (int i = 0; i < 10; ++i) {
            neurons[i].threshold = 1.2f;
            neurons[i].leak = 0.45f;
            neurons[i].potential = 0.0f;
            neurons[i].lastSpikeTime = 0;
        }
        neurons[S_TOUCH].threshold = 0.45f;
        neurons[M_IDLE].threshold  = 0.85f;
        neurons[M_LOVE].threshold  = 1.15f;

        synapses.clear();
        // Semilla inicial débil ("Tabula Rasa"): los pesos crecerán o cambiarán con el aprendizaje
        synapses.push_back(Synapse(S_HUNGER,  M_IDLE,  0.30f, 2.5f, -2.0f));
        synapses.push_back(Synapse(S_FATIGUE, M_SLEEP, 0.80f, 2.5f, -2.0f));
        synapses.push_back(Synapse(S_BOREDOM, M_PLAY,  0.50f, 2.5f, -2.0f));
        synapses.push_back(Synapse(S_PAIN,    M_SICK,  0.70f, 2.5f, -2.0f));
        synapses.push_back(Synapse(S_TOUCH,   M_LOVE,  0.45f, 2.5f, -2.0f));
        synapses.push_back(Synapse(S_TOUCH,   M_PLAY,  0.40f, 2.5f, -2.0f));
        synapses.push_back(Synapse(M_LOVE,    S_PAIN, -0.20f, 2.0f, -2.5f));
        synapses.push_back(Synapse(M_LOVE,    M_IDLE,  0.35f, 2.5f, -2.0f));
    }

    void applyGenome(const PetGenome& genome) override {
        // Mantiene su topología de aprendizaje pura sin sesgo genético
    }

    void onFed() override {
        // Condicionamiento Pavloviano al comer: Si pensaba en comida o afecto, refuerza el circuito
        Brain::onFed();
        dopamine = constrain(dopamine + 0.65f, 0.0f, 2.0f);
        reinforceOrCreateSynapse(S_HUNGER, M_LOVE, 0.18f);
        reinforceOrCreateSynapse(S_TOUCH,  M_IDLE, 0.12f);
        nudgeThought(THOUGHT_HEART, "[Pavlov] Asocio comida con bienestar (+sinapsis)", 0.45f);
    }

    void processPlasticity(float dt, float trust, PetStage stage) override {
        float safeDt = min(dt, 0.5f);
        dopamine = 0.25f + (dopamine - 0.25f) * expf(-0.08f * safeDt);
        uint32_t now = millis();

        // 1. Regla STDP (Spike-Timing-Dependent Plasticity) en todas las sinapsis
        for (size_t i = 0; i < synapses.size(); ++i) {
            int pre = synapses[i].preIndex;
            int post = synapses[i].postIndex;

            if (neurons[post].didSpike && neurons[pre].lastSpikeTime > 0) {
                uint32_t deltaMs = now - neurons[pre].lastSpikeTime;
                if (deltaMs <= 1600) {
                    // Pre disparó justo antes que Post -> LTP (Potenciación a Largo Plazo)
                    float timingFactor = expf(-(float)deltaMs / 600.0f);
                    float deltaW = 0.08f * timingFactor * (0.5f + dopamine);
                    if (synapses[i].weight >= 0.0f) {
                        synapses[i].reinforce(1.0f, deltaW);
                    } else {
                        // Las sinapsis inhibitorias se vuelven más fuertes (más negativas)
                        synapses[i].reinforce(1.0f, -deltaW * 0.6f);
                    }
                }
            }
        }

        // 2. Sinaptogénesis Asociativa (crea nuevos cables si se co-activan sensores y motores)
        if (neurons[S_TOUCH].didSpike) {
            if (neurons[S_HUNGER].potential > 0.35f) {
                reinforceOrCreateSynapse(S_HUNGER, M_LOVE, 0.14f);
            }
            if (neurons[S_BOREDOM].potential > 0.35f) {
                reinforceOrCreateSynapse(S_BOREDOM, M_LOVE, 0.14f);
            }
            if (neurons[S_PAIN].potential > 0.30f) {
                reinforceOrCreateSynapse(S_TOUCH, S_PAIN, -0.15f); // Aprende que la caricia calma el dolor
            }
        }

        // 3. Homeostasis Intrínseca: ajusta umbrales para que ninguna neurona quede muda ni saturada
        for (int n = 0; n < 10; ++n) {
            if (neurons[n].didSpike) {
                neurons[n].threshold = min(2.4f, neurons[n].threshold + 0.015f);
            } else {
                neurons[n].threshold = max(0.45f, neurons[n].threshold - (0.004f * safeDt));
            }
        }
    }

private:
    void reinforceOrCreateSynapse(int pre, int post, float deltaWeight) {
        for (size_t i = 0; i < synapses.size(); ++i) {
            if (synapses[i].preIndex == pre && synapses[i].postIndex == post) {
                synapses[i].reinforce(1.0f, deltaWeight);
                return;
            }
        }
        if (synapses.size() < 26) {
            synapses.push_back(Synapse(pre, post, deltaWeight, 2.5f, -2.5f));
        }
    }
};

/**
 * 3. CHAOTIC SPARK BRAIN ("Cerebro Hiper-Reactivo / Soñador / Impulsivo")
 * - Umbrales bajos, resonancia estocástica (ruido sináptico espontáneo) y avalanchas.
 * - Cambia de pensamientos con mucha creatividad (curiosidad, capricho, nostalgia, juego).
 */
class ChaoticSparkBrain : public Brain {
public:
    ChaoticSparkBrain() {
        setupConnectome();
    }

    const char* getName() const override { return "SNN Chaotic Spark (Hyper-Reactive Dreamer)"; }
    const char* getArchetypeId() const override { return "HYPER"; }

    void setupConnectome() override {
        Brain::setupConnectome();
        for (int i = 0; i < 10; ++i) {
            neurons[i].threshold *= 0.65f; // Muy excitable
            neurons[i].leak = 0.28f;      // Retiene la excitación mucho tiempo
        }
        // Lazos cruzados de asociación libre
        synapses.push_back(Synapse(S_BOREDOM, M_LOVE,  0.55f));
        synapses.push_back(Synapse(M_PLAY,    M_LOVE,  0.50f));
        synapses.push_back(Synapse(M_LOVE,    M_PLAY,  0.45f));
        synapses.push_back(Synapse(S_HUNGER,  M_PLAY,  0.40f));
    }

    void applyGenome(const PetGenome& genome) override {
        setupConnectome();
    }

    void update(float dt, const PetSensoryInput& input) override {
        float safeDt = min(dt, 0.5f);
        if (!input.isEgg && input.health > 0.0f) {
            // Ruido sináptico espontáneo (Chispazos de imaginación)
            if (random(0, 100) < 28) {
                int rndNeuron = random(0, 10);
                neurons[rndNeuron].feed(0.22f);
            }
        }
        Brain::update(safeDt, input);

        // Si no tiene un pensamiento urgente, salta entre ideas creativas espontáneamente
        if (!input.isEgg && input.health > 40.0f && forcedThoughtTimer <= 0.0f && random(0, 100) < 8) {
            static const ThoughtType creativeThoughts[] = {
                THOUGHT_CURIOUS, THOUGHT_CAPRICE, THOUGHT_NOSTALGIA, THOUGHT_PLAY, THOUGHT_HEART
            };
            ThoughtType pick = creativeThoughts[random(0, 5)];
            nudgeThought(pick, "Chispazo espontaneo de hiper-conectividad SNN", 0.30f);
        }
    }
};

inline std::unique_ptr<IBrain> createBrainArchetype(const String& typeName) {
    String t = typeName;
    t.trim();
    t.toUpperCase();

    if (t == "LEARNER" || t == "PLASTIC" || t == "PAVLOV") {
        return std::unique_ptr<IBrain>(new PlasticLearnerBrain());
    }
    if (t == "DUMB" || t == "TONTO" || t == "SLUGGISH" || t == "SLOW") {
        return std::unique_ptr<IBrain>(new SluggishBrain());
    }
    if (t == "HYPER" || t == "CHAOS" || t == "SPARK") {
        return std::unique_ptr<IBrain>(new ChaoticSparkBrain());
    }
    if (t == "FSM" || t == "SIMPLE" || t == "1996") {
        return std::unique_ptr<IBrain>(new SimpleReactiveBrain());
    }
    return std::unique_ptr<IBrain>(new Brain());
}
