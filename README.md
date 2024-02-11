# Servidor-para-Transmiss-o-de-Comandos-e-Telemetria-em-Corridas

O objetivo deste teste é aprimorar um servidor responsável pela transmissão de comandos à distância e pela receção de telemetria de carros de corrida durante uma competição desportiva.

### Funcionalidades Implementadas:
- **Informação de início da corrida:**
    - O servidor aguarda a mensagem "Partida!!" na FIFO "MASTER.in" para iniciar a corrida.
    - Após receber a mensagem, imprime no ecrã a informação de que a corrida foi iniciada.
    - Threads suspensas retomam a execução após a receção da mensagem.

- **Informação de disponibilidade de acesso à box:**
    - Implementado controle para que apenas um carro por equipa possa aceder à box simultaneamente.
    - Suspensão do envio da mensagem "BOX" se as condições especificadas não forem cumpridas.

- **Controlo de combustível:**
    - Restabelecimento do combustível do carro para 100 litros ao ser enviado para a box.
    - Implementação do abastecimento de combustível com precisão até aos centilitros.
    - Sincronização adequada para evitar condições de corrida na chamada concorrente da função de abastecimento.

- **Terminação do programa:**
    - O servidor apaga todas as FIFOs criadas após a execução.

### Testes Automatizados:
- Foram implementados testes automatizados para verificar o comportamento do servidor em diferentes cenários, conforme descrito nos testes 1 a 7

### Compilação e Execução:
- Para compilar o servidor, execute o comando `gcc main.c -o main` e `gcc client.c -o client `
- Para executar o servidor para um determinado teste, utilize o comando `./main <numero_do_teste>`.

