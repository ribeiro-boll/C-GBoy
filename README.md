# C-GBoy

<img width="320" height="488" alt="image" src="https://github.com/user-attachments/assets/c954b6a4-6186-4d26-8434-3d68af4c29ac" />

Um emulador de **Nintendo Game Boy clássico (DMG apenas!!! - não Game Boy Color!!!)** desenvolvido do zero em **C**, utilizando **SDL2** para renderização, entrada e controle da janela.

Este é meu primeiro projeto envolvendo emulação. A ideia surgiu após uma longa semana de férias da faculdade sem ter projetos para fazer, dai pensei: "O quão dificil seria fazer isso?", e agora, aqui estamos.

O projeto implementa os principais subsistemas do console, incluindo a CPU **Sharp SM83**, barramento de memória, interrupções, timers, DMA, PPU, sprites, joypad e suporte a banking de ROM/RAM através de MBC **(apenas MBC1 e MBC5 até o momento)**.


Durante o desenvolvimento, limitei o uso de IA no mínimo possível, até porque a ideia deste projeto foi aprender como portatil funciona, desse modo, utilizando apenas para:
* auxílio na interpretação de documentação.
* correção de erros que me impediam de prosseguir no projeto por mais de 3 dias. (no geral, foram apenas bugs, nada de implementação)
* o desenvolvimento deste README.md.
* a criação da ROM com a logo do projet.

> **Estado do projeto:** em desenvolvimento.
>
> O emulador já é capaz de executar alguns jogos comerciais, mas ainda não possui precisão ciclo-a-ciclo e existem comportamentos do hardware original que precisam ser implementados ou revisados.

---

# Aviso sobre ROMs

