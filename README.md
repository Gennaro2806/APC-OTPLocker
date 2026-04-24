# APC-OTPLocker

## Descrizione Progetto

**APC-OTPLocker** è un sistema di sicurezza avanzato per cassaforte che implementa un'autenticazione a due fattori (2FA) basata su codici OTP (One-Time Password). Il progetto è sviluppato come esame per il corso di **Architettura e Progetto dei Calcolatori** (prof. Mazzocca), Ingegneria Informatica M63.

Il sistema combina una verifica tramite PIN locale con l'autenticazione OTP trasmessa via Bluetooth, fornendo un doppio livello di sicurezza per l'apertura del locker.

---

## Architettura del Sistema

Il progetto adotta un'**architettura Master-Slave** basata su due microcontrollori:

```
┌─────────────────────┐
│  STM32F303VC        │
│  (MASTER)           │
│                     │
│  - Gestione FSM     │
│  - Controllo servo  │
│  - Tastierino       │
│  - Display OLED     │
│  - Allarmi (LED, buzzer)
└──────────┬──────────┘
           │ UART
           │
┌──────────▼──────────┐
│  ESP32              │
│  (SLAVE)            │
│                     │
│  - Comunicazione    │
│    con cellulare    │
└─────────────────────┘
```

### Comunicazione Master-Slave

- **Interfaccia:** UART
- **Protocollo:** Scambio di comandi tra STM32 ed ESP32
- **Funzione:** L'ESP32 riceve richieste di autenticazione dallo STM32 e invia i codici OTP al dispositivo Bluetooth dell'utente

---

## Componenti Hardware

### Microcontrollori

| Componente | Descrizione | Ruolo |
|-----------|-------------|-------|
| **STM32F303VC** | Controllo principale del sistema |
| **ESP32** | Gestione Bluetooth e OTP |

### Periferiche di Input/Output

| Componente | Funzione |
|-----------|----------|
| **Display OLED SSD1306** | Visualizzazione degli stati della FSM |
| **Tastierino 4x4** | Inserimento PIN e codice OTP |
| **Servo Motore** | Meccanismo di apertura/chiusura del locker |
| **LED Colorati** | Indicatori di stato (autenticazione, errore, etc.) |
| **Buzzer** | Segnalazione acustica per apertura/chiusura/allarme |

---

## Flusso di Autenticazione

### Sequenza di Sblocco

1. **Inserimento PIN:** L'utente inserisce il PIN personale tramite il tastierino numerico
2. **Validazione Locale:** L'STM32 verifica il PIN rispetto a quello memorizzato
3. **Richiesta OTP:** Se il PIN è corretto, l'STM32 invia una richiesta all'ESP32 via UART
4. **Invio OTP:** L'ESP32 genera un codice OTP e lo trasmette via Bluetooth al cellulare dell'utente
5. **Inserimento OTP:** L'utente inserisce il codice OTP ricevuto tramite il tastierino
6. **Sblocco:** Se l'OTP è valido, il servo motore apre il locker

### Feedback dell'Utente

- **Display OLED:** Mostra lo stato corrente (es. "Inserisci PIN", "Attesa OTP", "Autorizzato")
- **LED:** Indicatori visivi dei risultati 
- **Buzzer:** Segnalazione acustica dell'apertura/chiusura

---

## Struttura del Progetto

### Firmware STM32 (`Codice_STM32/`)

```
Core/
├── Inc/                          # Header files
│   ├── main.h                    # Configurazione principale
│   ├── fsm.h                     # Finite State Machine
│   ├── keypad.h                  # Gestione tastierino
│   ├── ssd1306.h                 # Driver display OLED
│   ├── servo.h                   # Controllo servo motore
│   ├── alarm.h                   # Gestione allarmi (LED, buzzer)
│   ├── esp_uart.h                # Comunicazione con ESP32
│   ├── fonts.h                   # Font per il display
│   └── stm32f3xx_hal_*.h         # HAL STM32
│
└── Src/                          # Implementazioni
    ├── main.c
    ├── fsm.c
    ├── keypad.c
    ├── ssd1306.c
    ├── servo.c
    ├── alarm.c
    ├── esp_uart.c
    ├── fonts.c
    └── stm32f3xx_*.c
```

### Firmware ESP32 (`Codice_ESP32/`)

- **Esp32.ino:** Firmware principale per la gestione Bluetooth e generazione OTP
- Gestisce la comunicazione UART con l'STM32
- Invia i codici OTP via Bluetooth al dispositivo mobile

### Documentazione e Utility 

- **Utils/COLLEGAMENTI.txt:** Schema di collegamento dei componenti
- **Assets** Cartella con immagini e video del progetto funzionante

---

## Moduli Principali

### UART ESP32

Gestisce la comunicazione con l'ESP32:
- Invio di comandi di autenticazione
- Ricezione dei codici OTP generati
- Sincronizzazione tra i due microcontrollori

### Keypad

Acquisisce l'input dell'utente:
- Lettura dei tasti premuti
- Debouncing per evitare letture errate
- Supporto per input multipli (PIN + OTP)

### SSD1306

Driver per il display OLED:
- Visualizzazione degli stati della FSM
- Messaggi di feedback all'utente
- Indicatori di progresso

### Servo

Controllo del meccanismo di apertura:
- PWM per il controllo della posizione
- Posizione di chiusura e apertura predefinite

### Alarm

Gestione dei segnali di allarme:
- Control dei LED (colori diversi per stati diversi)
- Generazione di segnali sonori dal buzzer

---

## Setup e Configurazione

### Prerequisiti

- STM32CubeIDE per la compilazione del firmware STM32
- Arduino IDE per il firmware ESP32
- Hardware secondo lo schema fornito in `Utils/COLLEGAMENTI.txt`

### Compilazione STM32

1. Aprire il progetto in STM32CubeIDE
2. Navigare a `Codice_STM32/`
3. Compilare il progetto (Build)
4. Caricare il firmware sul microcontrollore

### Caricamento ESP32

1. Aprire `Codice_ESP32/Prova/Prova.ino` in Arduino IDE
2. Configurare la scheda e la porta seriale
3. Caricare lo sketch sulla scheda ESP32

---

## Note Tecniche

- **Frequenza STM32:** 72 MHz
- **Frequenza ESP32:** 160/240 MHz
- **Comunicazione:** UART tra STM32 e ESP32
- **Protocollo Bluetooth:** Standard Bluetooth 4.0+ (ESP32)
- **Display:** I2C SSD1306 128x64 pixel
- **Tastierino:** 4x4 matrice
- **PWM Servo:** Standard 50Hz

---

## Autore

Progetto realizzato per il corso di Architettura e Progetto dei Calcolatori  
Università - Ingegneria Informatica M63

Gennaro Iannicelli  -   M63001668
Giuseppe Gatta      -   M63001669
Aurora D'Ambrosio   -   M63001662
