# 🍺 FermenStation

![PlatformIO](https://img.shields.io/badge/platformio-esp32-blue?logo=platformio)
![License](https://img.shields.io/badge/license-MIT-green)
![Status](https://img.shields.io/badge/status-em%20desenvolvimento-yellow)

> **Automação e monitoramento para fermentação artesanal com ESP32**

---

## 📸 Visão Geral

```mermaid

flowchart TD

    subgraph ESP32 [Dispositivo FermenStation]
        A[Sensor de Temperatura DS18B20] --> B{Logica de Controle e Leitura};
        B --> C[Reles Aquecimento, Resfriamento, Degelo];
        B --> D[Memoria Persistente Preferences];
        D -- Credenciais Wi-Fi, Device ID, Process ID --> B;
        B -- Modo AP Provisionamento --> E[Servidor Web HTTP];
        B -- Modo Station Operacao --> F[Conexao Wi-Fi Internet];
    end

    subgraph FlutterFlow App [Interface do Usuario]
        G[Tela de Provisionamento] --> H{Scan Wi-Fi e Conexao Manual};
        H -- Conexao Manual Usuario --> E;
        H -- Requisicoes HTTP Configuracoes --> E;
        I[Dashboard de Monitoramento] --> J{Gerenciamento de Receitas};
        I --> K{Visualizacao de Dados Historicos};
    end

    subgraph Supabase [Backend como Servico]
        L[Tabelas dispositivos, lote_receita, processos_ativos, historico_leituras] --> M[Funcoes RPC rpc_validate_device, rpc_get_active_process, rpc_controlar_fermentacao, etc.];
        M --> L;
    end

    E -- Requisicoes HTTP GET/POST --> G;
    F -- Requisicoes HTTPS RPCs --> M;
    M -- Respostas RPC --> F;
    M -- Dados CRUD --> I;
    J -- Atualiza dados --> L;
    K -- Consulta dados --> L;

    style A fill:#f9f,stroke:#333,stroke-width:2px
    style B fill:#bbf,stroke:#333,stroke-width:2px
    style C fill:#f9f,stroke:#333,stroke-width:2px
    style D fill:#f9f,stroke:#333,stroke-width:2px
    style E fill:#ccf,stroke:#333,stroke-width:2px
    style F fill:#ccf,stroke:#333,stroke-width:2px
    style G fill:#9cf,stroke:#333,stroke-width:2px
    style H fill:#9cf,stroke:#333,stroke-width:2px
    style I fill:#9cf,stroke:#333,stroke-width:2px
    style J fill:#9cf,stroke:#333,stroke-width:2px
    style K fill:#9cf,stroke:#333,stroke-width:2px
    style L fill:#cff,stroke:#333,stroke-width:2px
    style M fill:#cff,stroke:#333,stroke-width:2px



```


<p align="center">
  <img src="documentacao/fermenstation_diagrama.png" alt="Diagrama do Projeto" width="400"/>
  <br/>
  <i>Diagrama ilustrativo do sistema FermenStation</i>
</p>

---

## 🚀 Funcionalidades
- Monitoramento de temperatura em múltiplos pontos
- Controle de relés para aquecimento/resfriamento
- Interface web para configuração e visualização de dados
- Integração com Supabase para armazenamento remoto
- Log de eventos e leituras

---

## 🛠️ Tecnologias Utilizadas
- ESP32 (NodeMCU-32S)
- PlatformIO
- DallasTemperature & OneWire
- ArduinoJson
- ESPAsyncWebServer
- Supabase

---

## 📂 Estrutura de Pastas
```
FermenStation/
  ├─ src/                # Código-fonte principal
  ├─ include/            # Headers
  ├─ lib/                # Bibliotecas locais
  ├─ documentacao/       # Documentação e imagens
  ├─ test/               # Testes
  └─ platformio.ini      # Configuração do PlatformIO
```

---

## 🖼️ Exemplos de Tela

<p align="center">
  <img src="documentacao/tela_dashboard.png" alt="Dashboard" width="350"/>
  <img src="documentacao/tela_config.png" alt="Configuração" width="350"/>
</p>

---

## 📦 Como começar

1. **Clone o repositório:**
   ```bash
   git clone https://github.com/seuusuario/FermenStation.git
   ```
2. **Abra no PlatformIO (VSCode):**
3. **Conecte seu ESP32 via USB.**
4. **Compile e faça upload:**
   ```bash
   pio run --target upload
   ```

---

## 🔑 Configuração de Segredos

1. Copie o arquivo `include/secrets_example.h` para `include/secrets.h`
2. Preencha com sua URL e chave do Supabase.

---

## 🤝 Contribua
Sinta-se à vontade para abrir issues, enviar PRs ou sugerir melhorias!

---

## 📄 Licença
Distribuído sob a licença MIT. Veja `LICENSE` para mais informações.

---

<p align="center">
  <img src="documentacao/fermentador_icon.png" alt="Fermentador" width="80"/>
</p>

---

## 🔑 Configuração de Segredos

Para proteger suas chaves e dados sensíveis:

1. Copie o arquivo `include/secrets_example.h` para `include/secrets.h`:
   ```bash
   cp include/secrets_example.h include/secrets.h
   ```
2. Edite `include/secrets.h` e preencha com sua URL e chave do Supabase.
3. O arquivo `include/secrets.h` já está no `.gitignore` e **não será enviado ao repositório**.

---
