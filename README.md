# Projeto PH ORP: Monitoramento W218 via ESP32-C3

Este repositório contém a implementação completa para integração local do monitor de qualidade de água Tuya W218 (8-em-1) utilizando o ecossistema ESPHome. A solução substitui o módulo Wi-Fi original (WB3S) por um ESP32-C3, eliminando a dependência de nuvem e permitindo monitoramento local de alta performance.

## Arquitetura e Hardware

O monitor W218 utiliza um microcontrolador interno (MCU) para a leitura analógica dos sensores, comunicando-se com o módulo de rede via protocolo serial Tuya V3. Para a integração, o módulo original foi removido e substituído por um ESP32-C3 Super Mini.

### Pinagem de Conexão
| WB3S (Original) | ESP32-C3 Super Mini | Função |
| :--- | :--- | :--- |
| VCC (3.3V) | 3.3V | Alimentação |
| GND | GND | Terra |
| TX | GPIO 21 | Transmissão (ESP -> MCU) |
| RX | GPIO 20 | Recepção (MCU -> ESP) |

## O Caminho para o Handshake Perfeito (Engenharia Reversa)

A maior dificuldade deste projeto foi vencer o "deadlock" de comunicação do W218. Abaixo, detalhamos o que aprendemos durante o processo de tentativa e erro:

### O que falhou (Tentativas Frustradas)

*   **Status WiFi 0x03**: Inicialmente, reportamos o status `0x03` (Conectado ao WiFi), que é o comportamento padrão de muitos módulos Tuya. No entanto, o MCU do W218 ignorava essa mensagem e permanecia em modo de espera, nunca enviando os pacotes de dados (Data Points).
*   **Versionamento 0x03 no Cabeçalho**: Embora o MCU envie pacotes identificando-se como versão `0x03`, tentar responder utilizando esse mesmo byte de versão no cabeçalho resultava em descarte de pacotes pelo MCU.
*   **Sincronização Passiva**: Esperar que o MCU solicitasse dados ou iniciasse o handshake provou-se ineficaz, resultando em silêncio absoluto na UART após alguns segundos de boot.

### A Solução (O que funcionou)

Para "desbloquear" o MCU e fazê-lo transmitir os 8 sensores continuamente, implementamos as seguintes lógicas:

1.  **A "Mentira" Necessária (Status 0x04)**: O segredo para destravar o fluxo de dados é reportar o status `0x04` (Conectado à Nuvem). O MCU do W218 só libera os dados se ele acreditar que o módulo de rede tem uma conexão estabelecida com os servidores da Tuya.
2.  **Cabeçalho Fallback (0x00)**: Descobrimos que, independentemente da versão que o MCU reporta, ele sempre aceita respostas com o byte de versão `0x00`. Esta é a forma mais estável de comunicação para este hardware específico.
3.  **Heartbeat Proativo**: O ESP32-C3 envia um comando de Heartbeat (`0x00`) a cada 10 segundos. Isso evita que o MCU entre em modo de economia ou reinicie o processo de busca de rede.
4.  **Sincronização de Tempo (Time Sync)**: O dispositivo exige sincronização de tempo (`CMD 0x24`). Ao responder com a hora correta obtida via SNTP, o MCU estabiliza o envio dos DPs de pH e ORP, que são os mais sensíveis.

## Otimizações de Firmware e Estabilidade

1.  **Gerenciamento de Sockets**: Aumentamos a tabela de sockets do lwIP para 16 (`CONFIG_LWIP_MAX_SOCKETS: 16`). Isso resolve o erro crítico de "Socket Exhaustion" (ENFILE/errno 23) que travava o dispositivo anteriormente.
2.  **Redução de Carga**: O componente `web_server` foi desativado para priorizar a memória RAM e a estabilidade da API nativa do ESPHome.
3.  **Latência**: O modo `power_save_mode: none` garante que os handshakes de criptografia sejam processados sem atrasos.

## Mapeamento de Data Points (DP IDs)

| Sensor | ID | Multiplicador | Unidade |
| :--- | :--- | :--- | :--- |
| pH | 106 | 0.01 | pH |
| ORP | 131 | 1.0 | mV |
| Temperatura | 8 / 108 | 0.1 | °C |
| TDS | 126 | 1.0 | ppm |
| EC | 116 | 0.01 | mS/cm |
| Salinidade | 121 | 1.0 | ppm |
| Fator CF | 136 | 0.1 | CF |

## Licença

Este projeto é distribuído sob a licença MIT. Consulte o arquivo `LICENSE` para mais detalhes.
