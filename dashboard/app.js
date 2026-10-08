const canvas = document.getElementById('canvas');
const ctx = canvas ? canvas.getContext('2d') : null;
const pCanvas = document.getElementById('preview-canvas');
const pCtx = pCanvas ? pCanvas.getContext('2d') : null;

const terminal = document.getElementById('terminal');
const chatInput = document.getElementById('chat-input');
const btnSend = document.getElementById('btnSend');
const btnClearConsole = document.getElementById('btnClearConsole');
const btnConnect = document.getElementById('btnConnect');

let isTunerModeActive = false;
let port = null, reader = null, writer = null;

// Control de Exportacion
let isExporting = false;
let exportBuffer = "";

// Perfiles de Dominio
const domains = {
    "GLOBAL":      { deltaR:0, deltaG:0, deltaB:0, brightness:0, contrast:0, saturation:0 },
    "TAMA":        { deltaR:0, deltaG:0, deltaB:0, brightness:0, contrast:0, saturation:0 },
    "WORLD":       { deltaR:0, deltaG:0, deltaB:0, brightness:0, contrast:0, saturation:0 },
    "BUTTONS":     { deltaR:0, deltaG:0, deltaB:0, brightness:0, contrast:0, saturation:0 },
    "HUD":         { deltaR:0, deltaG:0, deltaB:0, brightness:0, contrast:0, saturation:0 },
    "THOUGHT":     { deltaR:0, deltaG:0, deltaB:0, brightness:0, contrast:0, saturation:0 },
    "POPUP_DAY":   { deltaR:0, deltaG:0, deltaB:0, brightness:0, contrast:0, saturation:0 },
    "POPUP_DREAM": { deltaR:0, deltaG:0, deltaB:0, brightness:0, contrast:0, saturation:0 }
};

const colorOverrides = {};

let currentDomain = "TAMA";
let isColorSpecificMode = false;
let selectedColorHex = "0xDC22";
let baseR = 220, baseG = 20, baseB = 20;

let pendingSend = false, isSending = false;
let currentSpritePath = "../sd_root/tama/sprites/base/tiernito/bebe/idle.png";

// Conectoma de 10 Neuronas intacto
const neurons = [
    { id: 0, name: "S_HUNGER",  x: 130, y: 65,  col: "#ff7043", v: 0.0, th: 1.0, spike: false },
    { id: 1, name: "S_FATIGUE", x: 130, y: 150, col: "#42a5f5", v: 0.0, th: 1.0, spike: false },
    { id: 2, name: "S_BOREDOM", x: 130, y: 235, col: "#ab47bc", v: 0.0, th: 1.0, spike: false },
    { id: 3, name: "S_PAIN",    x: 130, y: 320, col: "#ef5350", v: 0.0, th: 1.0, spike: false },
    { id: 4, name: "S_TOUCH",   x: 130, y: 405, col: "#26a69a", v: 0.0, th: 0.5, spike: false },

    { id: 5, name: "M_SLEEP",   x: 760, y: 65,  col: "#5c6bc0", v: 0.0, th: 2.0, spike: false },
    { id: 6, name: "M_PLAY",    x: 760, y: 150, col: "#66bb6a", v: 0.0, th: 1.8, spike: false },
    { id: 7, name: "M_SICK",    x: 760, y: 235, col: "#e53935", v: 0.0, th: 1.8, spike: false },
    { id: 8, name: "M_IDLE",    x: 760, y: 405, col: "#ffa726", v: 0.0, th: 0.8, spike: false },
    { id: 9, name: "M_LOVE",    x: 760, y: 320, col: "#f06292", v: 0.0, th: 1.6, spike: false }
];

let synapses = [
    { pre: 1, post: 5, w: 1.2 }, { pre: 3, post: 7, w: 0.9 },
    { pre: 4, post: 6, w: 0.6 }, { pre: 4, post: 8, w: 0.3 },
    { pre: 2, post: 6, w: 1.0 }, { pre: 0, post: 8, w: 0.4 },
    { pre: 4, post: 9, w: 0.9 }, { pre: 9, post: 8, w: 0.3 },
    { pre: 9, post: 7, w: -0.4 }, { pre: 9, post: 3, w: -0.35 },
    { pre: 4, post: 3, w: -0.3 }, { pre: 3, post: 8, w: -0.25 },
    { pre: 5, post: 6, w: -0.3 }, { pre: 6, post: 5, w: -0.3 },
    { pre: 7, post: 6, w: -0.35 }, { pre: 6, post: 7, w: -0.35 },
    { pre: 5, post: 8, w: -0.3 }, { pre: 8, post: 5, w: -0.25 },
    { pre: 7, post: 9, w: -0.3 }, { pre: 6, post: 6, w: 0.25 },
    { pre: 5, post: 5, w: 0.3 }, { pre: 8, post: 8, w: 0.2 },
    { pre: 9, post: 9, w: 0.25 }
];

// Conexión WebSerial
if (btnConnect) {
    btnConnect.onclick = async () => {
        try {
            port = await navigator.serial.requestPort();
            await port.open({ baudRate: 115200 });

            document.getElementById('connStatus').innerText = "CONECTADO";
            document.getElementById('connStatus').style.color = "var(--accent-green)";
            btnConnect.style.display = "none";

            readSerialLoop();
        } catch (err) {
            alert("Error al conectar WebSerial: " + err);
        }
    };
}

