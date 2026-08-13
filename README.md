# C-GBoy

## Demos

### Alleyway: 
![alleyway](images/alleyway.gif)

### Tetris: 
![tetris](images/tetris.gif)

SIM. Vou mastigar isso num nível **“quero terminar de ler e sentir que MBC3/MBC5 são só variações da mesma ideia”**.

A frase que eu quero que fique tatuada na tua cabeça é:

> **O MBC não “move” a ROM. Ele decide qual pedaço físico da ROM aparece em uma janela do espaço de endereçamento da CPU.**

Entendendo isso, 80% da confusão some.

## 1. Primeiro: por que essa porra existe?

A CPU do Game Boy tem endereços de 16 bits. Portanto ela consegue gerar endereços:

```text
0000
0001
0002
...
FFFF
```

Isso dá um espaço de endereçamento de 64 KiB. A região destinada ao cartucho é principalmente:

```text
0000–7FFF → ROM do cartucho
A000–BFFF → RAM do cartucho
```

Só que `0000–7FFF` tem apenas:

```text
0x8000 bytes = 32 KiB
```

Então uma ROM sem MBC, como uma ROM de 32 KiB, cabe bonitinha:

```text
CPU                    arquivo da ROM

0000 ────────────────→ ROM[0000]
...
3FFF ────────────────→ ROM[3FFF]

4000 ────────────────→ ROM[4000]
...
7FFF ────────────────→ ROM[7FFF]
```

Acabou.

Mas aí vem um jogo de, sei lá:

```text
512 KiB
```

A CPU só tem **32 KiB de janela para ROM**, mas o cartucho contém **512 KiB físicos**.

Não tem como todos os bytes aparecerem simultaneamente.

Então entra o MBC.

Ele funciona essencialmente como um **seletor/multiplexador de memória** entre a CPU e o chip de ROM. O Game Boy continua achando que está acessando `4000–7FFF`; o MBC decide **qual banco físico** está por trás daquela janela naquele momento. O Pan Docs descreve justamente o MBC1 como esse primeiro controlador de banking, com bancos de ROM de 16 KiB e bancos externos de RAM na janela `A000–BFFF`. ([GBDev][1])

---

# 2. Pensa em “janelas”, não em “mover banco”

Uma ROM de 128 KiB tem:

```text
128 KiB / 16 KiB = 8 bancos
```

Fisicamente, teu `memory.game_rom` poderia ser imaginado assim:

```text
Banco 0:
ROM[00000 – 03FFF]

Banco 1:
ROM[04000 – 07FFF]

Banco 2:
ROM[08000 – 0BFFF]

Banco 3:
ROM[0C000 – 0FFFF]

Banco 4:
ROM[10000 – 13FFF]

...
```

Só que a CPU não tem endereço `0x0C000`. O endereço da CPU só vai até `FFFF`.

Então o MBC oferece duas janelas:

```text
CPU:

0000–3FFF  → janela de 16 KiB
4000–7FFF  → janela de 16 KiB
```

A segunda é a mais fácil de entender:

```text
selecionou banco 1:

4000–7FFF → ROM banco 1
```

depois:

```text
selecionou banco 2:

4000–7FFF → ROM banco 2
```

depois:

```text
selecionou banco 7:

4000–7FFF → ROM banco 7
```

O endereço **da CPU não muda**.

O que muda é:

```text
qual pedaço de memory.game_rom
aquele endereço representa
```

Isso é bank switching.

---

# 3. Exemplo que vale ouro

Suponha:

```text
Banco selecionado = 3

CPU lê endereço = 0x4567
```

A janela banked começa em:

```text
0x4000
```

Então primeiro queremos descobrir **onde dentro daquele banco** a CPU está lendo:

```text
0x4567 - 0x4000 = 0x0567
```

Portanto ela quer:

```text
offset 0x0567 dentro do banco 3
```

Cada banco tem:

```text
0x4000 bytes
```

Então o banco 3 começa fisicamente em:

```text
3 × 0x4000
= 0xC000
```

Agora soma o offset:

```text
0xC000 + 0x0567
= 0xC567
```

Portanto:

```text
CPU lê 0x4567

MBC diz:
"banco atual é 3"

resultado físico:
game_rom[0xC567]
```

É **isso** que teu MBC faz.

Não copia nada.

Não `memcpy` banco nenhum.

Não altera a ROM.

Só transforma:

```text
endereço CPU
+
estado atual do mapper
↓
endereço físico no arquivo da ROM
```

Essa ideia vai permanecer em **MBC1, MBC3, MBC5 e praticamente qualquer mapper**.

---

# 4. Agora vem a frase mais esquisita do documento

O teu texto fala:

