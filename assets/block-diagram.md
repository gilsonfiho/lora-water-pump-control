# Diagrama de Blocos da Solução

Visão geral dos dois nós e do enlace LoRa.

![Diagrama de blocos da solução](block-diagram.svg)

> A imagem acima (SVG) abre em qualquer visualizador. O diagrama Mermaid abaixo
> é a **fonte editável** — renderiza automaticamente no GitHub e em editores com
> suporte a Mermaid; em previews sem esse suporte ele aparece como código.

```mermaid
flowchart LR
    subgraph RES["🛢️ NÓ RESERVATÓRIO (caixa d'água) — Transmissor"]
        direction TB
        BOIA["Boia de nível<br/>(1–2 boias)"] --> ESP1["ESP32-C3"]
        ESP1 <-->|UART| E1["Módulo LoRa<br/>E220-900T30D/T22D"]
        BAT1["Bateria 18650<br/>+ TP4056"] --> PWR1["Failover<br/>fonte/bateria"]
        EXT1["Fonte externa<br/>5–12 V"] --> PWR1
        PWR1 --> ESP1
        PWR1 -->|ADC / I2C| ESP1
    end

    subgraph PUMP["⚙️ NÓ BOMBA (captação) — Receptor/Atuador"]
        direction TB
        E2["Módulo LoRa<br/>E220-900T30D/T22D"] <-->|UART| ESP2["ESP32-C3"]
        ESP2 --> RELE["Relé / Contator"] --> BOMBA["🚰 Bomba d'água"]
        BAT2["Bateria 18650<br/>+ TP4056"] --> PWR2["Failover<br/>fonte/bateria"]
        EXT2["Fonte externa<br/>5–12 V"] --> PWR2
        PWR2 --> ESP2
    end

    E1 <===>|"LoRa 900 MHz · ~5 km<br/>STATUS · COMANDO · ACK · HEARTBEAT"| E2

    classDef radio fill:#cfe8ff,stroke:#2a6cb3;
    classDef power fill:#ffe9c7,stroke:#b37a2b;
    class E1,E2 radio;
    class PWR1,PWR2,BAT1,BAT2,EXT1,EXT2 power;
```

## Fluxo de dados

1. O **reservatório** lê a boia e decide (com histerese) ligar/desligar a bomba.
2. Envia `COMANDO_BOMBA` por LoRa **com ACK**; retransmite se necessário.
3. A **bomba** aplica o comando no relé e responde `ACK`.
4. `HEARTBEAT` periódico mantém o enlace vivo; sua ausência aciona o **failsafe**
   (bomba desliga).
5. `BATERIA_STATUS` reporta tensão/corrente/estado de carga.

> Para o esquemático elétrico detalhado (TP4056, chaveamento, INA219, pinos),
> ver [`schematic.md`](schematic.md).
