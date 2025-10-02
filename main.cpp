#include <zlib.h>
#include <cstdlib>
#include <iostream>
#include <string> 
#include <fstream>
#include <cstdio>
#include <vector>
#include <webp/decode.h>
#include <webp/encode.h>
#include <sstream>
#include <iomanip>
#include "SX1278.h"

int file_count = 1;

using namespace std;

void callback_Rx(char* payload, uint8_t len, int rssi, float snr); // user callback function for when a packet is received. 
void callback_Tx(void);  // user callback function for when a packet is transmited.

void save_webp_preview(const uint8_t* rgba, int width, int height, int step, std::string prefix) {
    uint8_t* output;
    size_t size = WebPEncodeRGBA(rgba, width, height, width * 4, 75, &output);

    if (size > 0 && output) {
        std::ostringstream fname;
        //fname << prefix << "_" << std::setw(3) << std::setfill('0') << step << ".webp";
        fname << prefix << "_preview.webp";

        std::ofstream file(fname.str(), std::ios::binary);
        file.write(reinterpret_cast<const char*>(output), size);
        file.close();

        std::cout << "Bildvorschau gespeichert: " << fname.str() << "\n";
        WebPFree(output);
    }
}

void generate_preview(std::string fname, uint8_t seq) {
    std::ifstream input(fname, std::ios::binary);
    if (!input) {
        std::cerr << "Datei input.webp nicht gefunden!\n";
        return;
    }

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(input)), {});
    size_t total_size = buffer.size();

    WebPDecoderConfig config;
    if (!WebPInitDecoderConfig(&config)) {
        std::cerr << "Decoder-Konfiguration fehlgeschlagen!\n";
        return;
    }

    config.output.colorspace = MODE_RGBA;
    WebPIDecoder* idec = WebPINewDecoder(&config.output);

    size_t chunk_size = 100;
    size_t offset = 0;

        size_t len = std::min(chunk_size, total_size - offset);
        VP8StatusCode status = WebPIAppend(idec, buffer.data(), total_size);
        offset += len;

        if (status == VP8_STATUS_OK) {
            // Vollständig dekodierbar
            save_webp_preview(config.output.u.RGBA.rgba, config.output.width, config.output.height, seq, fname);
            std::cout << "Bild vollständig dekodiert.\n";
        } else if (status == VP8_STATUS_SUSPENDED) {
            // Teilweise dekodiert, aber keine API für „wie viel“
            // Du kannst hier theoretisch die RGBA-Daten nehmen, sind aber evtl. unvollständig
            save_webp_preview(config.output.u.RGBA.rgba, config.output.width, config.output.height, seq, fname);
            std::cout << "Bild teilweise dekodiert (suspended).\n";
        } else {
            std::cerr << "Dekodierfehler: " << status << "\n";
        }

    WebPIDelete(idec);
}

int main(int argc, char** argv) {    
    try {
        loRa.onRxDone(callback_Rx);  // Register a user callback function 
        loRa.onTxDone(callback_Tx);  // Register a user callback function

        loRa.begin();                // settings the radio     
        loRa.continuous_receive();   // Puts the radio in continuous receive mode.

        while(1){   
            sleep(0.01);
        }     
    } catch (const std::runtime_error &e) {     
        cout << "Exception caught: " << e.what() << endl;
    }
    return 0;
}

void callback_Rx(char* payload, uint8_t len, int rssi, float snr) {

    // ---------------- Sicherheit: Mindestlänge prüfen ----------------
    // 1 Byte Seq + ≥1 Byte Daten + 4 Byte CRC  →  mindestens 6 Bytes
    if (len < 6) {
        std::cerr << "Paket zu kurz – verworfen!\n";
        return;
    }
    
    // ---------------- Sequenznummer auslesen ----------------
    uint8_t type = static_cast<uint8_t>(payload[0]);
    // ---------------- Sequenznummer auslesen ----------------
    uint8_t seq = static_cast<uint8_t>(payload[1]);
    seq++;
    // ---------------- Sequenzanzahl auslesen ----------------
    uint8_t seq_count = static_cast<uint8_t>(payload[2]);

    // ---------------- Daten- & CRC-Bereiche bestimmen ----------------
    const uint8_t* dataPtr   = reinterpret_cast<uint8_t*>(payload + 3);    // hinter Seq count
    std::size_t    dataLen   = len - 3 - 4;                                // reiner Datenanteil

    uint32_t rxCRC;
    std::memcpy(&rxCRC, payload + len - 4, 4);                             // letzte 4 Bytes

    // ---------------- CRC berechnen (Seq+Daten) ----------------
    uint32_t calcCRC = crc32(0L, Z_NULL, 0);
    calcCRC = crc32(calcCRC,
                    reinterpret_cast<const Bytef*>(payload),               // ab Sequenzbyte
                    len - 4);                                              // type, Seq, seq count + Daten

    bool crcOK = (calcCRC == rxCRC);

    // ---------------- Log-Ausgabe ----------------
    std::cout << "Rx: Seq " << static_cast<int>(seq) << "/" << static_cast<int>(seq_count) 
              << " | " << dataLen << " B  | CRC "
              << (crcOK ? "OK" : "FEHLER")
              << " | RSSI " << rssi << " dBm | SNR "
              << snr  << " dB\n";

    if (type == 0) {
        std::cout << "DATA TYPE: IMAGE DATA";
        // ---------------- Schreiben (oder Dummy-Nullen) ----------------
        std::ofstream outFile("data/"+std::to_string(file_count)+".webp", std::ios::binary | std::ios::app);
        if (!outFile) {
            std::cerr << "Fehler: konnte Datei nicht öffnen!\n";
            return;
        }

        if (crcOK) {
            outFile.write(reinterpret_cast<const char*>(dataPtr), dataLen);
            generate_preview("data/"+std::to_string(file_count)+".webp", seq);
        }
    } else if (type == 1) {
        std::cout << "DATA TYPE: TELEMETRY DATA\n";

        if (crcOK) {
            std::string telemetryMsg(reinterpret_cast<const char*>(dataPtr), dataLen);
            std::cout << "📡 Telemetrie empfangen: " << telemetryMsg << "\n";

            // Telemetriezeile an Datei anhängen
            std::ofstream telemetryFile("data/telemetry.txt", std::ios::app);
            if (telemetryFile) {
                telemetryFile << telemetryMsg << "\n";
                std::cout << "❌ CRC ungültig – Telemetrie geschrieben\n";
            } else {
                std::cerr << "Fehler: Konnte telemetry.txt nicht öffnen zum Schreiben.\n";
            }
        } else {
            std::cout << "❌ CRC ungültig – Telemetrie verworfen\n";
        }
    }


    // --- ACK/NACK senden ---
    std::vector<int8_t> packet = {
        static_cast<int8_t>(seq-1),
        static_cast<int8_t>(crcOK ? 0xAA : 0xFF)
    };

    loRa.send(packet.data(), static_cast<uint8_t>(packet.size()));

    std::cout << "→ " << (crcOK ? "ACK" : "NACK") << " gesendet für Seq " << static_cast<int>(seq) << "\n";

    if ((type == 0) && (seq == seq_count)) {
        std::cout << "Datei vollständig empfangen, inkrementiere Dateizähler..." << std::endl;
        generate_preview("data/"+std::to_string(file_count)+".webp", seq);
        file_count++;
    }
}

void callback_Tx(void) {
    cout << "Tx done : " << endl;
}