> `0000–7FFF` é usado tanto para leitura da ROM quanto para escrita nos registradores do MBC.

E provavelmente isso parece completamente amaldiçoado no começo.

Tipo:

> “PORRA, 2000–3FFF não era ROM? Como agora virou registrador?”

Porque depende se a CPU está **lendo ou escrevendo**.

Olha isso:

```text
CPU lê 0x2345
↓
cartucho devolve byte da ROM
```

Mas:

```text
CPU escreve 0x2345 = 0x07
↓
NÃO escreve na ROM
↓
MBC vê essa escrita
↓
altera seu registrador de banco
```

A ROM continua read-only.

Então:

```text
READ  0x2345 → ROM
WRITE 0x2345 → comando para MBC
```

Essa distinção é fundamental.

Fisicamente, o cartucho consegue ver não apenas as linhas de endereço, mas também se aquilo é uma operação de leitura ou escrita.

---

# 5. “Então esses registradores ficam nos endereços 2000–3FFF?”

Mais ou menos.

É importante não imaginar que existem **8192 registradores diferentes** ali.

Quando a documentação fala:

```text
2000–3FFF
ROM Bank Number
```

significa:

> **Qualquer escrita em qualquer endereço dessa faixa controla o mesmo registrador interno do MBC.**

Por exemplo:

```text
write 0x2000, 0x03
write 0x2345, 0x03
write 0x3000, 0x03
write 0x3FFF, 0x03
```

para esse propósito fazem a mesma coisa:

```text
ROM bank register ← 3
```

O endereço serve para o chip descobrir:

```text
"qual dos meus registradores o jogo está tentando alterar?"
```

O valor escrito diz:

```text
"qual valor colocar nele?"
```

---

# 6. O MBC1 tem basicamente quatro estados internos

Mentalmente, imagina quatro variáveis escondidas dentro do cartucho:

| Escrita da CPU | Estado alterado                  |
| -------------- | -------------------------------- |
| `0000–1FFF`    | RAM enable                       |
| `2000–3FFF`    | 5 bits baixos do banco de ROM    |
| `4000–5FFF`    | registrador secundário de 2 bits |
| `6000–7FFF`    | modo de banking                  |

Isso é essencialmente o coração inteiro do MBC1.

Não precisa pensar ainda em `memory.game_rom`.

Pensa:

```text
MBC1 {
    ram_enabled

    rom_bank_low5

    bank_high2

    mode
}
```

Conceitualmente é quase só isso.

---

# 7. `0000–1FFF`: RAM Enable

Essa é tranquila.

A RAM do cartucho não fica sempre acessível.

O jogo escreve um valor nessa faixa.

Se os quatro bits inferiores forem:

```text
1010 = 0xA
```

RAM habilitada.

Então:

```text
write 0000–1FFF com valor xA
↓
RAM ENABLED
```

Qualquer outro nibble inferior:

```text
RAM DISABLED
```

O Pan Docs especifica exatamente essa regra. ([GBDev][1])

Então:

```text
write(0x0000, 0x0A)
```

habilita.

Isso também:

```text
write(0x1234, 0xFA)
```

habilita, porque:

```text
0xFA & 0x0F
= 0x0A
```

Enquanto:

```text
write(0x0000, 0x00)
```

desabilita.

Quando desabilitada:

```text
A000–BFFF
```

não deve se comportar como RAM normal.

---

# 8. `2000–3FFF`: os cinco bits baixos do banco ROM

Agora começa a parte divertida.

Esse registrador tem 5 bits:

```text
xxxxx
```

5 bits conseguem representar:

```text
00000 = 0
até
11111 = 31
```

Então ele consegue escolher até 32 possibilidades:

```text
0–31
```

Uma escrita tipo:

```text
write 0x2000, 0b11100101
```

não usa os 8 bits.

Ele só liga pros 5 inferiores:

```text
11100101
   |||||
   00101
```

portanto:

```text
rom_bank_low5 = 5
```

Isso explica a frase no documento dizendo que escrever `E1` seleciona `01`: os bits superiores são descartados.

---

# 9. Mas existe a safadeza do banco zero

O MBC1 tem uma regra:

```text
rom_bank_low5 == 0
```

é **tratado como 1 para a janela `4000–7FFF`**. ([GBDev][1])

Então:

```text
jogo escreve banco 0

MBC:
"kkk não"

janela 4000–7FFF recebe banco 1
```

Por isso inicialmente:

```text
0000–3FFF → banco 0
4000–7FFF → banco 1
```

mesmo que o registrador interno tenha começado em `00`.

E aqui tem uma sutileza importante pra quando tu implementar:

**eu guardaria mentalmente o valor cru `00`.**

