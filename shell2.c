#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define MAX_LINE 80     // Tamanho máximo de um comando
#define HISTORY_SIZE 10 // Histórico de comandos

// Estrutura para armazenar o histórico
char *history[HISTORY_SIZE];
int history_count = 0;

// Diretório inicial
char initial_dir[1024];

// Índice atual para navegação no histórico
int history_index = -1;

// Função para adicionar um comando ao histórico
void add_to_history(char *command) {
  if (history_count < HISTORY_SIZE) { //verificar se o historico esta cheio
    history[history_count++] = strdup(command); //apenas adiciona o comando ao historico em um espaco disponivel 
  } else {
    free(history[0]);
    for (int i = 1; i < HISTORY_SIZE; i++) {
      history[i - 1] = history[i]; //adiciona o comando ao proximo espaco disponivel
    }
    history[HISTORY_SIZE - 1] = strdup(command);
  }
  history_index = history_count; // Redefine o índice ao final do histórico
}

// Função para exibir o histórico
void show_history() {
  for (int i = 0; i < history_count; i++) {
    printf("%d: %s\n", i + 1, history[i]);
  }
}

// Configura o terminal para entrada não bloqueante (modo não canônico)
void enable_raw_mode() {
  struct termios term; //contem estruturas que controlam o comportamento do terminal
  tcgetattr(STDIN_FILENO, &term); //STDIN_FILENO descritor de arquivo para a entrada do teclado
  term.c_lflag &= ~(ICANON | ECHO); // Desativa modo canônico e eco
  term.c_cc[VMIN] = 1;             // Lê um caractere por vez
  term.c_cc[VTIME] = 0;            // Sem timeout
  tcsetattr(STDIN_FILENO, TCSANOW, &term); //entra em modo canonico novamente
}

// Restaura o terminal ao modo padrão
void disable_raw_mode() {
  struct termios term;
  tcgetattr(STDIN_FILENO, &term);
  term.c_lflag |= (ICANON | ECHO); // Reativa modo canônico e eco
  tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

// Função para executar comandos com suporte a pipes
void execute_pipeline(char *commands[], int count) {
  int pipefd[2];
  int fd_in = 0;

  for (int i = 0; i < count; i++) {
    pipe(pipefd);
    pid_t pid = fork();

    if (pid == 0) {
      // Processo filho
      dup2(fd_in, 0); // Redireciona entrada padrão
      if (i < count - 1) {
        dup2(pipefd[1], 1); // Redireciona saída padrão
      }
      close(pipefd[0]);
      execlp(commands[i], commands[i], NULL);
      perror("Erro ao executar comando");
      exit(1);
    } else if (pid < 0) {
      perror("Erro ao criar processo");
      exit(1);
    }

    // Processo pai
    close(pipefd[1]);
    fd_in = pipefd[0]; // A saída do pipe se torna a entrada para o próximo comando
    waitpid(pid, NULL, 0);
  }
}


// Funcao principal
int main() {
  char input[MAX_LINE];                // Buffer para armazenar a entrada do usuario
  char current_command[MAX_LINE];      // Comando atual processado
  char *args[MAX_LINE / 2 + 1];        // Vetor para armazenar argumentos do comando

  // Obtém o diretório inicial do shell e verifica erros
  if (getcwd(initial_dir, sizeof(initial_dir)) == NULL) {
    perror("Erro ao obter o diretório inicial");
    return 1;
  }

  enable_raw_mode();                   // Configura o terminal para modo não canônico

  while (1) {                          // Loop principal do shell
    char cwd[1024];                    // Buffer para armazenar o diretório atual
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
      perror("Erro ao obter o diretório atual");
    }

    // Exibe o prompt com o diretório atual
    printf("\nshell [%s]> ", cwd);
    fflush(stdout);                    // Garante que o prompt seja exibido imediatamente

    memset(input, 0, MAX_LINE);        // Limpa o buffer de entrada
    memset(current_command, 0, MAX_LINE); // Limpa o comando atual
    int pos = 0;                       // Posição atual para leitura de caracteres

    // Loop para processar a entrada do usuário
    while (1) {
      char c;
      read(STDIN_FILENO, &c, 1);       // Lê um caractere do terminal

      if (c == '\n') {                 // Se o caractere e Enter
        if (pos > 0) {                 // Processa apenas se houver entrada
          input[pos] = '\0';           // Adiciona o caractere nulo ao final da string
          break;
        }
      } else if (c == 127) {           // Se o caractere e Backspace
        if (pos > 0) {                 
          input[--pos] = '\0';      
          printf("\b \b");             
          fflush(stdout);
        }
      } else {                         // Para outros caracteres
        if (pos < MAX_LINE - 1) {      // Garante que não exceda o tamanho maximo
          input[pos++] = c;            
          write(STDOUT_FILENO, &c, 1); 
        }
      }
    }

    if (strlen(input) > 0) {           // Se o comando nao estiver vazio
      add_to_history(input);           // Adiciona ao historico
    }

    // Divide o comando em comandos separados por pipes
    char *commands[MAX_LINE];          // Vetor para armazenar os comandos
    int command_count = 0;

    char *token = strtok(input, "|");  // Divide a entrada em tokens usando | como delimitador
    while (token != NULL) {
      commands[command_count++] = token; // Armazena cada comando separado
      token = strtok(NULL, "|");         // Avanca para o proximo token
    }

    if (command_count > 1) {            // Se há mais de um comando, utiliza pipeline
      execute_pipeline(commands, command_count); // Executa os comandos em pipeline
      continue;
    }

    // Divide o comando em argumentos separados por espaços
    int i = 0;
    token = strtok(input, " ");
    while (token != NULL) {
      args[i++] = token;                // Adiciona cada argumento ao vetor
      token = strtok(NULL, " ");
    }
    args[i] = NULL;                     // Finaliza o vetor com NULL

    if (args[0] != NULL && strcmp(args[0], "exit") == 0) { // Comando interno exit
      printf("Saindo do shell...\n");   // Mensagem ao usuário
      break;                            // Sai do loop principal
    }

    if (args[0] != NULL) {              // Se ha um comando, cria um processo filho
      pid_t pid = fork();               // Cria um novo processo
      if (pid < 0) {                    // Verifica falha no fork
        perror("Erro ao criar processo");
      } else if (pid == 0) {            // Código do processo filho
        if (execvp(args[0], args) == -1) { // Executa o comando
          perror("Erro ao executar comando"); // Exibe erro em caso de falha
        }
        exit(0);                        // Encerra o processo filho
      } else {
        waitpid(pid, NULL, 0);          // Processo pai aguarda o filho terminar
      }
    }
  }

  disable_raw_mode();                   // Restaura o terminal ao modo padrão
  return 0;                             // Finaliza o programa
}

