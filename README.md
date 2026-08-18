# C-GBoy

<img width="320" height="488" alt="image" src="https://github.com/user-attachments/assets/c954b6a4-6186-4d26-8434-3d68af4c29ac" />

Um emulador de **Nintendo Game Boy clássico (DMG apenas!!! — não Game Boy Color!!!)** desenvolvido do zero em **C**, utilizando **SDL2** para renderização, entrada e controle da janela.

Este é meu primeiro projeto envolvendo emulação. A ideia surgiu depois de uma longa semana de férias da faculdade sem encontrar nenhum projeto que realmente me animasse. Até que pensei: **"O quão difícil seria fazer um emulador do zero?"** E agora, aqui estamos.

O C-GBoy implementa os principais subsistemas do Game Boy DMG, incluindo a CPU **Sharp SM83**, barramento e mapa de memória, interrupções, timers, DMA, PPU, Background, Window, sprites, joypad, persistência de saves e banking de ROM/RAM através de **MBC1 e MBC5**.

Durante o desenvolvimento, limitei o uso de IA ao mínimo possível, porque a principal ideia deste projeto sempre foi estudar como o portátil funciona e implementar seus componentes por conta própria. O uso ficou restrito principalmente a:

* auxílio na interpretação de documentação;
* ajuda na investigação de bugs que me impediam de prosseguir por vários dias;
* desenvolvimento e revisão deste `README.md`;
* criação da ROM usada para exibir a logo do projeto.

> **Estado do projeto:** em desenvolvimento.
>
> O C-GBoy já executa jogos reais de Game Boy e possui CPU, memória, vídeo, entrada, timers, interrupções, DMA e suporte a cartuchos funcionais. Ainda existem detalhes de timing e comportamentos específicos do hardware a serem refinados, além de subsistemas que não fazem parte da implementação atual, como a APU.

# Aviso sobre ROMs e Boot ROM

Este repositório **não inclui ROMs comerciais** e **não inclui a BIOS/Boot ROM original do Game Boy**.

O C-GBoy inicia a execução diretamente em `0x0100`, utilizando um estado inicial definido pelo próprio emulador.

Utilize apenas ROMs, homebrews ou dumps que você tenha autorização legal para utilizar.

---

# Índice