Não pensaria:

```text
jogo escreveu 00
↓
mudo a variável para 01
```

Eu pensaria:

```text
registrador = 00

na hora de calcular o banco efetivo:
se os cinco bits == 0
    tratar como 1
```

Porque existem uns edge cases do MBC1 em ROMs menores onde guardar o valor original importa. O teu próprio documento entra nessa maluquice depois.

---

# 10. Tá, mas 5 bits só dão 32 bancos

Exatamente.

Cada banco:

```text
16 KiB
```

32 bancos:

```text
32 × 16 KiB
= 512 KiB
```

Olha como bate perfeitamente:

```text
5 bits de banco
↓
32 bancos
↓
512 KiB ROM
```

E é justamente por isso que o texto diz que a configuração básica do MBC1 consegue lidar naturalmente com até **512 KiB de ROM**. ([GBDev][1])

Mas MBC1 também pode chegar em ROMs maiores.

Como?

Com os outros dois bits.

---

# 11. `4000–5FFF`: os famosos 2 bits que causam toda a confusão

Esse registrador possui:

```text
2 bits
```

portanto:

```text
00
01
10
11
```

ou:

```text
0
1
2
3
```

O documento chama isso de:

```text
RAM Bank Number
OU
Upper Bits of ROM Bank Number
```

E é exatamente aqui que quase todo mundo lê a primeira vez e pensa:

> “COMO ASSIM OU???”

A resposta é física.

Esses dois bits são **duas linhas adicionais de endereço**.

Dependendo de como o cartucho foi construído, elas podem estar conectadas à ROM grande ou à RAM grande. ([GBDev][1])

Essa parte vale entender profundamente.

---

# 12. Vamos contar fios físicos, porque aí vira óbvio

Uma ROM de 512 KiB tem:

```text
512 KiB = 524288 bytes
         = 2^19 bytes
```

Pra escolher qualquer byte dela, precisamos de 19 bits de endereço:

```text
A0 ... A18
```

Dentro de cada banco de 16 KiB:

```text
16 KiB = 2^14
```

Então os 14 bits inferiores:

```text
A0–A13
```

vêm diretamente do endereço da CPU dentro da janela.

Sobram:

```text
A14–A18
```

Cinco bits.

Adivinha quem fornece?

```text
rom_bank_low5
```

KKKKKK.

Então:

```text
ROM física:

A18 A17 A16 A15 A14 | A13........A0
 \______5 bits_____/   \__CPU_____/
       MBC1
```

É por isso que o banco tem 5 bits.

Não é um número aleatório.

---

# 13. Agora uma ROM de 2 MiB

```text
2 MiB = 2^21 bytes
```

Agora precisamos:

```text
A0–A20
```

A CPU continua fornecendo:

```text
A0–A13
```

o registrador principal fornece:

```text
A14–A18
```

mas ainda faltam:

```text
A19 e A20
```

DOIS BITS.

Exatamente o registrador:

```text
4000–5FFF
```

Então numa ROM grande:

```text
     HIGH2      LOW5         offset
      ↓           ↓             ↓
A20 A19 | A18 A17 A16 A15 A14 | A13 ... A0
```

Agora olha a fórmula do teu documento:

```text
Selected ROM Bank =
    (Secondary Bank << 5)
    +
    ROM Bank
```



Faz todo sentido porque:

```text
HIGH2   LOW5

10      00101
```

vira:

```text
1000101
```

Ou seja:

```text
high2 = 2
low5  = 5
```

então:

```text
(2 << 5) + 5

2 × 32 + 5

64 + 5

69

banco 69
```

Em hexadecimal:

```text
0x45
```

---

# 14. Então `4000–5FFF` escolhe ROM ou RAM?

Agora chegamos na parte que o teu documento apresenta meio densamente.

Em uma configuração de cartucho com:

```text
ROM até 512 KiB
RAM até 32 KiB
```

a ROM **não precisa** desses dois bits extras.

A ROM já está satisfeita com os 5 bits.

Então as duas linhas extras podem ser conectadas à **RAM**.

Por outro lado, se o cartucho tiver uma ROM de:

```text
1 MiB
ou
2 MiB
```

ela precisa desses bits extras.

Então eles são usados pela ROM.

O preço é que não sobram essas linhas para bankear uma RAM de 32 KiB, então esse tipo de cartucho fica limitado a uma RAM fixa de 8 KiB. Essa limitação de wiring está explicitamente descrita no texto que você mandou.

Essa é a ideia da frase:

```text
MBC1 pode ter

512 KiB ROM + 32 KiB RAM

OU

até 2 MiB ROM + RAM fixa de 8 KiB
```

