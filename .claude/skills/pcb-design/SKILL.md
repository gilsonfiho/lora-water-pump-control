---
name: pcb-design
description: Regras e fluxo de trabalho para projetar as PCBs deste projeto (emissor e receptor LoRa, KiCad 10, ESP32-C3 + E220). Use sempre que for criar ou alterar esquematico, footprint, 3D, posicionamento, roteamento ou saidas de fabricacao em hardware/.
---

# Skill: projeto de PCB (KiCad 10)

Vale para tudo em `hardware/`. Complementa `CLAUDE.md` e `hardware/docs/decisoes.md`
(fonte das decisoes). Convencoes do repositorio: PT-BR, commits `tipo(escopo): msg` sem
acento e **sem trailer de co-autor**, marcacao `TODO(hw)`, tags de hardware `hw-vX.Y`.

## Regras que nao se negociam

1. **Seguranca primeiro.** Saida da bomba desligada em reset, boot e pino flutuante
   (pull-down no driver). Sem tensao de rede na placa sem decisao explicita do autor.
2. **Nao inventar dado.** Pinagem, footprint, corrente e valor saem de datasheet, com
   documento e pagina em `decisoes.md`. Falta datasheet? Pedir ou baixar (cada download
   com origem e tamanho, registrado em `SOURCES.md`). Dimensao lida de figura de baixa
   resolucao leva `TODO(hw)` e pede conferencia no modulo fisico.
3. **O E220 transmite no boot e nunca fica sem antena.** Aviso na serigrafia e no
   esquematico. A antena e o SMA-K do proprio modulo: a PCB so define a posicao.
4. **Sem aprovar, nao avanca de etapa.** Decisao visual (antena, RF, isolacao) pede o
   autor abrindo o KiCad, dizendo o que conferir.
5. **Arquivos de terceiros baixados nao vao ao Git** (`libs/vendor/`, `docs/datasheets/`).

## Layout: criterios de qualidade

- **Compacto e organizado:** agrupar por funcao (MCU, alimentacao, radio, E/S),
  conectores nas bordas com a entrada do cabo voltada para fora, sem componente alto
  sob o modulo E220 (fica 4,2 mm acima da placa; so pecas baixas e com folga de 3D).
- **Antena do ESP32-C3:** modulo rente a borda, antena para fora da placa, keepout do
  footprint respeitado (sem cobre, trilha, via nem componente), longe do E220 e do SMA.
- **GND:** plano continuo nas duas faces, vias de costura, retorno curto sob sinais
  rapidos e RF. Nada de plano partido sob o radio.
- **Desacoplamento junto ao pino** (100 nF no pino de alimentacao, volume perto do
  consumo). O E220 T30D puxa 620 mA de pico em TX: trilha de VCC_RF larga e curta.
- **Trilhas:** sinal 0,25 mm; potencia dimensionada pela corrente (5 V / VCC_RF:
  >= 0,8 mm; conferir pela corrente real). Nao descer abaixo de 0,2 mm / 0,2 mm sem
  checar as regras da fabrica (JLCPCB: conferir no site, nao de memoria).
- **Serigrafia legivel:** referencias pequenas e fora de pads, polaridade e pino 1
  marcados, funcao dos bornes e jumpers escrita, papel (`JP1`), versao e data.
- **2 camadas.** So propor 4 se for realmente necessario, justificando.
- **Estrapping do C3:** GPIO2 com pull-up, GPIO8 e GPIO9 em 1 no boot (ver `decisoes.md`).

## Fluxo e verificacao (sempre)

1. Esquematico e PCB saem dos scripts em `hardware/tools/` (`build_sch.py`,
   `build_pcb.py`). Alterar o script, nao o arquivo gerado, enquanto o layout for gerado.
2. `kicad-cli sch erc` e `kicad-cli pcb drc` **a cada etapa**; so avancar com zero erros e
   cada aviso explicado. Netlist exportada para conferir pino a pino.
3. Gerar render 3D (`kicad-cli pcb render`) e olhar a imagem: sobreposicao, orientacao
   de conector, antena. Exportar STEP.
4. Atualizar `hardware/docs/decisoes.md` (decisao + motivo + fonte) e `CHANGELOG.md`.
5. Um commit por etapa. Nada de `git push` sem pedido.

## Armadilhas ja encontradas

- O KiCad 10 nao traz simbolo nem footprint do ESP32-C3-MINI-1 nem do E220: sao proprios
  (`hardware/libs/`). O STEP do USB-C HRO nao vem na instalacao: modelo de envelope.
- Strings com `\\` em scripts Python no Windows viram caracteres de controle: gravar
  caminhos com cuidado e conferir o arquivo.
- O KiCad aberto reescreve `.kicad_pro` e cria `.history/` e `.lck`: estao no `.gitignore`.
- Regenerar sobrescreve edicao manual no KiCad: avisar antes de rodar o gerador.
