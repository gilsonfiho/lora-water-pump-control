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

## Histórico de mudanças

- **2026-10-10:** novo tipo `BOMBA_STATUS` (0x06), bomba → reservatório, com
  estado do nó, bloqueio por tempo máximo, saída e segundos ligada (4 bytes).
  Informativo, sem ACK. `VER` do quadro **não** mudou: o formato é o mesmo e um
  nó antigo simplesmente ignora o tipo desconhecido.