Não é uma limitação de software arbitrária.

É literalmente:

> “tenho essas duas saídas do chip; em qual memória vou ligar elas?”

---

# 15. Por que 32 KiB de RAM precisa de exatamente 2 bits extras?

Mesma matemática.

Cada banco de RAM tem:

```text
8 KiB = 0x2000 = 2^13
```

A janela:

```text
A000–BFFF
```

possui exatamente 8 KiB.

Então o endereço da CPU dá os 13 bits:

```text
A0–A12
```

Agora 32 KiB de RAM:

```text
32 KiB = 4 × 8 KiB
```

Tem quatro bancos:

```text
RAM bank 0
RAM bank 1
RAM bank 2
RAM bank 3
```

Pra escolher quatro coisas:

```text
2 bits
```

Logo:

```text
high2 = 00 → RAM banco 0
high2 = 01 → RAM banco 1
high2 = 10 → RAM banco 2
high2 = 11 → RAM banco 3
```

Novamente: nada é mágico.

---

# 16. Agora o tal do MODE

O registrador:

```text
6000–7FFF
```

usa apenas 1 bit.

Então:

```text
0 = mode 0
1 = mode 1
```

O documento chama:

```text
mode 0 = simple
mode 1 = advanced
```



E essa parte parece complicada porque ele está descrevendo **quando aqueles dois bits extras são permitidos afetar determinadas janelas**.

Vamos esquecer eletrônica por um segundo.

---

# 17. Mode 0: pensa “prioridade pra ROM”

No mode 0:

```text
0000–3FFF
```

fica travado no banco ROM:

```text
00
```

E:

```text
A000–BFFF
```

fica travado no banco RAM:

```text
00
```

Enquanto:

```text
4000–7FFF
```

pode usar a combinação completa dos registradores de ROM.

Então mentalmente:

```text
MODE 0

0000–3FFF
→ ROM banco 00

4000–7FFF
→ ROM [high2 + low5]

A000–BFFF
→ RAM banco 00
```

Essa é uma excelente representação mental.

---

# 18. Mode 1: libera os dois bits extras nas outras janelas

No mode 1:

```text
0000–3FFF
```

também passa a poder ser influenciado pelo `high2`.

E:

```text
A000–BFFF
```

também passa a poder ser influenciado pelo `high2`.

Enquanto a janela:

```text
4000–7FFF
```

continua usando:

```text
high2 + low5
```

Então o modelo lógico é:

```text
MODE 1

0000–3FFF
→ ROM [high2 + 00000]

4000–7FFF
→ ROM [high2 + low5]

A000–BFFF
→ RAM banco high2
```

Isso é exatamente o que os diagramas que você mandou estão tentando mostrar.

---

# 19. “Mas então mode 1 muda ROM E RAM ao mesmo tempo?”

Aqui está a pegadinha:

**logicamente sim, mas fisicamente só vai ter efeito onde esses fios existem.**

Lembra dos dois tipos de cartucho?

### Cartucho com ROM de até 512 KiB + RAM de 32 KiB

A ROM não possui `A19/A20`.

Então você pode fazer:

```text
high2 = 3
```

mas esses bits simplesmente **não chegam a lugar nenhum na ROM**.

Logo:

```text
0000–3FFF
```

continua efetivamente vendo banco 0.

Por outro lado, esses dois bits estão ligados na RAM.

Então no mode 1:

```text
high2 = 3
↓
A000–BFFF → RAM banco 3
```

---

### Cartucho com ROM de 2 MiB + RAM de 8 KiB

Agora `high2` está fisicamente ligado à ROM.

Então:

```text
high2 = 2
mode = 1
```

pode fazer:

```text
0000–3FFF → ROM banco 0x40
```

porque:

```text
10 00000
= 0x40
```

Mas a RAM só tem 8 KiB.

Só existe:

```text
RAM banco 0
```

Então tentar selecionar RAM banco 2 não tem efeito observável.

Essa é provavelmente **a parte mais importante de todo o documento** pra realmente entender MBC1.

---

# 20. Exemplo completo: cartucho de 512 KiB + 32 KiB RAM

512 KiB significa:

```text
32 bancos ROM
```

Então só precisamos dos 5 bits baixos:

```text
00000–11111
```

Suponha:

```text
rom_low5 = 7
high2 = 2
mode = 0
```

Como os bits `high2` não estão ligados à ROM:

```text
0000–3FFF → ROM banco 0
4000–7FFF → ROM banco 7
A000–BFFF → RAM banco 0
```

Agora troca:

```text
mode = 1
```

Resultado:

```text
0000–3FFF → ROM banco 0
4000–7FFF → ROM banco 7
A000–BFFF → RAM banco 2
```

