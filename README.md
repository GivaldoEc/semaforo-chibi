# semaforo-chibi
Trabalho de conclusão da disciplina "Desenvolvimento de sistemas embarcados em tempo real"


## Tarefas:

- [x] Virtual timers
- [ ] Mapear hardware
- [ ] Funções de hardware
- [ ] Máquina de estados
- [ ] Processar eventos

## Componentes:

- 19 Jumpers de tamanhos variados
- 6 resistores de 220 ohms
- 4 Botões
- 3 Leds RGB
- 1 Arduino Nano
- 1 Protoboard

## Pinos:

### Botões
- PC0 (PCINT8): Botão Pedestre (Verde)
- PC3 (PCINT11): Botão Via Secundária (Amarelo)
- PC1 (PCINT9): Botão Ambulância Secundária (Branco)
- PC2 (PCINT10): Botão Ambulância Primária (Preto)

### Semáforos
#### Pedestre
- PB3: Verde
- PB2: Vermelho
#### Secundário
- PB1: Verde
- PB0: Vermelho
- PD5: Amarelo
#### Primário
- PD7: Verde
- PD6: Vermelho
- PD4: Amarelo