Este repositório não inclui ROMs comerciais.

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
    - [7. APU](#7-apu)
- [Objetivo do projeto](#objetivo-do-projeto)
    - [Status](#status)

---

# Demonstrações
### The Legend of Zelda: Link's Awakening
<img width="320" height="488" alt="ezgif-5759ad37891f8ba8" src="https://github.com/user-attachments/assets/9e4792e9-58fa-412c-9235-ef68b63c552d" />


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

## Windows Download & Installation
[link](https://github.com/ribeiro-boll/C-GBoy/releases/download/v0.1.0/gameboy-windows.zip)

## Visão geral

O C-GBoy tenta reproduzir o funcionamento do Game Boy em um nível relativamente baixo, em vez de apenas simular o resultado final das instruções.

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

| Subsistema      | Estado               | Observações                                                                  |
| --------------- |----------------------|------------------------------------------------------------------------------|
| CPU SM83        | [x] Implementado     | tabela principal de opcodes implementada                                     |
| Prefixo `CB`    | [x] Implementado     | Rotates, shifts, `SWAP`, `BIT`, `RES` e `SET` possuem handlers               |
| Flags `Z N H C` | [x] Implementadas    | Manipulação centralizada no registrador `F`                                  |
| Stack           | [x] Implementada     | `PUSH`, `POP`, `CALL`, `RET` e vetores de interrupção                        |
| Interrupções    | [x] Implementadas    | VBlank, STAT, Timer, Serial e Joypad                                         |
| HALT            | [x] Implementado     | Inclui lógica relacionada ao HALT bug                                        |
| Timers          | [ ] Parcial          | DIV/TIMA/TMA/TAC implementados, mas ainda pode existir diferenças de timing  |
| DMA OAM         | [x] Implementado     | Transferência de 160 bytes                                                   |
| Memory Bus      | [x] Implementado     | Principais regiões disponíveis                                               |
| ROM Only        | [x] Suportado        | Cartuchos sem MBC                                                            |
| MBC1            | [x] implementado*    | Banking de ROM e RAM implementado, porém ainda requer testes mais precisos   |
| MBC2            | [ ] Não implementado | Estrutura reservada                                                          |
| MBC3            | [ ] Não implementado | Estrutura reservada                                                          |
| MBC5            | [x] implementado*    | Banking de ROM e RAM implementado, porém ainda requer testes mais precisos                                                          |
| PPU             | [x] implementado*    | Renderização baseada em scanlines funcional, porém ainda requer testes mais precisos |
| Background      | [x] Implementado     | Scroll através de SCX/SCY                                                    |
| Window          | [x] Implementada     | WX/WY e tile map configurável                                                |
| Sprites 8×8     | [x] Implementados    | Inclui flip, palette e prioridade                                            |
| Sprites 8×16    | [x] Implementados    | Seleção de pares de tiles                                                    |
| Joypad          | [x] Implementado     | Mapeado para teclado                                                         |
| Serial          | [ ] Stub             | Comportamento básico e solicitação de interrupção                            |
| Save RAM        | [x] Implementado     | Arquivo `.cartram` para MBC1                                                 |
| APU / Áudio     | [ ] Não implementado | Não existe subsistema de áudio atualmente                                    |
| Game Boy Color  | [ ] Não suportado    | Implementação voltada ao DMG                                                 |
| Boot ROM        | [ ] Não emulada      | Execução começa diretamente em `0x0100`, respeitando os direitos autorais    |

---

# Download e instalação para sistemas Windows

1. Faça o download baixando o ultimo lançamento/release do github ou utilizando o link ao lado: [último lançamento](https://github.com/ribeiro-boll/C-GBoy/releases/download/v0.1.0/gameboy-windows.zip)

2. Extraia em uma pasta (está pasta será o local de armazenamento das ROMs e dos Saves).

3. Crie um atalho do emulador, vá nas propriedades do atalho e no campo chamado: "Destino", digite o nome da ROM que será emulada, assim como a imagem abaixo:

 <img width="343" height="58" alt="image" src="https://github.com/user-attachments/assets/068ec355-e314-448e-8009-a531c74a3793" />

> [!WARNING]
>   1. **Não utilize espaços ou caracteres especiais no nome da ROM**.
>   2. **Caso queira usar uma pasta dentro do diretorio do emulador para armazenar as ROMS, siga a mesma regra acima para nomes, dito isto, digite da seguinte forma "nome_da_pasta/ROM_selecionada.gb"**.


4. Para rodar o C-GBoy e iniciar a emulação da ROM, apenas execute o atalho.

5. Não esqueça de dar uma olhada no esquema de controles do C-GBoy, pois para fechar o Emulador é apenas possivel via o controle oficial do emulador.

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

O tipo de cartucho é obtido diretamente do header da ROM:

```text
0x0147 - Cartridge Type
```

## ROM Only

Cartuchos com tipo `0x00` acessam diretamente `memory.game_rom`.

Não há troca de banco.

---

## MBC

Os tipos de cartucho MBC1 e MBC5 identificados pelo loader são normalizados internamente para o handler MBC.

A implementação do MBC1 e MBC5 ainda deve ser considerada experimental. Casos extremos de banking e diferenças entre os modos precisam de validação adicional contra hardware/documentação.

MBC2 e MBC3 ainda não possuem implementação funcional.

---

# Save RAM

Para cartuchos tratados como MBC1, a RAM externa é persistida em um arquivo auxiliar.

Se a ROM for:

```text
roms/game.gb
```

o save será armazenado como:

```text
roms/game.gb.cartram
```

O formato atual é uma cópia binária bruta dos quatro bancos internos:

```text
4 × 0x2000 bytes = 32768 bytes
```

O save é carregado durante a inicialização através de:

```c
load_game_save_on_start()
```

e escrito através de:

```c
persist_save_game_on_exit()
```

Também existe autosave periódico durante a execução.

O diretório onde a ROM está localizada deve possuir permissão de escrita para que o arquivo `.cartram` possa ser criado.

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

O controle de timing ainda é aproximado e não deve ser considerado uma implementação cycle-accurate.

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
└── MBC1

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

O emulador ainda está em desenvolvimento e possui diferenças importantes em relação ao hardware original.

Entre os pontos que ainda precisam de revisão estão:

* validação do arquivo de ROM antes de utilizar `fopen`, `fread` e `malloc`;
* validação do tamanho da ROM antes de acessos ao buffer;
* suporte completo aos diferentes tipos de cartridge header;
* implementação de MBC2;
* implementação de MBC3 e RTC;
* escrita correta na Echo RAM;
* comportamento individual dos registradores especiais de I/O;
* comportamento preciso de `DIV`;
* edge detection dos timers;
* atraso de overflow/reload do TIMA;
* revisão de alguns casos da ALU envolvendo carry;
* revisão completa de `HALT`, `EI` e interrupções;
* timing variável do Mode 3;
* restrições de acesso a VRAM/OAM dependendo do modo da PPU;
* comportamento do LCD ao ser ligado/desligado;
* STAT interrupt line com detecção real de rising edge;
* prioridades de sprites em todos os casos de sobreposição;
* timing preciso do DMA;
* comportamento do barramento durante DMA;
* Serial completo;
* APU;
* tratamento do evento `SDL_QUIT`;
* scaling da janela;
* validação de `argc`;
* tratamento de erros;
* testes automatizados;
* reorganização do projeto em módulos.

---

# Roadmap técnico

Uma possível ordem de evolução do projeto é:

### 1. CPU

* finalizar validação da tabela principal;
* validar todos os opcodes `CB`;
* revisar flags;
* revisar timings;
* finalizar comportamento de `HALT`, `STOP`, `EI`, `DI` e interrupções.

### 2. Memory Bus

* formalizar registradores de I/O;
* implementar corretamente Echo RAM;
* implementar regiões proibidas;
* adicionar restrições de acesso relacionadas à PPU e DMA.

### 3. Timers

* migrar para modelo baseado nos bits internos do DIV;
* implementar falling-edge do sinal selecionado;
* reproduzir overflow/reload do TIMA.

### 4. PPU

* revisar transições de modo;
* implementar STAT interrupt line;
* melhorar prioridade de objetos;
* implementar timing variável;
* validar Window e sprites contra test ROMs.

### 5. Cartridge
* implementar MBC2;
* implementar MBC3 + RTC;
* interpretar corretamente ROM size e RAM size pelo header.

### 6. Infraestrutura

* separar o código em módulos;
* criar sistema de testes automatizados;
* adicionar logs configuráveis;
* adicionar tratamento robusto de erros;
* criar configuração de controles;
* adicionar scaling da tela.

### 7. APU

Implementar os quatro canais do Game Boy:

```text
CH1 - Square + Sweep
CH2 - Square
CH3 - Wave
CH4 - Noise
```

e posteriormente integrar a saída com SDL Audio.

---

# Objetivo do projeto

Este projeto tem foco educacional e experimental.

Além de executar jogos, a implementação busca explorar diretamente conceitos como:

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

O projeto ainda **não deve ser considerado um emulador Game Boy COMPLETO ou cycle-accurate**.

A implementação atual já possui uma base significativa de CPU, memória, MBC1, PPU, sprites, DMA, timers e interrupções, mas ainda existem diferenças de comportamento e timing que podem afetar a compatibilidade com determinados softwares.

Contribuições, testes e estudos sobre o hardware DMG são bem-vindos.
