#pragma once

#include <vector>

namespace ox
{
enum class Engine { Modal, Pluck, Subtractive, FM, Wind, Organ };

// One entry per voice shown in the OX voice selector.
struct Patch
{
    const char* name;
    const char* category;
    Engine engine;
    int variant;     // engine specific flavour
    float attack;    // seconds
    float release;   // seconds
    float decay;     // engine specific (pluck T60, FM amp T60, filter env time, mallet decay scale)
    float bright;    // 0..1 (0.5 = neutral for Modal/FM)
    float detune;    // cents (ensemble spread / piano string beating / chorus)
    float vib;       // vibrato depth in cents
    float drive;     // 0..1 distortion
    float fltEnv;    // filter envelope depth in octaves
    float res;       // filter Q
    float gain;      // per-voice level trim
};

inline const std::vector<const char*>& categories()
{
    static const std::vector<const char*> c = { "Piano", "Mallets", "Guitar", "Synth", "Fluid", "Strings", "Winds", "Organ" };
    return c;
}

inline const std::vector<Patch>& patches()
{
    using E = Engine;
    static const std::vector<Patch> list =
    {
        //  name                category    engine        var  atk      rel     decay   bright  detune  vib     drive   fEnv    res     gain
        // ---- Piano ----
        { "Grand Piano",      "Piano",    E::Modal,       8, 0.001f, 0.30f,  1.00f,  0.50f,  1.2f,   0.0f,   0.00f,  0.0f,   0.7f,   1.60f },
        { "Bright Piano",     "Piano",    E::Modal,       9, 0.001f, 0.25f,  1.00f,  0.65f,  1.0f,   0.0f,   0.00f,  0.0f,   0.7f,   1.20f },
        { "Upright Piano",    "Piano",    E::Modal,      10, 0.001f, 0.25f,  0.80f,  0.38f,  1.6f,   0.0f,   0.00f,  0.0f,   0.7f,   2.00f },
        { "Honky-Tonk",       "Piano",    E::Modal,      11, 0.001f, 0.25f,  0.80f,  0.55f, 14.0f,   0.0f,   0.00f,  0.0f,   0.7f,   1.60f },
        { "Rhodes E.Piano",   "Piano",    E::FM,          0, 0.002f, 0.40f,  3.50f,  0.50f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   1.00f },
        { "DX E.Piano",       "Piano",    E::FM,          1, 0.002f, 0.35f,  3.00f,  0.50f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   1.00f },
        { "Clavinet",         "Piano",    E::FM,          4, 0.001f, 0.08f,  0.90f,  0.50f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   1.00f },
        // ---- Mallets ----
        { "Xylophone",        "Mallets",  E::Modal,       0, 0.001f, 0.12f,  1.00f,  0.50f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   1.80f },
        { "Marimba",          "Mallets",  E::Modal,       1, 0.001f, 0.20f,  1.00f,  0.50f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   1.50f },
        { "Vibraphone",       "Mallets",  E::Modal,       2, 0.001f, 0.50f,  1.00f,  0.50f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   2.00f },
        { "Glockenspiel",     "Mallets",  E::Modal,       3, 0.001f, 0.40f,  1.00f,  0.50f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   1.60f },
        { "Kalimba",          "Mallets",  E::Modal,       4, 0.001f, 0.30f,  1.00f,  0.50f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   1.80f },
        { "Music Box",        "Mallets",  E::Modal,       5, 0.001f, 0.40f,  1.00f,  0.50f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   1.60f },
        { "Celesta",          "Mallets",  E::Modal,       7, 0.001f, 0.40f,  1.00f,  0.50f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   1.80f },
        { "Tubular Bells",    "Mallets",  E::Modal,       6, 0.001f, 0.80f,  1.00f,  0.50f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   1.20f },
        // ---- Guitar ----
        { "Steel Guitar",     "Guitar",   E::Pluck,       0, 0.001f, 0.15f,  2.50f,  0.75f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   1.50f },
        { "Nylon Guitar",     "Guitar",   E::Pluck,       1, 0.001f, 0.15f,  2.00f,  0.45f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   1.50f },
        { "Lead Guitar",      "Guitar",   E::Pluck,       2, 0.001f, 0.30f,  4.00f,  0.80f,  0.0f,   0.0f,   0.75f,  0.0f,   0.7f,   0.90f },
        { "Muted Guitar",     "Guitar",   E::Pluck,       3, 0.001f, 0.06f,  0.40f,  0.35f,  0.0f,   0.0f,   0.10f,  0.0f,   0.7f,   1.20f },
        { "Banjo",            "Guitar",   E::Pluck,       5, 0.001f, 0.10f,  1.00f,  0.95f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   2.00f },
        { "Harp",             "Guitar",   E::Pluck,       4, 0.001f, 0.35f,  4.00f,  0.60f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   1.60f },
        { "Pluck Bass",       "Guitar",   E::Pluck,       6, 0.001f, 0.12f,  1.50f,  0.30f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   1.30f },
        // ---- Synth ----
        { "Saw Lead",         "Synth",    E::Subtractive, 0, 0.005f, 0.15f,  0.40f,  0.62f, 12.0f,  12.0f,   0.00f,  1.5f,   1.2f,   0.80f },
        { "Super Saw",        "Synth",    E::Subtractive, 1, 0.010f, 0.25f,  0.50f,  0.72f, 35.0f,   0.0f,   0.00f,  1.0f,   0.8f,   0.65f },
        { "Square Lead",      "Synth",    E::Subtractive, 6, 0.005f, 0.15f,  0.30f,  0.60f,  6.0f,  10.0f,   0.00f,  1.0f,   1.0f,   0.80f },
        { "Analog Pad",       "Synth",    E::Subtractive, 2, 0.800f, 1.20f,  1.50f,  0.55f, 18.0f,   0.0f,   0.00f,  0.8f,   0.8f,   0.70f },
        { "Synth Bass",       "Synth",    E::Subtractive, 5, 0.003f, 0.10f,  0.25f,  0.35f,  5.0f,   0.0f,   0.00f,  3.0f,   3.0f,   0.50f },
        // ---- Fluid ----
        { "Fluid Keys",       "Fluid",    E::FM,          2, 0.002f, 0.50f,  2.50f,  0.50f,  6.0f,   0.0f,   0.00f,  0.0f,   0.7f,   0.90f },
        { "Fluid Lead",       "Fluid",    E::FM,          5, 0.010f, 0.40f,  0.00f,  0.50f,  7.0f,  18.0f,   0.00f,  0.0f,   0.7f,   0.80f },
        { "Fluid Bell",       "Fluid",    E::FM,          3, 0.001f, 0.60f,  5.00f,  0.50f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   0.90f },
        // ---- Strings & brass ----
        { "String Ensemble",  "Strings",  E::Subtractive, 3, 0.350f, 0.60f,  1.00f,  0.62f, 22.0f,   6.0f,   0.00f,  0.3f,   0.8f,   0.70f },
        { "Synth Brass",      "Strings",  E::Subtractive, 4, 0.040f, 0.15f,  0.30f,  0.50f,  8.0f,   4.0f,   0.00f,  2.0f,   1.0f,   0.65f },
        { "Choir Ah",         "Strings",  E::Subtractive, 7, 0.500f, 0.80f,  1.00f,  0.60f, 14.0f,  10.0f,   0.00f,  0.0f,   0.7f,   0.55f },
        // ---- Winds ----
        { "Ocarina",          "Winds",    E::Wind,        0, 0.060f, 0.15f,  0.00f,  0.50f,  0.0f,  14.0f,   0.00f,  0.0f,   0.7f,   1.00f },
        { "Flute",            "Winds",    E::Wind,        1, 0.070f, 0.15f,  0.00f,  0.50f,  0.0f,  10.0f,   0.00f,  0.0f,   0.7f,   1.00f },
        { "Pan Flute",        "Winds",    E::Wind,        2, 0.050f, 0.12f,  0.00f,  0.50f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   1.00f },
        { "Clarinet",         "Winds",    E::Wind,        3, 0.040f, 0.12f,  0.00f,  0.50f,  0.0f,   6.0f,   0.00f,  0.0f,   0.7f,   1.00f },
        { "Whistle",          "Winds",    E::Wind,        4, 0.080f, 0.12f,  0.00f,  0.50f,  0.0f,  18.0f,   0.00f,  0.0f,   0.7f,   1.00f },
        // ---- Organ ----
        { "Jazz Organ",       "Organ",    E::Organ,       0, 0.005f, 0.05f,  0.00f,  0.50f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   1.00f },
        { "Rock Organ",       "Organ",    E::Organ,       1, 0.003f, 0.04f,  0.00f,  0.50f,  0.0f,   0.0f,   0.30f,  0.0f,   0.7f,   1.00f },
        { "Church Organ",     "Organ",    E::Organ,       2, 0.030f, 0.40f,  0.00f,  0.50f,  0.0f,   0.0f,   0.00f,  0.0f,   0.7f,   1.00f },
    };
    return list;
}
} // namespace ox
