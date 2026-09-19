# Contexto do projeto

Notas curtas para quem (pessoa ou IA) vai trabalhar no projeto. Registram o que
**não dá para descobrir lendo o código**: decisões, restrições e o motivo das
escolhas. Não repetem o que o código ou a documentação principal já dizem.

| Arquivo | Conteúdo |
|---------|----------|
| [`status.md`](status.md) | O que está feito, em andamento e o próximo passo. Atualizar a cada sessão. |
| [`bancada.md`](bancada.md) | Montagem, pinagem e roteiro do protótipo de bancada. |
| [`hardware.md`](hardware.md) | Pinagem, modelos dos módulos e valores confirmados no hardware real. |
| [`protocol.md`](protocol.md) | Mudanças e dúvidas sobre o protocolo (a especificação está em `ARCHITECTURE.md`). |
| [`decisions.md`](decisions.md) | Registro de decisões, com data e motivo. |

Documentação principal já existente:

- [`../datasheets/`](../datasheets/): manuais dos módulos E220-900T22D e E220-900T30D.
- [`../ARCHITECTURE.md`](../ARCHITECTURE.md): camadas, protocolo, máquinas de estado, failsafe.
- [`../../README.md`](../../README.md): visão geral, hardware, build e pendências `TODO(hw)`.
- [`../../CHANGELOG.md`](../../CHANGELOG.md): histórico de versões.

## Regras de manutenção

- Um assunto por arquivo; se crescer demais, dividir.
- Se algo passa a valer no código, remover daqui ou apontar para o arquivo.
- Datas em formato absoluto (AAAA-MM-DD), nunca "semana passada".