Olha como ficou simples.

Nesse cartucho, `high2` praticamente significa:

```text
"qual banco de RAM?"
```

---

# 21. Exemplo completo: cartucho de 2 MiB + 8 KiB RAM

Agora temos:

```text
2 MiB / 16 KiB
= 128 bancos
```

Precisamos de:

```text
7 bits
```

Então suponha:

```text
high2 = 2  = 10b
low5  = 5  = 00101b
```

Junta:

```text
10 00101
```

Resultado:

```text
0x45
= banco 69
```

No mode 0:

```text
0000–3FFF → banco 00
4000–7FFF → banco 45
A000–BFFF → RAM banco 0
```

No mode 1:

```text
0000–3FFF → banco 40
4000–7FFF → banco 45
A000–BFFF → RAM banco 0
```

Porque:

```text
high2 + 00000

10 00000 = 0x40
```

Agora o nome do documento:

```text
ROM Bank X0
```

fica muito menos estranho.

`X0` quer dizer basicamente bancos cujo pedaço baixo é zero:

```text
00
20
40
60
```

dependendo dos dois bits superiores. ([GBDev][1])

---

# 22. E agora entendemos a bizarrice `20 → 21`

Lembra:

```text
low5 == 0
```

na janela `4000–7FFF` é tratado como:

```text
low5 = 1
```

Então você tenta construir:

```text
high2 = 01
low5  = 00000
```

Você queria:

```text
01 00000
= 0x20
```

Só que o MBC vê:

```text
low5 == 00000
```

e aplica a regra:

```text
00000 → 00001
```

resultado:

```text
01 00001
= 0x21
```

Por isso:

```text
20 vira 21
40 vira 41
60 vira 61
```

na janela `4000–7FFF`. ([GBDev][1])

E onde aparecem os bancos:

```text
20
40
60
```

de verdade?

No mode 1, através da janela:

```text
0000–3FFF
```

porque ali o banco é:

```text
high2 + 00000
```

sem usar o registrador low5 da mesma maneira.

Esse detalhe que parecia uma regra completamente aleatória agora tem origem clara.

---

# 23. Como ler aqueles diagramas horrorosos

O teu documento mostra coisas como:

```text
20 19 | 18 17 16 15 14 | 13 ... 0
```

Aquilo são **bits do endereço físico da ROM**, não bits do endereço CPU simplesmente.

Para `4000–7FFF`, pense:

```text
endereço físico na ROM:

A20 A19 | A18 A17 A16 A15 A14 | A13 ... A0
   ↑               ↑                  ↑
 high2            low5           endereço CPU
```

Pronto.

Esse diagrama inteiro virou:

```text
[ banco ][ posição dentro do banco ]
```

porque qualquer endereço físico de uma memória bancária é exatamente isso:

```text
bank_number × bank_size + offset
```

E para RAM:

```text
A14 A13 | A12 ... A0
   ↑           ↑
 RAM bank     CPU
```

Por isso:

```text
physical_ram =
    ram_bank * 0x2000
    +
    (cpu_address - 0xA000)
```

Não precisa decorar o HTML amaldiçoado daquele documento KKKKK.

---

# 24. Como isso deve existir mentalmente no teu emulador

Separaria completamente duas coisas:

```text
DADOS DO CARTUCHO

game_rom[]
cart_ram[]
```

e:

```text
ESTADO DO MAPPER

ram_enabled
rom_bank_low5
bank_high2
banking_mode
```

O mapper não contém as ROMs.

Ele contém **os seletores**.

Então uma leitura no bus faz:

```text
CPU quer ler alguma coisa
↓
qual região?
```

Se:

```text
0000–3FFF
```

pergunta ao mapper:

```text
qual banco deve aparecer aqui?
```

Se:

```text
4000–7FFF
```

pergunta:

```text
qual banco deve aparecer aqui?
```

Se:

```text
A000–BFFF
```

pergunta:

```text
RAM está habilitada?
qual banco?
```

Todo o resto do Game Boy continua normal.

---

# 25. Uma transação real do jogo

Imagina o jogo fazendo:

```text
write 0x2000, 0x03
```

Isso **não** significa:

```text
ROM[0x2000] = 3
```

Significa:

```text
MBC1.rom_bank_low5 = 3
```

Depois, algumas instruções mais tarde:

```text
read 0x4567
```

O teu bus percebe:

```text
4567 está em 4000–7FFF
```

Pergunta:

```text
qual banco efetivo?
```

Resposta:

```text
3
```

Então:

```text
offset = 4567 - 4000
       = 0567

physical =
    3 * 4000
    + 0567

= C567
```

E retorna:

```text
game_rom[C567]
```