async function readSerialLoop() {
    const textDecoder = new TextDecoderStream();
    port.readable.pipeTo(textDecoder.writable);
    reader = textDecoder.readable.getReader();
    let buffer = "";

    while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        buffer += value;
        let lines = buffer.split("\n");
        buffer = lines.pop();

        for (let line of lines) {
            line = line.trim();
            if (!line) continue;

            // --- INTERCEPTOR DE EXPORTACIÓN RAW ---
            if (line === "!RAW_CFG_START") { 
                isExporting = true; 
                exportBuffer = ""; 
                continue; 
            }
            if (line === "!RAW_CFG_END") { 
                isExporting = false; 
                downloadBlob(exportBuffer, "tama_color_config.txt"); 
                continue; 
            }
            if (isExporting) { 
                exportBuffer += line + "\n"; 
                continue; 
            }
            // --------------------------------------

            const isTunerConfirmation = line.includes("TAMA COLOR TUNER") || 
                                        line.includes("MODO CALIBRADOR EN EJECUCIÓN") ||
                                        line.includes("Entrando al módulo ColorTuner") ||
                                        line.startsWith("!PROFILES:");

            const isTunerExitConfirmation = line.includes("Reiniciando hacia TAMA") || 
                                            line.includes("Saliendo del calibrador") || 
                                            line.includes("This is 4");

            if (isTunerConfirmation && !isTunerModeActive) {
                switchInterfaceMode(true);
                sendCommand("GET_PROFILES");
            } else if (isTunerExitConfirmation && isTunerModeActive) {
                switchInterfaceMode(false);
            }

            if (line.startsWith("!PROFILES:")) {
                try {
                    const data = JSON.parse(line.substring(10));
                    if (data.domains) {
                        for (let d in data.domains) {
                            if (domains[d]) Object.assign(domains[d], data.domains[d]);
                        }
                    }
                    if (data.global_profile) { 
                        Object.assign(domains["GLOBAL"], data.global_profile);
                    }
                    if (data.colors) {
                        for (let c in data.colors) {
                            colorOverrides[c] = data.colors[c];
                        }
                    }
                    updateTunerUI();
                } catch(e) {}
            } else if (line.startsWith("!BRAIN:") && !isTunerModeActive) {
                try {
                    const data = JSON.parse(line.substring(7));
                    updateBrainState(data);
                } catch (e) {}
                } else if (line.startsWith("!GENOME:") && !isTunerModeActive) {
                try {
                    const data = JSON.parse(line.substring(8));
                    document.getElementById('lblPersonalityTag').innerText = data.tag;
                    document.getElementById('txtGenMetab').innerText = data.metab.toFixed(2) + "x";
                    document.getElementById('txtGenSoc').innerText = data.soc.toFixed(2) + "x";
                    document.getElementById('txtGenRes').innerText = data.res.toFixed(2) + "x";
                    document.getElementById('txtGenSlp').innerText = data.slp.toFixed(2) + "x";
                } catch (e) {}
            } else {
                appendLog(line);
                if (line.includes("Frase seleccionada:") || line.includes("Motivo neuronal:") || line.includes("Sueno")) {
                    updateDialogueCard(line);
                }
            }
        }
    }
}

async function sendCommand(cmd) {
    if (!port || !port.writable) return;
    try {
        const textEncoder = new TextEncoder();
        writer = port.writable.getWriter();
        await writer.write(textEncoder.encode(cmd + "\n"));
        writer.releaseLock();
        appendLog("> " + cmd, "color: #fbeeaa; font-weight: bold;");

        const upper = cmd.trim().toUpperCase();
        if (upper === "TUNECOLOURS" && !isTunerModeActive) {
            switchInterfaceMode(true);
            sendCommand("GET_PROFILES");
        } else if (upper === "EXIT" && isTunerModeActive) {
            switchInterfaceMode(false);
        }
    } catch (err) {}
}

if (btnSend && chatInput) {
    btnSend.onclick = () => { sendCommand(chatInput.value); chatInput.value = ""; };
    chatInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') { sendCommand(chatInput.value); chatInput.value = ""; }
    });
}

if (btnClearConsole && terminal) {
    btnClearConsole.onclick = () => { terminal.innerHTML = ""; };
}

function classifyLog(text) {
    if (text.startsWith(">")) return "log-cmd";
    if (text.includes("ALERT") || text.includes("Error") || text.includes("FALLECIDO") || 
        text.includes("Fallo") || text.includes("CRÍTICO") || text.includes("Caido") || text.includes("Corrompida")) return "log-error";
    if (text.startsWith("---") || text.startsWith("===") || text.startsWith("***")) return "log-divider";
    if (text.startsWith("[KERNEL]") || text.startsWith("[TEST]")) return "log-kernel";
    if (text.startsWith("[ASSETS]")) return "log-assets";
    if (text.startsWith("[TIME]") || text.includes("NTP") || text.includes("EVENTO NTP")) return "log-time";
    if (text.startsWith("[MAIN]")) return "log-main";
    if (text.startsWith("[OTA]")) return "log-ota";
    if (text.startsWith("[AI-CACHE]") || text.startsWith("[AI-TRIGGER]") || text.includes("TRIGGER") || text.startsWith("[DREAM-TRIGGER]")) return "log-ai";
    if (text.startsWith("[PET]") || text.startsWith("[EVOLUCIÓN") || text.includes("TAMA PET DIAGNOSTICS") || 
        text.includes("Especie:") || text.includes("Etapa:") || text.includes("Estado:") || text.includes("Hambre:") || text.includes("Salud:")) return "log-pet";
    if (text.startsWith("[COLOR-SD]") || text.startsWith("[TUNER]")) return "log-tuner";
    if (text.startsWith("[TOUCH]")) return "log-touch";
    if (text.startsWith("[MIND]") || text.startsWith("[BRAIN]") || text.startsWith("!GENOME")) return "log-mind"; 
    if (text.startsWith("[MIND]") || text.startsWith("[BRAIN]")) return "log-mind";
    if (text.includes("Urgencia") || text.includes("Mult Hambre") || text.includes("Spike Rate") || text.includes("Dopamina") || text.includes("Apego")) return "log-diag";
    return "log-default";
}

