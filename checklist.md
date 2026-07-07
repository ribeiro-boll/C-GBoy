# Game Boy Emulator Checklist

Checklist pequena para guiar o desenvolvimento inicial do emulador de Game Boy.

## Objetivo atual

Fazer o emulador sair da fase de testes simples e começar a executar código real de ROM, com foco inicial em jogos ROM ONLY, como Tetris.

A ideia é primeiro fazer funcionar de forma simples e depois melhorar a organização, precisão e compatibilidade.

---

## 1. Stack básica

* [x] Implementar `push16(value)`
* [x] Implementar `pop16()`
* [x] Implementar `PUSH BC`
* [x] Implementar `PUSH DE`
* [x] Implementar `PUSH HL`
* [x] Implementar `PUSH AF`
* [x] Implementar `POP BC`
* [x] Implementar `POP DE`
* [x] Implementar `POP HL`
* [x] Implementar `POP AF`
* [x] Garantir que `POP AF` mascara `F` com `0xF0`

Regra principal:

```text
PUSH: SP desce e escreve na memória
POP: lê da memória e SP sobe
```

---

## 2. Controle de fluxo

* [x] Implementar `JP a16`
* [x] Implementar `JR e8`
* [x] Implementar `CALL a16`
* [x] Implementar `RET`
* [x] Implementar `RST vec`

#### implementado na criação das funções

* [ ] Implementar condicionais `NZ`
* [ ] Implementar condicionais `Z`
* [ ] Implementar condicionais `NC`
* [ ] Implementar condicionais `C`

Regras principais:

```text
CALL:
  empilha PC de retorno
  PC = endereço chamado

RET:
  PC = valor desempilhado
```

---

## 3. Opcodes principais

* [x] `LD r,r`
* [x] `LD r,n8`
* [x] `LD r,[HL]`
* [x] `LD [HL],r`
* [x] `LD [HL],n8`
* [x] `INC r8`
* [x] `DEC r8`
* [x] `INC r16`
* [x] `DEC r16`
* [x] `ADD`
* [x] `ADC`
* [x] `SUB`
* [x] `SBC`
* [x] `AND`
* [x] `OR`
* [x] `XOR`
* [x] `CP`
* [x] `LDH [a8],A`
* [x] `LDH A,[a8]`

---

## 4. Bus e memória mínima

* [x] Implementar leitura de ROM `0x0000–0x7FFF`
* [x] Implementar leitura/escrita em VRAM `0x8000–0x9FFF`
* [x] Implementar leitura/escrita em cart RAM `0xA000–0xBFFF`
* [x] Implementar leitura/escrita em WRAM `0xC000–0xDFFF`
* [x] Implementar leitura/escrita em OAM `0xFE00–0xFE9F`
* [x] Implementar leitura/escrita em IO `0xFF00–0xFF7F`
* [x] Implementar leitura/escrita em HRAM `0xFF80–0xFFFE`
* [x] Implementar IE `0xFFFF`

Por enquanto, foco em ROM ONLY:

```text
0x0000–0x7FFF -> game_rom[address]
```

MBC fica para depois.

---

## 5. Timer e interrupções básicas

* [ ] Implementar `DIV`
* [ ] Implementar `TIMA`
* [ ] Implementar `TMA`
* [ ] Implementar `TAC`
* [ ] Implementar `IF` em `0xFF0F`
* [ ] Implementar `IE` em `0xFFFF`
* [ ] Implementar `IME`
* [ ] Implementar interrupção de VBlank
* [ ] Implementar interrupção de Timer

---

## 6. CB Prefix

* [ ] Detectar opcode `0xCB`
* [ ] Ler o próximo byte como opcode CB
* [ ] Implementar `BIT`
* [ ] Implementar `SET`
* [ ] Implementar `RES`
* [ ] Implementar rotates/shifts básicos
* [ ] Implementar operações em registradores
* [ ] Implementar operações em `[HL]`

---

## 7. PPU mínima

* [ ] Criar framebuffer `160x144`
* [ ] Implementar registrador `LCDC`
* [ ] Implementar registrador `STAT`
* [ ] Implementar `SCX`
* [ ] Implementar `SCY`
* [ ] Implementar `LY`
* [ ] Implementar `LYC`
* [ ] Renderizar background simples
* [ ] Gerar interrupção de VBlank

---

## 8. Joypad

* [ ] Implementar registrador `P1/JOYP` em `0xFF00`
* [ ] Mapear botões direcionais
* [ ] Mapear botões A/B/Start/Select
* [ ] Fazer leitura correta pelo jogo
* [ ] Gerar interrupção de joypad se necessário

---

## Ordem sugerida

```text
1. Stack
2. Controle de fluxo
3. Opcodes principais
4. Bus/write de memória
5. Timer/interrupções
6. CB prefix
7. PPU mínima
8. Joypad
9. MBC/cartuchos avançados
```

---

## Meta imediata

Antes de pensar em PPU, Color ou compatibilidade alta, focar em:

* [ ] `push16`
* [ ] `pop16`
* [ ] `PUSH`
* [ ] `POP`
* [ ] `CALL`
* [ ] `RET`

Quando isso funcionar, o emulador deixa de rodar apenas testes lineares e começa a conseguir executar código real com subrotinas.
