# Protocolo

A especificação (quadro, tipos de mensagem, ACK, retransmissão) fica em
[`../ARCHITECTURE.md`](../ARCHITECTURE.md#protocolo-de-comunicação). Este arquivo
guarda só o que **muda ou está em aberto**, para não duplicar a especificação.

## Restrições de projeto

- Uma transação confiável em voo por vez (suficiente para 2 nós).
- Só `COMANDO_BOMBA` exige ACK; heartbeat e telemetria não. Inclui o
  `BOMBA_STATUS`: o laço do atuador não pode esperar confirmação, e perder um
  status não afeta a segurança.
- Qualquer alteração no formato do quadro exige subir `VER` e casar os dois nós.

## Em aberto

- **Confirmação por corrente (fora do protótipo).** O `BOMBA_STATUS` (0x06) já
  reporta o estado do atuador, mas isso é o que o firmware *acha* que fez. O
  produto exige comprovar que a bomba **realmente consome** (SCT-013). Falta
  acrescentar a corrente medida ao payload e definir o que o reservatório faz se
  o comando foi aceito e não há corrente.
- **Reação do reservatório ao bloqueio da bomba.** Hoje o `BOMBA_STATUS` só
  alimenta log. Em aberto: o reservatório deve parar de insistir em LIGAR,
  sinalizar ao usuário, ou os dois.
- **Repetidor (SKU 3).** O quadro tem `SRC`/`DST` de 1 byte, mas não há campo de
  salto nem regra de retransmissão. Definir antes de implementar o SKU.
- **Taxa aérea.** O LLCC68 limita o SF a 9 com BW de 125 kHz. Revisar o padrão de
  2,4 kbps após o teste de campo.
- **Papéis por strap.** Com firmware único, endereços `SRC`/`DST` deixam de ser
  fixos por ambiente de build e passam a depender do papel lido no boot.

- **Gestão/rotação de chave (issue #6).** O MAC usa hoje uma **chave única e
  estática** (`config/SecurityConfig.h`), igual em todos os nós e SKUs. Falta
  definir provisão por nó/fábrica, rotação e o que fazer se a chave vazar.

## Histórico de mudanças

- **2026-10-11:** quadro `VER 0x01` → `0x02`, com contador monotônico de 4 bytes
  (anti-replay, por `src`) e MAC de 8 bytes (SipHash-2-4, chave de 128 bits) —
  autenticação (#5) e anti-replay (#7). O MAC cobre de SYNC ao payload; o CRC
  segue cobrindo tudo, só para enquadramento. `decodePacket()` passou a exigir a
  chave e a devolver `DecodeError`. **Formato incompatível:** os dois nós
  precisam da mesma versão e da mesma chave. O contador é atribuído por
  transmissão física, então retransmissão legítima não vira replay.
- **2026-10-10:** novo tipo `BOMBA_STATUS` (0x06), bomba → reservatório, com
  estado do nó, bloqueio por tempo máximo, saída e segundos ligada (4 bytes).
  Informativo, sem ACK. `VER` do quadro não mudou **nessa** data: o tipo novo
  não altera o formato, e um nó antigo apenas ignora o tipo desconhecido.