**Esse é o MBC1 acontecendo.**

---

# 26. RAM banking funciona do MESMO jeito

Suponha cartucho com 32 KiB de RAM.

Temos:

```text
4 bancos × 8 KiB
```

Jogo:

```text
habilita RAM
```

depois:

```text
mode = 1
high2 = 2
```

Agora:

```text
A000–BFFF
```

representa RAM banco 2.

Jogo escreve:

```text
A123 = 0x55
```

Offset dentro da janela:

```text
A123 - A000
= 0123
```

Banco 2 começa:

```text
2 × 0x2000
= 0x4000
```

Então fisicamente:

```text
cart_ram[0x4123] = 0x55
```

Agora muda:

```text
high2 = 1
```

e lê:

```text
A123
```

Não vê mais aquele `55`, porque agora está lendo:

```text
cart_ram[
    1 * 0x2000 + 0x123
]
```

É exatamente a mesma técnica da ROM.

Isso também explica por que RAM externa com bateria vira save: o jogo escreve seus dados nesses bancos; depois o emulador persiste esse bloco em disco. ([GBDev][1])

---

# 27. Uma coisa relevante pra tua estrutura de RAM

Pra quatro bancos de 8 KiB, são:

```text
4 × 0x2000
= 0x8000 bytes
= 32 KiB
```

Então uma implementação que tenha armazenamento físico de apenas:

```text
0x2000
```

consegue representar somente **um banco de 8 KiB**.

O mapper não substitui a necessidade de realmente ter espaço pra todos os bancos físicos.

A ROM é igual: felizmente teu arquivo inteiro da ROM já costuma estar carregado em memória, então banking nela é só escolher índices diferentes.

---

# 28. E o tal “masking” quando a ROM é menor?

Suponha que o registrador teoricamente consiga selecionar:

```text
31
```

mas o cartucho só tem:

```text
8 bancos
```

Fisicamente, as linhas extras de endereço simplesmente **não existem**.

Por exemplo, 8 bancos requerem:

```text
3 bits
```

Então:

```text
xxxxx
```

na prática só alguns bits chegam ao chip.

O documento descreve justamente que bits superiores não necessários para o tamanho da ROM são ignorados. ([GBDev][1])

Isso é outra razão pra pensar no MBC como **fios de endereço físicos**, e não como uma API bonitinha:

```text
"seleciona banco 8293829"
```

Não.

São bits que chegam ou não chegam a pinos de endereço.

---

# 29. O edge case estranho de ROM pequena e banco zero

Aqui entra aquele parágrafo estranho do documento.

Suponha ROM de 256 KiB:

```text
16 bancos
```

Só precisamos de:

```text
4 bits
```

Agora o jogo escreve:

```text
low5 = 10000b
```

Isso é:

```text
0x10
```

O MBC verifica a regra `00 → 01` olhando **os cinco bits completos**.

Ele vê:

```text
10000 != 00000
```

portanto NÃO transforma em 1.

Só que fisicamente a ROM usa apenas quatro bits:

```text
10000
 ↓↓↓↓
 0000
```

O bit mais alto é ignorado.

Resultado físico:

```text
banco 0
```

Então em ROM pequena existe essa maneira torta de fazer banco 0 aparecer na janela `4000–7FFF`. O documento chama atenção exatamente para esse detalhe.

Não é algo que você precisa ficar pensando durante cada linha de código agora, mas explica por que eu falei antes:

> **guarda o valor real do registrador e calcula o banco efetivo depois.**

---

# 30. Agora: por que depois disso MBC3 vai parecer férias

O MBC3 mantém exatamente o mesmo princípio:

```text
CPU tem janela
↓
mapper possui registradores
↓
escritas na região ROM configuram mapper
↓
leituras são traduzidas para o banco físico
```

Só que ele joga fora boa parte das maluquices do MBC1.

O MBC3 suporta até 2 MiB de ROM, ou 128 bancos de 16 KiB, e até quatro bancos de 8 KiB de RAM; algumas variantes também possuem RTC. ([GBDev][2])

A arquitetura mental vira aproximadamente:

```text
0000–3FFF → ROM banco 0, sempre

4000–7FFF → ROM banco escolhido

A000–BFFF → RAM escolhida
             OU RTC
```

E o grande presente:

**não tem esse mode 0/mode 1 compartilhando bits altos da forma bizarra do MBC1.**

---

# 31. Por que o banco do MBC3 tem 7 bits?

Porque:

```text
2 MiB ROM
÷
16 KiB por banco
=
128 bancos
```

128 possibilidades:

```text
2^7
```

Então:

```text
7 bits
```

Simples.

