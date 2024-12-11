#Pipe Fork

Este trabalho consiste na implementação de um programa em linguagem C que, quando em execução, atua como um shell que aceita comandos do usuário e os executa em processos filhos. Um shell, após ser iniciado, aguarda comandos do usuário pela entrada padrão (teclado). Como resposta, o shell executa o programa ls e grep cada um no contexto de um processo filho, usando para isso chamadas de sistema fork() e da família exec(). Como resultado do comando, a saída de ls é passada para a entrada de grep, que por sua vez mostra seu resultado na saída padrão (stdout).
