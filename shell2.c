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
  if (history_count < HISTORY_SIZE) {
    history[history_count++] = strdup(command);
  } else {
    free(history[0]);
    for (int i = 1; i < HISTORY_SIZE; i++) {
      history[i - 1] = history[i];
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
  struct termios term;
  tcgetattr(STDIN_FILENO, &term);
  term.c_lflag &= ~(ICANON | ECHO); // Desativa modo canônico e eco
  term.c_cc[VMIN] = 1;             // Lê um caractere por vez
  term.c_cc[VTIME] = 0;            // Sem timeout
  tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

// Restaura o terminal ao modo padrão
void disable_raw_mode() {
  struct termios term;
  tcgetattr(STDIN_FILENO, &term);
  term.c_lflag |= (ICANON | ECHO); // Reativa modo canônico e eco
  tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

// Função principal
int main() {
  char input[MAX_LINE];
  char current_command[MAX_LINE]; // Para armazenar o comando atual ao navegar no histórico
  char *args[MAX_LINE / 2 + 1];   // Comando dividido em tokens
  int background;

  // Salvar o diretório inicial
  if (getcwd(initial_dir, sizeof(initial_dir)) == NULL) {
    perror("Erro ao obter o diretório inicial");
    return 1;
  }

  enable_raw_mode();

  while (1) {
    // Obter o diretório atual
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
      perror("Erro ao obter o diretório atual");
    }

    // Exibir o prompt com o diretório atual
    printf("\nshell [%s]> ", cwd);
    fflush(stdout);

    // Limpar buffers
    memset(input, 0, MAX_LINE);
    memset(current_command, 0, MAX_LINE);
    int pos = 0;

    while (1) {
      char c;
      read(STDIN_FILENO, &c, 1);

      if (c == '\n') { // Enter
        if (pos > 0) {
          input[pos] = '\0';
          break;
        }
      } else if (c == 127) { // Backspace
        if (pos > 0) {
          input[--pos] = '\0';
          printf("\b \b");
          fflush(stdout);
        }
      } else if (c == '\033') { // Tecla de escape para setas
        char seq[2];
        read(STDIN_FILENO, &seq[0], 1);
        read(STDIN_FILENO, &seq[1], 1);

        if (seq[0] == '[') {
          if (seq[1] == 'A') { // Seta para cima
            if (history_index > 0) {
              history_index--;
              strcpy(current_command, history[history_index]);
              printf("\r\033[Kshell [%s]> %s", cwd, current_command);
              fflush(stdout);
              pos = strlen(current_command);
              strcpy(input, current_command);
            }
          } else if (seq[1] == 'B') { // Seta para baixo
            if (history_index < history_count - 1) {
              history_index++;
              strcpy(current_command, history[history_index]);
              printf("\r\033[Kshell [%s]> %s", cwd, current_command);
              fflush(stdout);
              pos = strlen(current_command);
              strcpy(input, current_command);
            } else if (history_index == history_count - 1) {
              history_index++;
              printf("\r\033[Kshell [%s]> ", cwd);
              fflush(stdout);
              pos = 0;
              memset(input, 0, MAX_LINE);
            }
          }
        }
      } else { // Qualquer outro caractere
        if (pos < MAX_LINE - 1) {
          input[pos++] = c;
          write(STDOUT_FILENO, &c, 1);
        }
      }
    }

    // Adicionar ao histórico
    if (strlen(input) > 0) {
      add_to_history(input);
    }

    // Dividir o comando em tokens
    int i = 0;
    char *token = strtok(input, " ");
    while (token != NULL) {
      args[i++] = token;
      token = strtok(NULL, " ");
    }
    args[i] = NULL;

    // Verificar se o comando é "exit"
    if (args[0] != NULL && strcmp(args[0], "exit") == 0) {
      printf("Saindo do shell...\n");
      break; // Sai do loop e encerra o programa
    }

    // Verificar se o comando é "cd"
    if (args[0] != NULL && strcmp(args[0], "cd") == 0) {
      if (args[1] == NULL) {
        // Voltar para o diretório inicial
        if (chdir(initial_dir) != 0) {
          perror("Erro ao mudar para o diretório inicial");
        }
      } else {
        if (chdir(args[1]) != 0) {
          perror("Erro ao mudar de diretório");
        }
      }
      continue;
    }

    // Verificar se o comando é "history"
    if (args[0] != NULL && strcmp(args[0], "history") == 0) {
      show_history();
      continue;
    }

    // Executar comandos normais
    if (args[0] != NULL) {
      pid_t pid = fork();
      if (pid < 0) {
        perror("Erro ao criar processo");
      } else if (pid == 0) {
        // Processo filho
        if (execvp(args[0], args) == -1) {
          perror("Erro ao executar comando");
        }
        exit(0);
      } else {
        // Processo pai
        waitpid(pid, NULL, 0);
      }
    }
  }

  disable_raw_mode();
  return 0;
}