function appendLog(text, customStyle = "") {
    if (!terminal) return;
    const div = document.createElement('div');
    div.className = "log-line " + classifyLog(text);
    if (customStyle) div.style = customStyle;
    div.innerText = text;
    terminal.appendChild(div);
    terminal.scrollTop = terminal.scrollHeight;
}

function switchInterfaceMode(toTuner) {
    isTunerModeActive = toTuner;
    document.getElementById('main-grid').style.display = toTuner ? "none" : "grid";
    document.getElementById('tuner-grid').style.display = toTuner ? "grid" : "none";
    document.getElementById('standardChips').style.display = "block";
    document.getElementById('btnExitTunerHeader').style.display = toTuner ? "inline-block" : "none";

    const title = document.getElementById('titleHeader');
    const sub = document.getElementById('subtitleHeader');

    if (toTuner) {
        title.innerText = "TAMA // LABORATORIO DE COLOR HSL DESACOPLADO";
        title.style.color = "var(--accent-blue)";
        sub.innerText = "Luz Pura Desacoplada, Vista Previa Ampliada y Calibración Color por Color";
        updateTunerUI();
        renderOriginalPreview();
    } else {
        title.innerText = "TAMA // MISSION CONTROL & BANCO DE COGNICIÓN";
        title.style.color = "var(--accent-gold)";
        sub.innerText = "Control de Estados Biológicos, Sprites y Estimulación SNN";
    }
}

function enterColorTuner() { 
    switchInterfaceMode(true);
    sendCommand("TUNECOLOURS"); 
    setTimeout(() => sendCommand("GET_PROFILES"), 200);
}

function exitColorTuner() { 
    sendCommand("EXIT"); 
    switchInterfaceMode(false);
}

function saveToSD() { sendCommand("SAVE"); }
function factoryResetSD() {
    if (confirm("¿Restaurar todos los perfiles de color a valores de fábrica?")) {
        for (let d in domains) domains[d] = { deltaR:0, deltaG:0, deltaB:0, brightness:0, contrast:0, saturation:0 };
        for (let c in colorOverrides) delete colorOverrides[c];
        updateTunerUI();
        sendCommand("FACTORY");
    }
}

// ================= MATEMÁTICA HSL =================
function clamp(v, min, max) { return Math.min(Math.max(v, min), max); }
function rgb565(r, g, b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

function rgbToHsl(r, g, b) {
    let rf = r / 255, gf = g / 255, bf = b / 255;
    let max = Math.max(rf, gf, bf), min = Math.min(rf, gf, bf);
    let h = 0, s = 0, l = (max + min) / 2;

    if (max !== min) {
        let d = max - min;
        s = l > 0.5 ? d / (2 - max - min) : d / (max + min);
        switch(max) {
            case rf: h = (gf - bf) / d + (gf < bf ? 6 : 0); break;
            case gf: h = (bf - rf) / d + 2; break;
            case bf: h = (rf - gf) / d + 4; break;
        }
        h /= 6;
    }
    return { h, s, l };
}

function hue2rgb(p, q, t) {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1/6) return p + (q - p) * 6 * t;
    if (t < 1/2) return q;
    if (t < 2/3) return p + (q - p) * (2/3 - t) * 6;
    return p;
}

function hslToRgb(h, s, l) {
    let r, g, b;
    if (s === 0) {
        r = g = b = l;
    } else {
        let q = l < 0.5 ? l * (1 + s) : l + s - l * s;
        let p = 2 * l - q;
        r = hue2rgb(p, q, h + 1/3);
        g = hue2rgb(p, q, h);
        b = hue2rgb(p, q, h - 1/3);
    }
    return {
        r: Math.round(clamp(r * 255, 0, 255)),
        g: Math.round(clamp(g * 255, 0, 255)),
        b: Math.round(clamp(b * 255, 0, 255))
    };
}

function applyPerceptualPipeline(r, g, b, p) {
    let { h, s, l } = rgbToHsl(r, g, b);

    if (p.brightness > 0) l += (1.0 - l) * (p.brightness / 100.0);
    else if (p.brightness < 0) l += l * (p.brightness / 100.0);
    l = clamp(l, 0.0, 1.0);

    if (p.contrast !== 0) {
        let cFactor = Math.max(0.0, 1.0 + (p.contrast / 100.0));
        l = 0.5 + (l - 0.5) * cFactor;
        l = clamp(l, 0.0, 1.0);
    }

    if (p.saturation > 0) s += (1.0 - s) * (p.saturation / 100.0);
    else if (p.saturation < 0) s += s * (p.saturation / 100.0);
    s = clamp(s, 0.0, 1.0);

    let rgb = hslToRgb(h, s, l);
    return {
        r: Math.round(clamp(rgb.r + p.deltaR, 0, 255)),
        g: Math.round(clamp(rgb.g + p.deltaG, 0, 255)),
        b: Math.round(clamp(rgb.b + p.deltaB, 0, 255))
    };
}