- [Demonstrações](#demonstrações)
- [Controles](#controles)
- [Visão geral](#visão-geral)
- [Funcionalidades atuais](#funcionalidades-atuais)
- [Download e instalação para sistemas Windows](#download-e-instalação-para-sistemas-windows)
- [Requisitos Linux](#requisitos-linux)
- [Compilação](#compilação)
- [Execução](#execução)
- [Arquitetura da CPU](#arquitetura-da-cpu)
  - [Decodificação de opcodes](#decodificação-de-opcodes)
- [Stack](#stack)
- [Mapa de memória](#mapa-de-memória)
- [Cartridge / MBC](#cartridge--mbc)
  - [ROM Only](#rom-only)
  - [MBC1](#mbc1)
  - [MBC5](#mbc5)
- [Save RAM](#save-ram)
- [Interrupções](#interrupções)
- [HALT](#halt)
- [Timers](#timers)
- [DMA / OAM](#dma--oam)
- [PPU](#ppu)
  - [Modos da PPU](#modos-da-ppu)
- [Background](#background)
- [Window](#window)
- [Sprites](#sprites)
- [Paleta DMG](#paleta-dmg)
- [SDL2](#sdl2)
- [Sincronização](#sincronização)
- [Debug e testes internos](#debug-e-testes-internos)
- [Estado inicial](#estado-inicial)
- [Estrutura atual do projeto](#estrutura-atual-do-projeto)
- [Limitações conhecidas](#limitações-conhecidas)
- [Roadmap técnico](#roadmap-técnico)
  - [1. CPU](#1-cpu)
  - [2. Memory Bus](#2-memory-bus)
  - [3. Timers](#3-timers)
  - [4. PPU](#4-ppu)
  - [5. Cartridge](#5-cartridge)
  - [6. Infraestrutura](#6-infraestrutura)
- [Objetivo do projeto](#objetivo-do-projeto)
  - [Status](#status)
- [Agradecimentos](#agradecimentos)

# Demonstrações
### The Legend of Zelda: Link's Awakening
<img width="160" height="244" alt="ezgif-5759ad37891f8ba8" src="https://github.com/user-attachments/assets/9e4792e9-58fa-412c-9235-ef68b63c552d" />


### Tetris
<img width="160" height="244" alt="ezgif-5754264bdc134c25" src="https://github.com/user-attachments/assets/ba760557-561e-4080-83d8-ea43208b9105" />


### Pocket Monsters Midori
<img width="160" height="244" alt="ezgif-5a65b8ecf8475fc2" src="https://github.com/user-attachments/assets/6e0116a8-ecf8-422a-b852-cd9a24ea11d2" />



# Controles

| Game Boy                       | Teclado                |
| ------------------------------ | ---------------------- |
| D-Pad ↑                        | `↑`                    |
| D-Pad ↓                        | `↓`                    |
| D-Pad ←                        | `←`                    |
| D-Pad →                        | `→`                    |
| A                              | `A`                    |
| B                              | `S`                    |
| Select                         | `Z`                    |
| Start                          | `X`                    |
| Salvar e sair                  | `ESC` duas vezes       |
| Sair sem salvar explicitamente | `Backspace` duas vezes |

O estado dos botões é refletido no registrador `P1/JOYP` em `0xFF00`.

Os bits de entrada seguem a lógica ativa em nível baixo utilizada pelo Game Boy.

---

# Visão geral

O C-GBoy reproduz o funcionamento do Game Boy em um nível relativamente baixo, mantendo CPU, memória, PPU, timers, DMA e interrupções sincronizados a partir dos ciclos consumidos pelas instruções.

Cada instrução executada pela CPU retorna sua quantidade correspondente de ciclos. Esses ciclos são então utilizados para avançar outros componentes do sistema, como:

* timers `DIV` e `TIMA`;
* transferência DMA;
* estados da PPU;
* interrupções.

A implementação é organizada principalmente ao redor de três estruturas:

* `GB_reg`: estado da CPU, registradores e controle de execução;
* `GB_Memory`: mapa de memória, ROM, RAM e controladores de cartucho;
* `GB_PPU`: estado da unidade gráfica, scanlines, sprites e framebuffer.

A SDL2 funciona como a camada entre o hardware emulado e o sistema operacional, sendo utilizada para apresentar o framebuffer e receber as entradas do teclado.

Fluxo simplificado da emulação:

```text
                     +------------------+
                     |       ROM        |
                     +---------+--------+
                               |
                               v
+---------+      +-------------+-------------+
| Joypad  | ---> |     Barramento de         |
+---------+      |        memória             |
                 +------+------+-------------+
                        |      |
                   +----+      +------+
                   v                  v
              +---------+         +---------+
              |   CPU   |         |   PPU   |
              |  SM83   |         | / VRAM  |
              +----+----+         +----+----+
                   |                   |
                   | ciclos            | framebuffer
                   v                   v
            +-------------+       +---------+
            | Timer / DMA |       |  SDL2   |
            +-------------+       +---------+
```

A partir daqui, cada subsistema é detalhado individualmente nas próximas seções.

# Funcionalidades atuais

| Subsistema      | Estado                  | Observações |
| --------------- | ----------------------- | ----------- |
| CPU SM83        | [x] Implementado        | Tabela principal de opcodes implementada |
| Prefixo `CB`    | [x] Implementado        | Rotates, shifts, `SWAP`, `BIT`, `RES` e `SET` possuem handlers |
| Flags `Z N H C` | [x] Implementadas       | Manipulação centralizada no registrador `F` |
| Stack           | [x] Implementada        | `PUSH`, `POP`, `CALL`, `RET` e vetores de interrupção |
| Interrupções    | [x] Implementadas       | VBlank, STAT, Timer, Serial e Joypad |
| HALT            | [x] Implementado        | Inclui tratamento relacionado ao HALT bug |
| Timers          | [ ] Parcial             | DIV/TIMA/TMA/TAC funcionais, ainda com diferenças de timing em casos específicos |
| DMA OAM         | [x] Implementado        | Transferência de 160 bytes para OAM |
| Memory Bus      | [x] Implementado        | Principais regiões do espaço de endereçamento disponíveis |
| ROM Only        | [x] Suportado           | Cartuchos sem MBC |
| MBC1            | [x] Implementado        | Banking de ROM e RAM funcional |
| MBC2            | [ ] Não implementado    | Sem suporte no momento |
| MBC3            | [ ] Não implementado    | Sem suporte no momento; RTC também não implementado |
| MBC5            | [x] Implementado        | Banking de ROM de 9 bits e banking de RAM implementados |
| PPU             | [x] Implementada        | Renderização baseada em scanlines |
| Background      | [x] Implementado        | Scroll através de SCX/SCY |
| Window          | [x] Implementada        | WX/WY e tile map configurável |
| Sprites 8×8     | [x] Implementados       | Flip, palette e prioridade |
| Sprites 8×16    | [x] Implementados       | Seleção de pares de tiles |
| Joypad          | [x] Implementado        | Mapeado para teclado |
| Serial          | [ ] Stub                | Comportamento básico e solicitação de interrupção |
| Save RAM        | [x] Implementado        | Persistência em arquivo `.cartram` para os cartuchos atualmente suportados pelo sistema de save |
| APU / Áudio     | [ ] Não implementado    | O emulador atualmente funciona sem áudio |
| Game Boy Color  | [ ] Não suportado       | Implementação focada no hardware DMG |
| Boot ROM        | [ ] Não utilizada       | A BIOS/Boot ROM original não é incluída; execução começa em `0x0100` |

---

# Download e instalação para sistemas Windows

A versão compilada para Windows pode ser obtida pela página de releases do projeto.

**Download atual:** [C-GBoy v0.1.2 para Windows](https://github.com/ribeiro-boll/C-GBoy/releases/download/v0.1.3/gameboy-windows.zip)

## 1. Baixando e extraindo

1. Baixe o arquivo `.zip` da release.
2. Extraia todo o conteúdo para uma pasta de sua preferência.
3. Mantenha os arquivos distribuídos no pacote dentro da pasta extraída.
4. Coloque a ROM que deseja executar na mesma pasta do emulador ou em uma subpasta, por exemplo:

```text
C-GBoy/
├── gameboy.exe
├── ...
└── roms/
    └── jogo.gb
```

> [!WARNING]
> Para evitar problemas com a forma como o caminho é passado ao executável, prefira nomes de ROMs e pastas sem espaços ou caracteres especiais.

## 2. Executando pelo Prompt de Comando / PowerShell

Abra um terminal dentro da pasta do C-GBoy e informe o caminho da ROM como primeiro argumento:

```powershell
.\gameboy.exe roms\jogo.gb
```

Se a ROM estiver na mesma pasta do executável:

```powershell
.\gameboy.exe jogo.gb
```

## 3. Criando um atalho para uma ROM

Também é possível criar um atalho do Windows que abre diretamente um jogo específico:

1. Clique com o botão direito em `gameboy.exe`.
2. Escolha **Criar atalho**.
3. Clique com o botão direito no novo atalho e abra **Propriedades**.
4. No campo **Destino**, mantenha o caminho do executável e adicione o caminho da ROM depois dele.

Exemplo:

```text
"C:\C-GBoy\gameboy.exe" roms\jogo.gb
```

Se preferir manter cada ROM na pasta principal:

```text
"C:\C-GBoy\gameboy.exe" jogo.gb
```

A imagem abaixo mostra um exemplo da configuração do campo **Destino**:

<img width="343" height="58" alt="Exemplo de configuração de atalho no Windows" src="https://github.com/user-attachments/assets/068ec355-e314-448e-8009-a531c74a3793" />

Depois disso, basta abrir o atalho para iniciar o C-GBoy diretamente com aquela ROM.

## 4. Saves

Quando o cartucho utiliza RAM persistente e o tipo é suportado pelo sistema de save atual, o C-GBoy cria um arquivo auxiliar com extensão:

```text
.cartram
```

Esse arquivo deve permanecer acessível ao emulador para que o save possa ser carregado novamente.

## 5. Fechando o emulador

Consulte a seção [Controles](#controles):

- `ESC` duas vezes: salva e encerra;
- `Backspace` duas vezes: encerra sem salvar explicitamente.

> [!IMPORTANT]
> Nenhuma ROM comercial ou BIOS/Boot ROM do Game Boy acompanha a release do C-GBoy.

---

# Requisitos Linux

Para compilar o projeto são necessários:

* compilador C com suporte a C11/GNU11;
* SDL2;
* `pkg-config`;
* sistema operacional com suporte à SDL2.

---

### Debian / Ubuntu

```bash
sudo apt update
sudo apt install build-essential pkg-config libsdl2-dev
```

### Arch Linux

```bash
sudo pacman -S base-devel pkgconf sdl2
```

### Fedora

```bash
sudo dnf install gcc pkgconf-pkg-config SDL2-devel
```

---

# Compilação

Assumindo que o código principal esteja salvo como `main.c`:

```bash
gcc -std=gnu11 -O2 -Wall -Wextra main.c -o gameboy $(pkg-config --cflags --libs sdl2)
```

Para uma compilação voltada a debug:

```bash
gcc -std=gnu11 -O0 -g -Wall -Wextra main.c -o gameboy $(pkg-config --cflags --libs sdl2)
```

O projeto utiliza `strdup`, portanto `gnu11` é utilizado no exemplo acima para disponibilizar as extensões POSIX/GNU normalmente fornecidas pelo ambiente.

---

# Execução

O executável recebe o caminho de uma ROM como primeiro argumento:

```bash
./gameboy caminho/para/jogo.gb
```

Exemplo:

```bash
./gameboy roms/game.gb
```

Atualmente o programa assume que `argv[1]` existe. Executar o modo normal sem informar uma ROM não é suportado e deve ser tratado futuramente com validação de argumentos.

Nenhuma ROM comercial é distribuída com o projeto.

---

# Arquitetura da CPU

O estado da CPU é armazenado em `GB_reg`.

Registradores de 8 bits:

```text
A  F
B  C
D  E
H  L
```

Registradores especiais:

```text
SP - Stack Pointer
PC - Program Counter
```

Os pares tradicionais utilizados pela arquitetura são:

```text
AF
BC
DE
HL
```

O registrador `F` armazena as quatro flags da SM83:

```text
Bit:  7 6 5 4 3 2 1 0
      Z N H C 0 0 0 0
```

| Flag | Nome       | Função                            |
| ---- | ---------- | --------------------------------- |
| `Z`  | Zero       | Resultado da operação foi zero    |
| `N`  | Subtract   | Última operação foi uma subtração |
| `H`  | Half Carry | Carry/borrow entre os nibbles     |
| `C`  | Carry      | Carry/borrow da operação          |

Os quatro bits inferiores de `F` são mantidos zerados, inclusive durante `POP AF`.

---

## Decodificação de opcodes

A tabela de instruções é dividida de acordo com os nibbles do opcode.

Funções como:

```c
get_opcode_row()
get_opcode_collum()
check_operand_collumn()
```

fazem o dispatch para handlers especializados.

A tabela principal é dividida entre:

```text
check_operand_row_collum_0()
...
check_operand_row_collum_F()
```

O prefixo `0xCB` possui uma tabela independente:

```text
check_operand_row_collum_0_CB_PREFIX()
...
check_operand_row_collum_F_CB_PREFIX()
```

Essa tabela contém operações como:

```text
RLC
RRC
RL
RR
SLA
SRA
SRL
SWAP
BIT
RES
SET
```

incluindo operações sobre registradores e memória através de `HL`.

---

# Stack

A stack cresce em direção a endereços menores, seguindo o comportamento da CPU original.

Operações centrais:

```c
push_into_stack_16bit()
pop_from_stack_to_register_16bit()
pop_from_stack_for_emulator_use_16bit()
```

O emulador também suporta wraparound natural do registrador `SP`, já que ele é armazenado como `uint16_t`.

Isso permite, por exemplo:

```text
SP = 0x0000
PUSH -> SP = 0xFFFE
```

---

# Mapa de memória

O espaço de endereçamento é de 16 bits:

```text
0x0000 - 0xFFFF
```

O barramento é centralizado principalmente em:

```c
read_from_memory_8bit()
write_into_memory_8bit()
```

Mapa atual:

| Região              |   Intervalo | Implementação                               |
| ------------------- | ----------: | ------------------------------------------- |
| ROM Bank 0          | `0000-3FFF` | ROM / MBC                                   |
| ROM Bank N          | `4000-7FFF` | ROM / MBC                                   |
| VRAM                | `8000-9FFF` | `memory.VRAM`                               |
| External RAM        | `A000-BFFF` | Cart RAM / MBC                              |
| WRAM                | `C000-DFFF` | `memory.WRAM`                               |
| Echo RAM            | `E000-FDFF` | Leitura espelhada; escrita ainda incompleta |
| OAM                 | `FE00-FE9F` | `memory.OAM`                                |
| Área não utilizável | `FEA0-FEFF` | Sem armazenamento                           |
| I/O                 | `FF00-FF7F` | `memory.IO`                                 |
| HRAM                | `FF80-FFFE` | `memory.HRAM`                               |
| IE                  |      `FFFF` | `memory.IE`                                 |

---

# Cartridge / MBC

O tipo do cartucho é identificado a partir do header da ROM, principalmente pelo byte:

```text
0x0147 - Cartridge Type
```

O loader utiliza essa informação para encaminhar os acessos da região de cartucho ao mapper correspondente.

## ROM Only

Cartuchos do tipo ROM Only acessam diretamente o conteúdo carregado em `memory.game_rom`.

Nesse caso não existe troca de bancos através de MBC.

## MBC1

O C-GBoy possui suporte a MBC1, incluindo:

* habilitação da RAM externa;
* seleção dos bits inferiores do banco de ROM;
* seleção dos bits superiores;
* modos de banking;
* banking de RAM;
* acesso à RAM externa.

A implementação já executa jogos MBC1 e continua sendo validada em casos extremos de banking e timing.

## MBC5

O C-GBoy também possui suporte a MBC5.

O mapper utiliza um número de banco de ROM de **9 bits**, permitindo selecionar diretamente bancos na janela `0x4000-0x7FFF`, além de possuir seleção independente do banco de RAM externa.

A implementação atual cobre:

* habilitação de RAM;
* 8 bits inferiores do banco de ROM;
* 9º bit do banco de ROM;
* seleção independente do banco de RAM;
* leitura e escrita na RAM externa;
* cálculo do banco físico de acordo com o tamanho da ROM carregada.

Isso amplia bastante a compatibilidade do C-GBoy com jogos maiores e homebrews modernos que continuam compatíveis com o hardware DMG.

As variantes MBC5 com **rumble** ainda não possuem integração específica com um dispositivo de vibração.

MBC2 e MBC3 ainda não possuem implementação funcional.

---

# Save RAM

O C-GBoy possui persistência de RAM de cartucho através de arquivos auxiliares com extensão `.cartram`.

Se a ROM for:

```text
roms/game.gb
```

o arquivo de save correspondente utiliza o nome:

```text
roms/game.gb.cartram
```

O save é carregado durante a inicialização através de:

```c
load_game_save_on_start()
```

e persistido através de:

```c
persist_save_game_on_exit()
```

Também existe autosave periódico durante a execução.

A RAM de cartucho é mantida separada do restante da memória do Game Boy e respeita o banking do mapper correspondente.

O diretório utilizado para o arquivo `.cartram` precisa possuir permissão de escrita.

---

# Interrupções

São utilizados os registradores:

```text
FF0F - IF - Interrupt Flag
FFFF - IE - Interrupt Enable
```

Além do estado global:

```c
cpu.IME
```

As cinco interrupções da CPU são reconhecidas na ordem de prioridade do hardware:

| Bit | Interrupção |    Vetor |
| --: | ----------- | -------: |
|   0 | VBlank      | `0x0040` |
|   1 | LCD STAT    | `0x0048` |
|   2 | Timer       | `0x0050` |
|   3 | Serial      | `0x0058` |
|   4 | Joypad      | `0x0060` |

O processamento é realizado por:

```c
check_if_is_interrupted()
```

Quando uma interrupção é atendida:

1. o bit correspondente em `IF` é limpo;
2. `IME` é desabilitado;
3. o endereço atual é colocado na stack;
4. `PC` recebe o vetor correspondente.

Existe também estado dedicado para o atraso de habilitação relacionado à instrução `EI`.

---

# HALT

O estado da CPU possui:

```c
bool is_halted;
bool only_waiting_for_interrupt_cond;
bool halt_bug;
```

Quando a CPU está parada, o loop continua avançando os componentes dependentes de clock em incrementos de ciclos.

Há também tratamento específico para o **HALT bug**, preservando temporariamente o endereço do `PC` durante a condição.

Essa parte ainda está marcada no código para revisão do fluxo entre HALT, interrupções e execução da instrução seguinte.

---

# Timers

Os registradores utilizados são:

| Endereço | Registrador |
| -------- | ----------- |
| `FF04`   | DIV         |
| `FF05`   | TIMA        |
| `FF06`   | TMA         |
| `FF07`   | TAC         |

A CPU mantém acumuladores internos:

```c
contador_ciclos_div
contador_ciclos_tima
```

O DIV é incrementado a cada 256 ciclos acumulados.

O TIMA utiliza as quatro frequências selecionáveis pelo TAC:

```text
16 ciclos
64 ciclos
256 ciclos
1024 ciclos
```

Quando `TIMA` transborda:

```text
TIMA = TMA
```

e uma interrupção de Timer é solicitada.

### Limitações atuais

A implementação atual simplifica alguns detalhes do timer real.

Ainda precisam ser revisados:

* seleção da frequência utilizando estritamente os bits `1-0` de TAC;
* comportamento de edge detection;
* atraso real entre overflow do TIMA e reload de TMA;
* efeitos de escritas em DIV/TIMA/TMA/TAC em momentos específicos.

---

# DMA / OAM

Escrever em:

```text
FF46 - DMA
```

inicia uma transferência para OAM.

O endereço de origem é calculado como:

```text
valor << 8
```

e são transferidos:

```text
160 bytes
```

para:

```text
FE00-FE9F
```

A implementação considera:

```text
1 byte = 4 ciclos
```

O progresso da transferência é mantido através de:

```c
DMA_transfer_pending
DMA_transfer_curr_addr
DMA_transfer_limit_addr
DMA_transfer_OAM_addr
```

e atualizado por:

```c
DMA_transfer_Verify()
```

---

# PPU

A PPU trabalha com um framebuffer interno de:

```text
160 × 144 pixels
```

representado por:

```c
uint8_t LCDscreen[144][160];
uint8_t LCDscreen_pixel_color[144][160];
```

A segunda matriz preserva o índice de cor original do Background/Window e é utilizada na resolução de prioridade com sprites.

---

## Modos da PPU

Cada scanline utiliza atualmente o seguinte modelo:

```text
Mode 2 - OAM Scan       80 ciclos
Mode 3 - Drawing       172 ciclos
Mode 0 - HBlank        204 ciclos
--------------------------------
Total                  456 ciclos
```

Linhas:

```text
0-143   área visível
144-153 VBlank
```

No início do VBlank, o framebuffer é enviado ao renderer SDL2.

> O tempo do Mode 3 é atualmente fixo. O Game Boy real possui duração variável dependendo do pipeline da PPU, sprites, Window e outros fatores.

---

# Background

O renderer suporta os dois tile maps disponíveis pelo LCDC:

```text
9800-9BFF
9C00-9FFF
```

e as duas formas de endereçamento de tile data:

```text
8000-8FFF - IDs unsigned
8800-97FF - IDs signed relativos a 9000
```

SCX e SCY são utilizados para aplicar scrolling de 8 bits, incluindo wraparound no mapa de `256 × 256` pixels.

Registradores:

```text
FF42 - SCY
FF43 - SCX
```

---

# Window

A Window utiliza:

```text
FF4A - WY
FF4B - WX
```

O offset de hardware de `WX - 7` é considerado durante a renderização.

A linha interna da Window é acompanhada separadamente através de:

```c
ppu.current_LY_from_window
```

e só avança quando a Window participa efetivamente da scanline.

---

# Sprites

O OAM Scan percorre os 40 objetos possíveis na OAM e seleciona até:

```text
10 sprites por scanline
```

que é o limite do Game Boy DMG.

São suportados sprites:

```text
8 × 8
8 × 16
```

Atributos atualmente processados:

* prioridade BG/OBJ;
* Y flip;
* X flip;
* seleção OBP0/OBP1;
* tile ID;
* posição X/Y.

Os pixels transparentes dos sprites, cujo índice de cor é `0`, não sobrescrevem o framebuffer.

A prioridade em relação ao Background utiliza o índice de cor original salvo em `LCDscreen_pixel_color`.

---

# Paleta DMG

O projeto usa quatro tons verdes para representar a paleta monocromática do Game Boy:

```text
Cor 0: #C6E096
Cor 1: #9EC46C
Cor 2: #5A8846
Cor 3: #2A4E28
```

As cores são aplicadas através de:

```c
SDL_SetColor0()
SDL_SetColor1()
SDL_SetColor2()
SDL_SetColor3()
```

O mapeamento lógico dos pixels continua sendo feito pelos registradores de palette do Game Boy, como:

```text
FF47 - BGP
FF48 - OBP0
FF49 - OBP1
```

A conversão final do índice DMG para RGB ocorre apenas no renderer SDL.

---

# SDL2

A SDL2 é utilizada atualmente para:

* criação da janela;
* renderer 2D;
* desenho do framebuffer;
* leitura do teclado;
* temporização;
* apresentação dos frames.

O framebuffer interno mantém a resolução nativa do Game Boy:

```text
160 × 144
```

No estado atual do código, a janela SDL é criada como:

```c
SDL_CreateWindowAndRenderer(160, 244, ...);
```

enquanto a área efetivamente renderizada pela PPU continua sendo `160 × 144`.

Isso pode ser ajustado posteriormente ou substituído por uma estratégia de scaling.

---

# Sincronização

Cada opcode retorna sua duração em ciclos.

Exemplo conceitual:

```c
uint8_t ciclos = check_operand_collumn(opcode);

incrementar_ciclos(ciclos);
DMA_transfer_Verify(ciclos);
check_cycle_counter();
ppu_cycles_Verify();
```

Dessa forma, CPU, timer, DMA e PPU avançam utilizando a mesma unidade temporal.

Existe também uma tentativa de limitar a velocidade da emulação utilizando:

```c
SDL_GetPerformanceCounter()
SDL_Delay()
```

com alvo aproximado de:

```text
16.74 ms
```

por frame.

O controle de timing ainda é aproximado e não busca precisão cycle-accurate no estado atual.

---

# Debug e testes internos

O código possui um modo de testes controlado por:

```c
bool cond_debug = false;
```

Ao definir:

```c
bool cond_debug = true;
```

a execução normal da ROM é substituída por pequenos programas de teste carregados diretamente na memória.

Atualmente existem **14 testes internos**:

```text
ld_hl_indirect_completo
ld_indirect_wraparound
alu_bordas_flags
inc_dec_hl_mem_wrap
inc_dec_16bit_wrap
add_hl_16bit_bordas
add_sp_e8_ld_hl_sp_e8
rotates_carry_chain
daa_matriz
cpl_scf_ccf_flags
jp_jr_condicoes
call_ret_condicoes
push_pop_af_mask
stack_sp_wraparound
```

Eles cobrem casos como:

* acesso indireto através de `HL`;
* wraparound de endereços;
* flags da ALU;
* `INC` / `DEC`;
* aritmética de 16 bits;
* `ADD SP,e8`;
* rotations e carry;
* `DAA`;
* `CPL`, `SCF` e `CCF`;
* jumps condicionais;
* chamadas e retornos condicionais;
* chamadas aninhadas;
* stack;
* máscara do nibble inferior de `F`;
* wraparound do `SP`.

Os testes imprimem o estado dos registradores, flags, `PC` e contadores de ciclos durante a execução.

Esse mecanismo é útil para desenvolvimento, mas ainda não substitui uma suíte automatizada com asserts e testes externos de conformidade.

---

# Estado inicial

A execução da ROM começa diretamente em:

```text
PC = 0x0100
SP = 0xFFFE
```

Ou seja, a Nintendo Boot ROM não é executada.

Os registradores gerais são inicialmente zerados pelo emulador.

O LCDC é inicializado como:

```text
FF40 = 0x91
```

Esse modelo é simplificado e não reproduz integralmente o estado do hardware após a execução da boot ROM.

---

# Estrutura atual do projeto

No estado atual, a implementação está concentrada em um único arquivo C:

```text
.
├── main.c
└── README.md
```

Internamente, porém, o código já possui separação lógica entre:

```text
CPU
├── registradores
├── flags
├── ALU
├── opcodes
├── prefixo CB
├── stack
└── interrupções

Memory Bus
├── ROM
├── VRAM
├── Cartridge RAM
├── WRAM
├── Echo RAM
├── OAM
├── I/O
├── HRAM
└── IE

Cartridge
├── MBC1
└── MBC5

PPU
├── timing
├── LCDC / STAT
├── OAM Scan
├── Background
├── Window
├── sprites 8×8
└── sprites 8×16

Periféricos
├── Timer
├── DMA
├── Joypad
└── Serial

Frontend
└── SDL2
```

Uma evolução natural do projeto é transformar essas divisões lógicas em módulos `.c/.h` independentes.

---

# Limitações conhecidas

O C-GBoy está em desenvolvimento e ainda existem comportamentos do hardware original que podem ser refinados.

Entre os principais pontos pendentes estão:

* validação mais robusta do arquivo de ROM e do header;
* suporte completo aos diferentes tipos de cartucho;
* implementação de MBC2;
* implementação de MBC3 e RTC;
* suporte específico às variantes MBC5 com rumble;
* escrita e comportamento completos da Echo RAM;
* comportamento individual de todos os registradores especiais de I/O;
* comportamento mais preciso de `DIV`;
* edge detection dos timers;
* atraso de overflow/reload do TIMA;
* revisão de casos específicos da ALU e flags;
* refinamento de `HALT`, `EI` e interrupções em casos extremos;
* timing variável do Mode 3;
* restrições exatas de acesso a VRAM/OAM de acordo com o modo da PPU;
* comportamento do LCD ao ser ligado/desligado;
* STAT interrupt line com detecção precisa de rising edge;
* refinamento de prioridade de sprites em todos os casos de sobreposição;
* timing mais preciso do DMA;
* comportamento do barramento durante DMA;
* Serial completo;
* APU / áudio;
* tratamento completo do evento `SDL_QUIT`;
* scaling configurável da janela;
* tratamento de erros mais robusto;
* ampliação dos testes automatizados;
* reorganização do projeto em módulos separados.

Essas limitações afetam principalmente precisão de hardware e compatibilidade com casos específicos; elas não alteram o fato de que o C-GBoy já executa ROMs reais utilizando os subsistemas implementados.

---

# Roadmap técnico

O foco atual é aumentar a compatibilidade e refinar os componentes já implementados.

### 1. CPU

* ampliar validação da tabela principal;
* validar todos os opcodes `CB` em casos extremos;
* revisar flags e timings;
* refinar `HALT`, `STOP`, `EI`, `DI` e interrupções.

### 2. Memory Bus

* formalizar o comportamento dos registradores de I/O;
* completar Echo RAM;
* implementar corretamente regiões proibidas;
* adicionar restrições de acesso relacionadas à PPU e DMA.

### 3. Timers

* migrar para um modelo mais fiel baseado nos bits internos do DIV;
* implementar falling-edge do sinal selecionado;
* reproduzir com maior precisão overflow/reload do TIMA.

### 4. PPU

* revisar transições de modo;
* refinar STAT interrupt line;
* melhorar casos extremos de prioridade de objetos;
* implementar timing variável do Mode 3;
* ampliar validação de Window e sprites.

### 5. Cartridge

* ampliar testes do MBC1 e MBC5;
* implementar MBC2;
* implementar MBC3 + RTC;
* utilizar de forma mais completa os campos de ROM size e RAM size do header;
* tratar variantes de MBC5 com rumble.

### 6. Infraestrutura

* separar o código em módulos `.c/.h`;
* ampliar o sistema de testes automatizados;
* adicionar logs configuráveis;
* melhorar tratamento de erros;
* criar configuração de controles;
* adicionar scaling da tela.

> A APU não faz parte do roadmap atual. O C-GBoy continuará funcionando sem áudio enquanto o foco do projeto permanecer em CPU, vídeo, memória, cartuchos e compatibilidade.

---

# Objetivo do projeto

Este projeto tem foco educacional e experimental, mas o resultado é um **emulador funcional de Game Boy DMG**.

Além de executar jogos, o desenvolvimento do C-GBoy explora diretamente conceitos como:

* arquitetura de CPUs de 8 bits;
* interpretação de instruction sets;
* manipulação de flags;
* endianness;
* stack;
* memory-mapped I/O;
* banking de memória;
* interrupções;
* DMA;
* geração de vídeo baseada em ciclos;
* tile maps;
* sprites;
* sincronização entre subsistemas;
* desenvolvimento de emuladores.

Por esse motivo, diversas partes são implementadas explicitamente em vez de abstraídas por bibliotecas externas.

## Status

O C-GBoy é um **emulador de Game Boy DMG em desenvolvimento**, com CPU SM83, memória, PPU, sprites, DMA, timers, interrupções, joypad, MBC1 e MBC5 implementados.

Ele já executa jogos reais e homebrews compatíveis com os recursos atualmente suportados.

O projeto ainda possui diferenças de timing e alguns subsistemas ausentes — principalmente áudio e suporte ao Game Boy Color —, mas essas são limitações da implementação atual, não uma mudança na natureza do projeto.

Contribuições, testes e estudos sobre o hardware DMG são bem-vindos.

# Agradecimentos

Os sites abaixo foram utilizados como referência durante o desenvolvimento do projeto:

* Pan Docs: https://gbdev.io/pandocs/
* Game Boy Assembly Instruction Set: https://rgbds.gbdev.io/docs/v0.9.3/gbz80.7
* Game Boy Assembly / Machine Code Reference: https://gbdev.io/gb-opcodes/optables/