No MBC3, a escrita em `2000–3FFF` seleciona esse banco de ROM; banco `00` também é tratado como banco `01` para a janela selecionável. ([GBDev][2])

Então, depois de sobreviver ao MBC1, você vai olhar isso e pensar:

```text
"ah

rom_bank = value & 0x7F

entendi"
```

KKKKK.

---

# 32. E a RAM do MBC3?

Novamente:

```text
32 KiB
÷
8 KiB
=
4 bancos
```

Logo:

```text
00
01
02
03
```

A janela continua:

```text
A000–BFFF
```

Então:

```text
ram_bank * 0x2000
+
(address - 0xA000)
```

Mesma matemática que você acabou de aprender.

---

# 33. E aí o MBC3 coloca o RTC nessa mesma ideia de “janela”

Esse é o pulo mental bonito.

O registrador de seleção em `4000–5FFF` pode escolher:

```text
00–03
```

que representam bancos RAM,

ou selecionar registradores do RTC.

Os seletores RTC vão de `08` a `0C`; representam segundos, minutos, horas e os bits do contador de dias/controle. ([GBDev][2])

Então pense:

```text
selector = 02
↓
A000–BFFF aponta para RAM banco 2
```

ou:

```text
selector = 08
↓
A000–BFFF representa registrador de segundos do RTC
```

A MESMA JANELA.

Só mudou o que o mapper colocou atrás dela.

É literalmente o mesmo conceito de banking expandido para um periférico.

O RTC ainda tem uma operação de latch: escrever `00` e depois `01` no registrador de latch captura uma cópia estável do relógio para leitura. ([GBDev][2])

Mas conceitualmente já acabou:

> “mapper escolhe o que fica visível na janela”.

---

# 34. E MBC5 fica ainda mais idiota depois disso

O MBC5 suporta ROMs de até 8 MiB. ([GBDev][3])

Faz a matemática:

```text
8 MiB
÷
16 KiB
=
512 bancos
```

Quantos bits pra 512?

```text
2^9 = 512
```

Então:

```text
9 bits de banco ROM
```

Pronto.

Só existe um probleminha:

um write da CPU tem apenas:

```text
8 bits de valor
```

Como escreve banco de 9 bits?

Divide o registrador.

No MBC5:

```text
2000–2FFF
→ 8 bits inferiores do banco

3000–3FFF
→ 9º bit
```

([GBDev][3])

Então se:

```text
low8 = 0x45
high1 = 1
```

o banco é:

```text
1 01000101
```

ou:

```text
(1 << 8) | 0x45
```

Pronto.

Comparado com MBC1 isso parece brincadeira de criança.

---

# 35. MBC5 e RAM

MBC5 pode ter até 128 KiB de RAM. ([GBDev][3])

Faz novamente:

```text
128 KiB
÷
8 KiB
=
16 bancos
```

Quantos bits?

```text
2^4 = 16
```

Então:

```text
RAM bank = 4 bits
```

A mesma janela continua sendo:

```text
A000–BFFF
```

E continua valendo:

```text
physical =
    bank * 0x2000
    +
    window_offset
```

Você percebe o padrão?

---

# 36. MBC1 → MBC3 → MBC5 lado a lado

| Conceito                   | MBC1                                                         | MBC3        | MBC5                                                         |
| -------------------------- | ------------------------------------------------------------ | ----------- | ------------------------------------------------------------ |
| Janela ROM fixa            | `0000–3FFF` normalmente banco 0, com peculiaridade do mode 1 | banco 0     | banco 0                                                      |
| Janela ROM variável        | `4000–7FFF`                                                  | `4000–7FFF` | `4000–7FFF`                                                  |
| Tamanho banco ROM          | 16 KiB                                                       | 16 KiB      | 16 KiB                                                       |
| Banco ROM                  | 5 bits + 2 bits compartilhados                               | 7 bits      | 9 bits                                                       |
| `00→01` na janela variável | sim, com peculiaridades                                      | sim         | não funciona da mesma forma; MBC5 permite selecionar banco 0 |
| Janela RAM                 | `A000–BFFF`                                                  | `A000–BFFF` | `A000–BFFF`                                                  |
| Tamanho banco RAM          | 8 KiB                                                        | 8 KiB       | 8 KiB                                                        |
| Complicação especial       | mode 0/1 + wiring compartilhado                              | RTC         | banco ROM maior; rumble em algumas variantes                 |

O padrão comum entre eles é o importante: **o mapper mantém estado e transforma endereços da CPU em endereços físicos do cartucho.** As diferenças são quais registradores controlam essa transformação. MBC3 acrescenta RTC; MBC5 amplia bastante o número de bancos e simplifica o banking de ROM em dois registradores dedicados. ([GBDev][2])