function getCurrentActiveProfile() {
    if (isColorSpecificMode) {
        if (!colorOverrides[selectedColorHex]) {
            colorOverrides[selectedColorHex] = { deltaR:0, deltaG:0, deltaB:0, brightness:0, contrast:0, saturation:0 };
        }
        return colorOverrides[selectedColorHex];
    }
    return domains[currentDomain];
}

function onSliderDrag(prop, val) {
    const p = getCurrentActiveProfile();
    p[prop] = clamp(parseInt(val) || 0, -100, 100);
    updateTunerUI();
}

function onSliderCommit(prop, val) {
    const p = getCurrentActiveProfile();
    p[prop] = clamp(parseInt(val) || 0, -100, 100);
    updateTunerUI();
    scheduleSendProfile();
}

function stepProp(prop, delta) {
    const p = getCurrentActiveProfile();
    p[prop] = clamp(p[prop] + delta, -100, 100);
    updateTunerUI();
    scheduleSendProfile();
}

function setProp(prop, val) {
    const p = getCurrentActiveProfile();
    p[prop] = val;
    updateTunerUI();
    scheduleSendProfile();
}

function scheduleSendProfile() {
    pendingSend = true;
    if (isSending) return;
    isSending = true;

    requestAnimationFrame(async () => {
        while (pendingSend) {
            pendingSend = false;
            if (port && port.writable) {
                try {
                    const p = getCurrentActiveProfile();
                    let cmd = "";
                    if (isColorSpecificMode) {
                        cmd = `COLOR ${selectedColorHex} ${p.deltaR} ${p.deltaG} ${p.deltaB} ${p.brightness} ${p.contrast} ${p.saturation}\n`;
                    } else if (currentDomain === "GLOBAL") {
                        cmd = `GLOBAL_TRIM ${p.deltaR} ${p.deltaG} ${p.deltaB} ${p.brightness} ${p.contrast} ${p.saturation}\n`;
                    } else {
                        cmd = `TRIM ${currentDomain} ${p.deltaR} ${p.deltaG} ${p.deltaB} ${p.brightness} ${p.contrast} ${p.saturation}\n`;
                    }                            
                    const encoder = new TextEncoder();
                    writer = port.writable.getWriter();
                    await writer.write(encoder.encode(`BASE ${baseR} ${baseG} ${baseB}\n`));
                    await writer.write(encoder.encode(cmd));
                    writer.releaseLock();
                } catch(e) {}
            }
            await new Promise(res => setTimeout(res, 25));
        }
        isSending = false;
    });
}

function switchDomain(dom) {
    currentDomain = dom;
    document.querySelectorAll('.domain-btn').forEach(b => {
        b.classList.toggle('active', b.innerText.includes(dom));
    });
    sendCommand(`DOMAIN ${dom}`);
    selectGlobalDomainMode();
    renderOriginalPreview();
}

function selectGlobalDomainMode() {
    isColorSpecificMode = false;
    document.getElementById('lblAjusteModo').innerText = `DOMINIO GLOBAL [${currentDomain}]`;
    document.getElementById('lblAjusteModo').style.color = "var(--accent-gold)";
    document.querySelectorAll('.palette-chip').forEach(c => c.classList.remove('active'));
    updateTunerUI();
    scheduleSendProfile();
}

function selectColorTarget(name, r, g, b, hexStr) {
    isColorSpecificMode = true;
    selectedColorHex = hexStr;
    baseR = r; baseG = g; baseB = b;

    document.getElementById('lblAjusteModo').innerText = `COLOR INDIVIDUAL: ${name} (${hexStr})`;
    document.getElementById('lblAjusteModo').style.color = "var(--accent-blue)";

    document.querySelectorAll('.palette-chip').forEach(c => {
        c.classList.toggle('active', c.dataset.hex === hexStr);
    });

    updateTunerUI();
    scheduleSendProfile();
}

function resetCurrentTarget() {
    if (isColorSpecificMode) {
        delete colorOverrides[selectedColorHex];
        sendCommand(`COLOR_RESET ${selectedColorHex}`);
    } else {
        domains[currentDomain] = { deltaR:0, deltaG:0, deltaB:0, brightness:0, contrast:0, saturation:0 };
    }
    updateTunerUI();
    scheduleSendProfile();
}

function updateTunerUI() {
    const p = getCurrentActiveProfile();

    document.getElementById('sliderBri').value = p.brightness;
    document.getElementById('numBri').value = p.brightness;
    document.getElementById('sliderCon').value = p.contrast;
    document.getElementById('numCon').value = p.contrast;
    document.getElementById('sliderSat').value = p.saturation;
    document.getElementById('numSat').value = p.saturation;

    document.getElementById('sliderR').value = p.deltaR;
    document.getElementById('numR').value = p.deltaR;
    document.getElementById('sliderG').value = p.deltaG;
    document.getElementById('numG').value = p.deltaG;
    document.getElementById('sliderB').value = p.deltaB;
    document.getElementById('numB').value = p.deltaB;

    const res = applyPerceptualPipeline(baseR, baseG, baseB, p);
    const orig565 = "0x" + rgb565(baseR, baseG, baseB).toString(16).padStart(4, '0').toUpperCase();
    const corr565 = "0x" + rgb565(res.r, res.g, res.b).toString(16).padStart(4, '0').toUpperCase();

    document.getElementById('box-original').style.background = `rgb(${baseR}, ${baseG}, ${baseB})`;
    document.getElementById('txtOrigRGB').innerText = `R:${baseR} G:${baseG} B:${baseB}`;
    document.getElementById('txtOrig565').innerText = orig565;

    document.getElementById('box-corrected').style.background = `rgb(${res.r}, ${res.g}, ${res.b})`;
    document.getElementById('txtCorrRGB').innerText = `R:${res.r} G:${res.g} B:${res.b}`;
    document.getElementById('txtCorr565').innerText = corr565;
}

