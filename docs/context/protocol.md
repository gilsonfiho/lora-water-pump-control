# Protocolo

A especificação (quadro, tipos de mensagem, ACK, retransmissão) fica em
[`../ARCHITECTURE.md`](../ARCHITECTURE.md#protocolo-de-comunicação). Este arquivo
guarda só o que **muda ou está em aberto**, para não duplicar a especificação.

## Restrições de projeto

- Uma transação confiável em voo por vez (suficiente para 2 nós).
- Só `COMANDO_BOMBA` exige ACK; heartbeat e telemetria não.
- Qualquer alteração no formato do quadro exige subir `VER` e casar os dois nós.

## Em aberto

- **Feedback da bomba (fora do protótipo).** O ACK atual só prova que o rádio do nó da bomba recebeu
  o comando. O produto exige confirmar que a bomba **realmente ligou**
  (SCT-013). Definir mensagem para isso, por exemplo um `STATUS_BOMBA` com
  estado e corrente medida, enviado pelo nó da bomba, e o que o reservatório faz
  se o comando foi aceito mas não há corrente.
- **Repetidor (SKU 3).** O quadro tem `SRC`/`DST` de 1 byte, mas não há campo de
  salto nem regra de retransmissão. Definir antes de implementar o SKU.
- **Taxa aérea.** O LLCC68 limita o SF a 9 com BW de 125 kHz. Revisar o padrão de
  2,4 kbps após o teste de campo.
- **Papéis por strap.** Com firmware único, endereços `SRC`/`DST` deixam de ser
  fixos por ambiente de build e passam a depender do papel lido no boot.

## Histórico de mudanças

- Nenhuma mudança desde a `v0.0.1`.