---

# 37. Se eu resumisse MBC1 em uma única imagem mental

Imagina isso:

```text
                         CARTUCHO
                    ┌─────────────────┐
                    │                 │
CPU address ───────►│      MBC1       │────► endereço físico ROM
                    │                 │
CPU value ─────────►│ low5            │
(quando write)      │ high2           │
                    │ mode            │
                    │ ram_enable      │
                    └────────┬────────┘
                             │
                             ▼

                   ┌──────────────────┐
                   │ ROM inteira      │
                   │                  │
                   │ bank 00          │
                   │ bank 01          │
                   │ bank 02          │
                   │ ...              │
                   │ bank 7F          │
                   └──────────────────┘
```

E do lado RAM:

```text
A000–BFFF
    ↓
   MBC
    ↓
qual RAM bank?
    ↓
┌─────────────┐
│ RAM bank 0  │
│ RAM bank 1  │
│ RAM bank 2  │
│ RAM bank 3  │
└─────────────┘
```

O MBC é **só o porteiro**.

A memória física está ali o tempo todo.

---

# 38. A sequência que eu usaria na tua cabeça na hora de implementar

Quando houver **WRITE**, não pensa “memória” primeiro. Pergunta:

```text
endereço está entre 0000–7FFF?
```

Se sim e o cartucho é MBC:

```text
isso provavelmente é um COMANDO para o mapper
```

Então a faixa do endereço determina qual registrador interno muda.

Quando houver **READ**, pergunta:

```text
0000–3FFF?
→ mapper decide banco da janela baixa

4000–7FFF?
→ mapper decide banco da janela alta

A000–BFFF?
→ mapper decide RAM / banco / disponibilidade
```

E no final tudo vira uma conta tipo:

```text
physical_address =
    bank * bank_size
    +
    offset_inside_window
```

Essa fórmula é praticamente a alma do banking.

---

# 39. E o MBC1M do fim do documento?

**Não deixa essa porra atrapalhar teu aprendizado agora.**

O MBC1M é uma wiring alternativa usada em carts multicart. A diferença principal é onde aqueles bits superiores entram no número do banco; em vez da organização usual, ele desloca a divisão dos bits para permitir selecionar grupos de ROM correspondentes a jogos diferentes. O teu documento explica que isso leva a bancos `00/10/20/30` onde um MBC1 regular grande teria `00/20/40/60`.

Conceitualmente, porém, não ensina absolutamente nada novo:

```text
registradores
↓
bits de endereço
↓
banco físico
```

É só outra forma de ligar os fios.

Eu ignoraria MBC1M até teu MBC1 normal estar rodando jogos.

---

# 40. O teste pra saber se você realmente entendeu

Se eu te disser:

```text
MBC1 grande

low5 = 0b00101
high2 = 0b10
mode = 0

CPU lê 0x5234
```

tu consegue chegar:

```text
bank =
10 00101
= 0x45
= 69

offset =
0x5234 - 0x4000
= 0x1234

physical =
69 * 0x4000 + 0x1234
```

Se isso fizer sentido, **você já entendeu bank switching**.

Se eu mudar amanhã pra MBC5 e disser:

```text
low8 = 0x45
high1 = 1
```

tu vai simplesmente pensar:

```text
bank =
(1 << 8) | 0x45
```

e o resto:

```text
bank * 0x4000
+
(address - 0x4000)
```

é **idêntico**.

E se eu disser MBC3:

```text
rom_bank = 37
```

é ainda mais simples:

```text
37 * 0x4000
+
(address - 0x4000)
```

Então sim: **MBC1 é de longe o melhor pra aprender primeiro justamente porque ele contém quase todas as ideias importantes e ainda adiciona umas frescuras próprias**. Depois dele, MBC3 é “banking + relógio” e MBC5 é praticamente “banking com números maiores”.

A parte que eu realmente faria questão de tu sair daqui entendendo por osmose é:

```text
CPU ADDRESS ≠ PHYSICAL CARTRIDGE ADDRESS


CPU ADDRESS
      +
MAPPER STATE
      ↓
PHYSICAL CARTRIDGE ADDRESS
```

**É isso. Esse é o MBC.**

O resto do documento é basicamente uma especificação de **quais bits entram onde nessa equação**.

[1]: https://gbdev.io/pandocs/MBC1.html?utm_source=chatgpt.com "MBC1 - Pan Docs"
[2]: https://gbdev.io/pandocs/MBC3.html?utm_source=chatgpt.com "MBC3 - Pan Docs"
[3]: https://gbdev.io/pandocs/MBC5.html?utm_source=chatgpt.com "MBC5 - Pan Docs"