const spritePaths = {
    "bebe_idle":     "../sd_root/tama/sprites/base/tiernito/bebe/idle.png",
    "bebe_happy":    "../sd_root/tama/sprites/base/tiernito/bebe/happy.png",
    "bebe_eating":   "../sd_root/tama/sprites/base/tiernito/bebe/eating.png",
    "bebe_sleeping": "../sd_root/tama/sprites/base/tiernito/bebe/sleeping.png",
    "bebe_sick":     "../sd_root/tama/sprites/base/tiernito/bebe/sick.png",
    "bebe_sad":      "../sd_root/tama/sprites/base/tiernito/bebe/sad.png",
    "bebe_dead":     "../sd_root/tama/sprites/base/tiernito/bebe/dead.png",
    "child_idle":    "../sd_root/tama/sprites/base/tiernito/child/idle.png",
    "adulto_idle":   "../sd_root/tama/sprites/base/tiernito/adulto/idle.png",
    "huevo_idle":    "../sd_root/tama/sprites/base/tiernito/huevo/idle.png"
};

const imageCache = {};

function changeCanvasZoom(zoomStr) {
    const z = parseFloat(zoomStr) || 1.5;
    pCanvas.style.width = Math.round(172 * z) + "px";
    pCanvas.style.height = Math.round(320 * z) + "px";
}

function onVariantChange(val) {
    if (spritePaths[val]) {
        currentSpritePath = spritePaths[val];
        renderOriginalPreview();
    }
}

function extractPaletteFromCanvas() {
    const bar = document.getElementById('palette-bar');
    if (!bar || !pCtx || !pCanvas) return;
    bar.innerHTML = "";

    try {
        const imgData = pCtx.getImageData(0, 0, pCanvas.width, pCanvas.height).data;
        const colorMap = {};

        for (let i = 0; i < imgData.length; i += 24) {
            const a = imgData[i + 3];
            if (a < 128) continue;

            const r = imgData[i];
            const g = imgData[i + 1];
            const b = imgData[i + 2];
            const hex = "0x" + rgb565(r, g, b).toString(16).padStart(4, '0').toUpperCase();

            if (!colorMap[hex]) {
                colorMap[hex] = { r, g, b, count: 1 };
            } else {
                colorMap[hex].count++;
            }
        }

        const sorted = Object.entries(colorMap)
            .sort((a, b) => b[1].count - a[1].count)
            .slice(0, 8);

        sorted.forEach(([hex, item]) => {
            const chip = document.createElement('div');
            chip.className = "palette-chip";
            chip.dataset.hex = hex;
            chip.innerHTML = `<span class="color-dot" style="background:rgb(${item.r},${item.g},${item.b});"></span><span>${hex}</span>`;
            chip.onclick = () => selectColorTarget("Color " + hex, item.r, item.g, item.b, hex);
            bar.appendChild(chip);
        });
    } catch(e) {
        bar.innerHTML = `<span style="font-size:11px; color:#80deea;">* Toca directamente la imagen para calibrar un píxel.</span>`;
    }
}

function renderOriginalPreview() {
    if (!pCtx || !pCanvas) return;
    const w = pCanvas.width = 172;
    const h = pCanvas.height = 320;
    pCtx.clearRect(0, 0, w, h);

    const selVariant = document.getElementById('selSpriteVariant');
    if (selVariant) {
        selVariant.style.display = (currentDomain === "TAMA") ? "inline-block" : "none";
    }

    let assetUrl = "";
    let isSpriteSheet = false;

    if (currentDomain === "TAMA") {
        assetUrl = currentSpritePath;
        isSpriteSheet = true;
    } else if (currentDomain === "WORLD") {
        assetUrl = "../sd_root/tama/ui/bg_main.png";
    } else if (currentDomain === "BUTTONS") {
        assetUrl = "../sd_root/tama/ui/icons_ui.png";
    } else if (currentDomain === "THOUGHT") {
        assetUrl = "../sd_root/tama/ui/hunger_icon.png";
    }

    if (assetUrl) {
        let img = imageCache[assetUrl];
        if (!img) {
            img = new Image();
            img.src = assetUrl;
            img.onload = () => {
                imageCache[assetUrl] = img;
                drawLoadedImage(img, isSpriteSheet);
            };
            img.onerror = () => {
                drawFallbackVectorGraphic();
            };
        } else {
            drawLoadedImage(img, isSpriteSheet);
        }
    } else {
        drawFallbackVectorGraphic();
    }
}

