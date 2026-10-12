# Decisões de hardware

Formato: data, decisão, motivo. Mais recentes no topo.

## 2026-10-11: estrutura do projeto

- Branch `feature/pcb`, a partir da `develop` (já atualizada com a
  `origin/develop`), e não da `main`: a `develop` tem o trabalho mais recente.
- Projeto em `hardware/`, com `libs/`, `fab/` e `docs/`. Bibliotecas ficam
  versionadas; backups, autosave e `*.kicad_prl` ficam no `.gitignore`.
- Tags de hardware `hw-vX.Y`.

## Em aberto (bloqueiam o esquemático)

- Fabricante (JLCPCB, PCBWay ou outro) e tipo de montagem (SMT ou manual).
- Caixa, tamanho alvo da placa e posição do SMA.
- Rádio: T22D, T30D ou os dois footprints sobrepostos.
- Fonte externa: 5 V, 12 V ou ambas.
- Medição de bateria: divisor + ADC ou INA219/226.
- Saída da bomba: contato seco em bornes ou relé de potência na placa.
- Boias: 1 ou 2.
- Mapa de pinos final (faltam 14 sinais para 8 GPIOs livres; ver conversa de
  2026-10-11) e se o console migra para o USB Serial/JTAG.
- Ferramentas: KiCad e leitor de PDF ainda não instalados.
