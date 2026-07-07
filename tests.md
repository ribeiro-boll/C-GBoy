
# Programa de teste — CPU do Game Boy

Carregue os bytes abaixo a partir do endereço `0x0100`.

## Bytecode

```text
21 00 C0
3E 04
06 03
80
77
3C
23
77
C3 0C 01
````

## Assembly equivalente

```asm
LD HL, $C000
LD A, $04
LD B, $03
ADD A, B
LD [HL], A

INC A
INC HL
LD [HL], A

loop:
JP loop
```

## Mapeamento por endereço

| Endereço | Bytes      | Instrução      |
| -------- | ---------- | -------------- |
| `0x0100` | `21 00 C0` | `LD HL, $C000` |
| `0x0103` | `3E 04`    | `LD A, $04`    |
| `0x0105` | `06 03`    | `LD B, $03`    |
| `0x0107` | `80`       | `ADD A, B`     |
| `0x0108` | `77`       | `LD [HL], A`   |
| `0x0109` | `3C`       | `INC A`        |
| `0x010A` | `23`       | `INC HL`       |
| `0x010B` | `77`       | `LD [HL], A`   |
| `0x010C` | `C3 0C 01` | `JP $010C`     |

## Execução esperada

### Estado inicial

```text
PC = 0x0100
A  = 0x00
B  = 0x00
HL = 0x0000
```

### Depois de `LD HL, $C000`

```text
HL = 0xC000
PC = 0x0103
```

### Depois de `LD A, $04`

```text
A  = 0x04
PC = 0x0105
```

### Depois de `LD B, $03`

```text
B  = 0x03
PC = 0x0107
```

### Depois de `ADD A, B`

```text
A = 0x04 + 0x03
A = 0x07

PC = 0x0108
```

### Depois de `LD [HL], A`

Como `HL = 0xC000`:

```text
RAM[0xC000] = 0x07
PC = 0x0109
```

### Depois de `INC A`

```text
A  = 0x08
PC = 0x010A
```

### Depois de `INC HL`

```text
HL = 0xC001
PC = 0x010B
```

### Depois do segundo `LD [HL], A`

```text
RAM[0xC001] = 0x08
PC = 0x010C
```

### Loop final

```asm
JP $010C
```

A CPU continuará executando o salto:

```text
0x010C → 0x010C → 0x010C → ...
```

## Estado final esperado

```text
A           = 0x08
B           = 0x03
HL          = 0xC001
RAM[0xC000] = 0x07
RAM[0xC001] = 0x08
PC          = 0x010C
```

## Array em C

```c
#include <stdint.h>
#include <string.h>

uint8_t program[] = {
    0x21, 0x00, 0xC0, // LD HL, $C000
    0x3E, 0x04,       // LD A, $04
    0x06, 0x03,       // LD B, $03
    0x80,             // ADD A, B
    0x77,             // LD [HL], A
    0x3C,             // INC A
    0x23,             // INC HL
    0x77,             // LD [HL], A
    0xC3, 0x0C, 0x01  // JP $010C
};

uint8_t memory[0x10000];

void load_test_program(void)
{
    memcpy(&memory[0x0100], program, sizeof(program));
}
```

Depois de carregar:

```c
cpu.pc = 0x0100;
```

## Trace esperado

```text
PC=0100 OP=21 HL=0000 A=00 B=00
PC=0103 OP=3E HL=C000 A=00 B=00
PC=0105 OP=06 HL=C000 A=04 B=00
PC=0107 OP=80 HL=C000 A=04 B=03
PC=0108 OP=77 HL=C000 A=07 B=03
PC=0109 OP=3C HL=C000 A=07 B=03
PC=010A OP=23 HL=C000 A=08 B=03
PC=010B OP=77 HL=C001 A=08 B=03
PC=010C OP=C3 HL=C001 A=08 B=03
PC=010C OP=C3 HL=C001 A=08 B=03
```

## Opcodes necessários

| Opcode | Instrução    | Tamanho |
| ------ | ------------ | ------: |
| `0x21` | `LD HL, n16` | 3 bytes |
| `0x3E` | `LD A, n8`   | 2 bytes |
| `0x06` | `LD B, n8`   | 2 bytes |
| `0x80` | `ADD A, B`   |  1 byte |
| `0x77` | `LD [HL], A` |  1 byte |
| `0x3C` | `INC A`      |  1 byte |
| `0x23` | `INC HL`     |  1 byte |
| `0xC3` | `JP n16`     | 3 bytes |

```