function drawLoadedImage(img, isSpriteSheet) {
    const w = pCanvas.width;
    const h = pCanvas.height;
    pCtx.clearRect(0, 0, w, h);
    pCtx.imageSmoothingEnabled = false;

    if (isSpriteSheet) {
        const frameW = 48;
        const frameH = Math.min(48, img.height);
        const scale = 3;
        const destW = frameW * scale;
        const destH = frameH * scale;
        const dx = Math.floor((w - destW) / 2);
        const dy = Math.floor((h - destH) / 2);

        pCtx.drawImage(img, 0, 0, frameW, frameH, dx, dy, destW, destH);
    } else {
        pCtx.drawImage(img, 0, 0, w, h);
    }
    extractPaletteFromCanvas();
}

function drawFallbackVectorGraphic() {
    const w = pCanvas.width;
    const h = pCanvas.height;
    pCtx.clearRect(0, 0, w, h);

    if (currentDomain === "POPUP_DAY") {
        const bx = 10, by = 50, bw = 152, bh = 220;
        pCtx.fillStyle = "#4c2818";
        pCtx.fillRect(bx, by, bw, bh);
        pCtx.fillStyle = "#ffffff";
        pCtx.fillRect(bx + 2, by + 2, bw - 4, bh - 4);
        pCtx.fillStyle = "#f5dfbf";
        pCtx.fillRect(bx + 3, by + 3, bw - 6, bh - 6);

        pCtx.fillStyle = "#4c2818";
        pCtx.font = "bold 12px monospace";
        pCtx.textAlign = "center";
        pCtx.fillText("POPUP DIURNO", w/2, by + 26);

        pCtx.fillStyle = "#4c2818";
        pCtx.fillRect(bx + 16, by + 60, bw - 32, 38);
        pCtx.fillStyle = "#569440";
        pCtx.fillRect(bx + 18, by + 62, bw - 36, 34);
        pCtx.fillStyle = "#ffffff";
        pCtx.fillText("ACEPTAR", w/2, by + 84);

        pCtx.fillStyle = "#4c2818";
        pCtx.fillRect(bx + 16, by + 115, bw - 32, 38);
        pCtx.fillStyle = "#424242";
        pCtx.fillRect(bx + 18, by + 117, bw - 36, 34);
        pCtx.fillStyle = "#ffffff";
        pCtx.fillText("CANCELAR", w/2, by + 139);
    } else if (currentDomain === "POPUP_DREAM") {
        const bx = 10, by = 60, bw = 152, bh = 190;
        pCtx.fillStyle = "#182838";
        pCtx.fillRect(bx, by, bw, bh);
        pCtx.fillStyle = "#94b2b2";
        pCtx.strokeRect(bx + 2, by + 2, bw - 4, bh - 4);
        pCtx.fillStyle = "#296565";
        pCtx.fillRect(bx + 4, by + 4, bw - 8, bh - 8);

        pCtx.fillStyle = "#defbfb";
        pCtx.font = "12px monospace";
        pCtx.textAlign = "center";
        pCtx.fillText("SUENO ONIRICO", w/2, by + 32);
        pCtx.fillText("tibio como el nido...", w/2, by + 75);
        pCtx.fillText("zzz...", w/2, by + 105);
    } else if (currentDomain === "HUD") {
        const items = [
            { icon:"♥", col:"#fa6000", bgCol:"#f80000", val:0.85, label:"HAMBRE" },
            { icon:"★", col:"#fde000", bgCol:"#fee000", val:0.70, label:"FELIZ" },
            { icon:"⚡", col:"#05bfff", bgCol:"#07ffff", val:0.95, label:"ENERGIA" }
        ];
        items.forEach((it, idx) => {
            let y = 60 + idx * 55;
            pCtx.fillStyle = it.bgCol;
            pCtx.font = "bold 16px monospace";
            pCtx.fillText(it.icon, 16, y + 16);
            pCtx.fillStyle = "#210404";
            pCtx.fillRect(38, y, 116, 20);
            pCtx.fillStyle = "#10a2a2";
            pCtx.fillRect(40, y + 2, 112, 16);
            pCtx.fillStyle = it.col;
            pCtx.fillRect(40, y + 2, 112 * it.val, 16);
        });
    }
    extractPaletteFromCanvas();
}

if (pCanvas) {
    pCanvas.onclick = (e) => {
        const rect = pCanvas.getBoundingClientRect();
        const scaleX = pCanvas.width / rect.width;
        const scaleY = pCanvas.height / rect.height;
        const x = Math.floor((e.clientX - rect.left) * scaleX);
        const y = Math.floor((e.clientY - rect.top) * scaleY);

        try {
            const pixel = pCtx.getImageData(x, y, 1, 1).data;
            if (pixel[3] > 20) {
                const r = pixel[0], g = pixel[1], b = pixel[2];
                const c565 = "0x" + rgb565(r, g, b).toString(16).padStart(4, '0').toUpperCase();
                selectColorTarget(`Pixel (${x},${y})`, r, g, b, c565);
            }
        } catch(err) {
            console.log("Canvas read restriction, use palette chips:", err);
        }
    };
}

function updateDialogueCard(logLine) {
    const lbl = document.getElementById('lblDialogueText');
    if (!lbl) return;
    if (logLine.includes("Frase seleccionada:")) {
        const parts = logLine.split("Frase seleccionada:");
        if (parts[1]) lbl.innerText = parts[1].replace(/\"/g, '').trim();
    } else if (logLine.includes("Sueno:") || logLine.includes("Sueno sintetizado:")) {
        const parts = logLine.includes("Sueno sintetizado:") ? logLine.split("Sueno sintetizado:") : logLine.split("Sueno:");
        if (parts[1]) lbl.innerText = "💤 " + parts[1].replace(/\"/g, '').trim();
    }
}

function updateBrainState(data) {
    if (!data) return;

    if (data.spikeRate !== undefined) {
        document.getElementById('lblSpikeRate').innerText = Number(data.spikeRate).toFixed(1);
    }
    if (data.dopamine !== undefined) {
        document.getElementById('lblDopamine').innerText = Number(data.dopamine).toFixed(2);
    }
    if (data.rumination !== undefined) {
        document.getElementById('lblRumination').innerText = Number(data.rumination).toFixed(2);
    }
    if (data.thoughtName) {
        document.getElementById('lblThoughtName').innerText = data.thoughtName;
    }
    if (data.thoughtReason) {
        document.getElementById('lblThoughtReason').innerText = data.thoughtReason;
    }

    if (data.trust !== undefined) {
        const trustVal = clamp(Number(data.trust), 0, 100);
        document.getElementById('barTrust').style.width = trustVal + "%";
        document.getElementById('txtTrust').innerText = Math.round(trustVal) + "%";
    }

    if (data.drives) {
        if (data.drives.hunger !== undefined) {
            document.getElementById('barHunger').style.width = data.drives.hunger + "%";
            document.getElementById('txtHunger').innerText = Math.round(data.drives.hunger) + "%";
        }
        if (data.drives.social !== undefined) {
            document.getElementById('barSocial').style.width = data.drives.social + "%";
            document.getElementById('txtSocial').innerText = Math.round(data.drives.social) + "%";
        }
        if (data.drives.sleep !== undefined) {
            document.getElementById('barSleep').style.width = data.drives.sleep + "%";
            document.getElementById('txtSleep').innerText = Math.round(data.drives.sleep) + "%";
        }
        if (data.drives.distress !== undefined) {
            document.getElementById('barDistress').style.width = data.drives.distress + "%";
            document.getElementById('txtDistress').innerText = Math.round(data.drives.distress) + "%";
        }
    }

    if (data.neurons && Array.isArray(data.neurons)) {
        for (let i = 0; i < Math.min(neurons.length, data.neurons.length); i++) {
            if (data.neurons[i].v !== undefined) neurons[i].v = Number(data.neurons[i].v);
            if (data.neurons[i].th !== undefined) neurons[i].th = Number(data.neurons[i].th);
            if (data.neurons[i].spk !== undefined) neurons[i].spike = Boolean(data.neurons[i].spk);
        }
    }

    if (data.synapses && Array.isArray(data.synapses) && data.synapses.length > 0) {
        synapses = data.synapses;
    }
}

function renderConnectome() {
    if (!ctx || !canvas) return;

    ctx.clearRect(0, 0, canvas.width, canvas.height);

    ctx.strokeStyle = "rgba(68, 37, 22, 0.2)";
    ctx.lineWidth = 1;
    for (let x = 0; x < canvas.width; x += 40) {
        ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, canvas.height); ctx.stroke();
    }
    for (let y = 0; y < canvas.height; y += 40) {
        ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(canvas.width, y); ctx.stroke();
    }

    ctx.fillStyle = "#90a4ae";
    ctx.font = "bold 11px monospace";
    ctx.fillText("CORTEZA AFERENTE (SENSORIAL)", 30, 25);
    ctx.fillText("CORTEZA EFERENTE (MOTORA & APEGO)", 620, 25);

    for (let s of synapses) {
        let pre = neurons[s.pre];
        let post = neurons[s.post];
        if (!pre || !post) continue;

        let w = (typeof s.w === 'number') ? s.w : parseFloat(s.w) || 0.0;
        let isExcitatory = w >= 0;

        if (s.pre === s.post) {
            const loopRadius = 18;
            const loopCenterX = pre.x + 18;
            const loopCenterY = pre.y - 18;

            ctx.beginPath();
            ctx.arc(loopCenterX, loopCenterY, loopRadius, 0.35 * Math.PI, 1.85 * Math.PI, false);
            ctx.setLineDash([]);
            ctx.lineWidth = Math.max(1.2, Math.abs(w) * 2.2);
            ctx.strokeStyle = isExcitatory ? "rgba(255, 213, 79, 0.65)" : "rgba(128, 222, 234, 0.65)";
            ctx.stroke();

            const endX = pre.x + 15;
            const endY = pre.y - 2;
            ctx.beginPath();
            ctx.moveTo(endX, endY);
            ctx.lineTo(endX + 4, endY - 6);
            ctx.lineTo(endX + 7, endY);
            ctx.fillStyle = isExcitatory ? "#ffd54f" : "#80deea";
            ctx.fill();

            const tag = (w >= 0 ? "+" : "") + w.toFixed(2);
            ctx.fillStyle = "rgba(11, 8, 19, 0.9)";
            ctx.fillRect(loopCenterX - 16, loopCenterY - 18, 32, 13);
            ctx.strokeStyle = isExcitatory ? "rgba(255, 213, 79, 0.7)" : "rgba(128, 222, 234, 0.7)";
            ctx.lineWidth = 1;
            ctx.strokeRect(loopCenterX - 16, loopCenterY - 18, 32, 13);

            ctx.fillStyle = isExcitatory ? "#ffd54f" : "#80deea";
            ctx.font = "bold 9px monospace";
            ctx.textAlign = "center";
            ctx.fillText(tag, loopCenterX, loopCenterY - 8);
            ctx.textAlign = "start";
            continue;
        }

        ctx.beginPath();
        ctx.moveTo(pre.x, pre.y);
        ctx.bezierCurveTo(pre.x + 220, pre.y, post.x - 220, post.y, post.x, post.y);
        ctx.lineWidth = Math.max(1.2, Math.abs(w) * 2.8);

        if (pre.spike) {
            ctx.setLineDash([]);
            ctx.strokeStyle = "#ffffff";
            ctx.shadowColor = "#ffffff";
            ctx.shadowBlur = 16;
        } else if (!isExcitatory) {
            ctx.setLineDash([4, 4]);
            ctx.strokeStyle = `rgba(100, 180, 246, ${Math.min(0.85, Math.abs(w) * 0.65)})`;
            ctx.shadowBlur = 0;
        } else {
            ctx.setLineDash([]);
            ctx.strokeStyle = `rgba(186, 104, 200, ${Math.min(0.85, w * 0.55)})`;
            ctx.shadowBlur = 0;
        }
        ctx.stroke();
        ctx.shadowBlur = 0;
        ctx.setLineDash([]);

        let mx = (pre.x + post.x) / 2;
        let my = (pre.y + post.y) / 2;
        let tag = (w >= 0 ? "+" : "") + w.toFixed(2);

        ctx.fillStyle = "rgba(11, 8, 19, 0.85)";
        ctx.fillRect(mx - 18, my - 7, 36, 14);
        ctx.strokeStyle = isExcitatory ? "rgba(255, 213, 79, 0.6)" : "rgba(128, 222, 234, 0.6)";
        ctx.lineWidth = 1;
        ctx.strokeRect(mx - 18, my - 7, 36, 14);

        ctx.fillStyle = isExcitatory ? "#ffd54f" : "#80deea";
        ctx.font = "bold 9px monospace";
        ctx.textAlign = "center";
        ctx.fillText(tag, mx, my + 3);
        ctx.textAlign = "start";
    }

    for (let n of neurons) {
        let v = (typeof n.v === 'number') ? n.v : parseFloat(n.v) || 0.0;
        let th = (typeof n.th === 'number') ? n.th : parseFloat(n.th) || 1.0;
        let progress = clamp(v / th, 0.0, 1.0);

        ctx.beginPath();
        ctx.arc(n.x, n.y, 22, 0, Math.PI * 2);
        ctx.strokeStyle = "rgba(255, 255, 255, 0.1)";
        ctx.lineWidth = 3;
        ctx.stroke();

        if (progress > 0) {
            ctx.beginPath();
            ctx.arc(n.x, n.y, 22, -Math.PI / 2, (-Math.PI / 2) + (progress * Math.PI * 2));
            ctx.strokeStyle = n.col;
            ctx.lineWidth = 3;
            ctx.stroke();
        }

        ctx.beginPath();
        ctx.arc(n.x, n.y, n.spike ? 19 : 14, 0, Math.PI * 2);
        ctx.fillStyle = n.spike ? "#ffffff" : n.col;
        ctx.shadowColor = n.col;
        ctx.shadowBlur = n.spike ? 28 : 8;
        ctx.fill();
        ctx.shadowBlur = 0;

        ctx.strokeStyle = "#ffffff";
        ctx.lineWidth = 1.5;
        ctx.stroke();

        ctx.fillStyle = "#ffffff";
        ctx.font = "bold 11px monospace";
        ctx.fillText(n.name, n.x - 28, n.y - 28);

        ctx.fillStyle = "#ffe082";
        ctx.font = "10px monospace";
        ctx.fillText(`V:${v.toFixed(2)}/${th.toFixed(1)}`, n.x - 28, n.y + 36);
    }
}

function drawCanvas() {
    try {
        if (!isTunerModeActive) {
            renderConnectome();
        }
    } catch (err) {
        console.error("Canvas draw error:", err);
    }
    requestAnimationFrame(drawCanvas);
}

// ================= EXPORTACIÓN E IMPORTACIÓN =================

function exportConfig() {
    sendCommand("GET_RAW_CFG");
    alert("Exportando... Espere a que se genere el archivo.");
}

function downloadBlob(content, filename) {
    const blob = new Blob([content], { type: "text/plain" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
}

function importConfig(event) {
    const file = event.target.files[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = function(e) {
        const content = e.target.result;
        if(confirm("¿Aplicar esta configuración de color? Reemplazará la actual en RAM y SD.")) {
            sendCommand("!SET_RAW_START");
            // Enviar linea por linea por limitaciones del buffer Serial
            const lines = content.split('\n');
            let i = 0;
            function sendNext() {
                if (i < lines.length) {
                    if(lines[i].trim() !== "") sendCommand(lines[i].trim());
                    i++;
                    setTimeout(sendNext, 20); // Delay corto para no saturar Serial
                } else {
                    sendCommand("!SET_RAW_END");
                    setTimeout(() => sendCommand("GET_PROFILES"), 500); // Refrescar UI
                }
            }
            sendNext();
        }
        event.target.value = ''; // Resetear input
    };
    reader.readAsText(file);
}

// Arranque de ciclo de renderizado de la UI
drawCanvas();
updateTunerUI();
renderOriginalPreview();
changeCanvasZoom("1.5");
