#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/un.h> // per struct sockaddr_un
#include <sys/socket.h>
#include <signal.h>
#include <sys/wait.h>
#include <pthread.h>

#define SOCKADDR "localSocket"

void playMatch(int *);
void stopMatch(int);
void handle_sigchld(int);

int main(){


    // imposto l'handler per SIGCHLD, in modo da non creare processi zombie
    //ogni volta che lo stato del processo figlio cambia (SIGCHLD) viene eseguita la funzione handle_sigchld
    signal(SIGCHLD, handle_sigchld);

    // apro il socket di ascolto
    int sock = socket(AF_LOCAL, SOCK_STREAM, 0);
    if(sock == -1) {
        perror("socket()");
        return 1;
    }

    // imposto l'indirizzo del socket
    struct sockaddr_un addr = {
        .sun_family = AF_LOCAL,
        .sun_path = SOCKADDR 
    };

    // mi preoccupo di rimuovere il file del socket in caso esista già.
    // se è impegnato da un altro server non si potrà rimuovere, ma bind()
    // successivamente mi darà errore
    unlink(SOCKADDR);
    // lego l'indirizzo al socket di ascolto
    if(bind(sock, (struct sockaddr *)&addr, sizeof addr) == -1) {
        perror("bind()");
        return 2;
    }

    // Abilito effettivamente l'ascolto, con un massimo di 5 client in attesa
    listen(sock, 5);

    fprintf(stderr, "In attesa di giocatori...\n");

    // continuo all'infinito ad aspettare client (players)
    while (1)
    {
        struct sockaddr_un player1_addr;
        socklen_t player1_len = sizeof(player1_addr);
        int fd_player1 = accept(sock, (struct sockaddr *)&player1_addr, &player1_len);
        if(fd_player1 == -1) {
            perror("accept()");
            return 3;
        }

        //Messaggio di attesa del secondo giocatore
        char message[] = "Benvenuto! Per favore attendi la connessione di un secondo giocatore...\n";
        int messagelen = strlen(message) + 1;
        send(fd_player1, &messagelen, sizeof(messagelen), 0); // Invio lunghezza del messaggio
        send(fd_player1, message, messagelen, 0); // Invio il messaggio

        struct sockaddr_un player2_addr;
        socklen_t player2_len = sizeof(player2_addr);
        int fd_player2 = accept(sock, (struct sockaddr *)&player2_addr, &player2_len);
        if(fd_player2 == -1) {
            perror("accept()");
            return 3;
        }

        /*
        * ogni volta che il server accetta due nuove connessioni,
        * queste viengono gestite da un nuovo processo figlio
        */
        pid_t pid = fork();
        if(pid == -1) {
            perror("fork()");
            return 4;
        }

        // il figlio gestisce la connessione, in un thread separato, il padre torna subito in ascolto
        if(!pid) {
            fprintf(stderr, "***PROCESSO FIGLIO (PID %d)***\n", getpid());
            pthread_t matchThread;
            int players[2] = {fd_player1, fd_player2};
            pthread_create(&matchThread, NULL, (void *)playMatch, players);

            int result;
            pthread_join(matchThread, (void *)&result);

            if(result != 0){
                perror("Thread");
                printf("Error: %d \n", result);
            }
            raise(SIGKILL);
        }//end if(pid!=0)

        if(pid==0) fprintf(stdout, "---ATTESA NUOVI GIOCATORI---");
    }//end while(1) (attesa connessioni server)
}//end main()




void playMatch(int *players){

    fprintf(stderr, "***THREAD***\n");

    int fd_player1 = players[0];
    int fd_player2 = players[1];

    fprintf(stderr, "Aperta connessione (PID %d).\n",getpid());

    char greet[] = "Giocatore 1, benvenuto nel server di NIM!\n";
    int greetlen = strlen(greet) + 1;
    send(fd_player1, &greetlen, sizeof(greetlen), 0); // Invio lunghezza del messaggio
    send(fd_player1, greet, greetlen, 0); // Invio il messaggio

    sprintf(greet,"Giocatore 2, benvenuto nel server di NIM!\n");
    send(fd_player2, &greetlen, sizeof(greetlen), 0); // Invio lunghezza del messaggio
    send(fd_player2, greet, greetlen, 0); // Invio il messaggio

    //Inizializzazione dati di gioco
    fprintf(stderr, "Inizializzaione partita...\n");
    srand(time(0));

    int stack1 = rand() % 100 + 1;   //elementi pila 1
    int stack2 = rand() % 100 + 1;   //elementi pila 2
    int elements = stack1 + stack2;

    int playerRound = 1;    //La prima mossa spetta al primo giocatore connesso
    int gameState[2] = { stack1 , stack2 };
    
    fprintf(stderr, "Partita inizializzata\n Elementi pila 1: %d\n Elementi pila 2: %d\n", stack1, stack2);

    //Gestione gioco
    fprintf(stderr, "Partita iniziata!\n");
    while(elements > 0){
        
        switch(playerRound){

            case 1:
            {   
                fprintf(stderr, "Turno giocatore 1\n");

                char player1Message[25] = "Giocatore 1 tocca a te!\n";
                int player1Messagelen = strlen(player1Message) + 1;
                send(fd_player1, &player1Messagelen, sizeof(player1Messagelen), 0); 
                send(fd_player1, player1Message, player1Messagelen, 0); 

                char player2Message[] = "Attendi la fine del turno di Giocatore 1...\n";
                int player2Messagelen = strlen(player2Message) + 1;
                send(fd_player2, &player2Messagelen, sizeof(player2Messagelen), 0); 
                send(fd_player2, player2Message, player2Messagelen, 0);

                fprintf(stderr, "\tInvio dati...\n");
                player1Messagelen = 2;
                send(fd_player1, &player1Messagelen, sizeof(player1Messagelen), 0);
                send(fd_player1, &gameState, sizeof(gameState), 0); //Invio situazione corrente
                fprintf(stderr, "\tAttesa mossa...\n");
                if(recv(fd_player1, &gameState, sizeof(gameState), 0) <=0){ //Ricezione situazione dopo mossa
                    stopMatch(fd_player2);
                    fprintf(stderr, "Partita terminata\n");
                }
                fprintf(stderr, "\tDati ricevuti...\n");

                elements = gameState[0] + gameState[1];
                playerRound = 2;
            
                break;
            }

            case 2:
            {
                fprintf(stderr, "Turno giocatore 2\n");

                char player1Message[] = "Attendi la fine del turno di Giocatore 2...\n";
                int player1Messagelen = strlen(player1Message) + 1;
                send(fd_player1, &player1Messagelen, sizeof(player1Messagelen), 0); 
                send(fd_player1, player1Message, player1Messagelen, 0); 

                char player2Message[] = "Giocatore 2 tocca a te!\n";
                int player2Messagelen = strlen(player2Message) + 1;
                send(fd_player2, &player2Messagelen, sizeof(player2Messagelen), 0); 
                send(fd_player2, player2Message, player2Messagelen, 0);

                fprintf(stderr, "\tInvio dati...\n");
                player2Messagelen = 2;
                send(fd_player2, &player2Messagelen, sizeof(player2Messagelen), 0);
                send(fd_player2, &gameState, sizeof(gameState), 0); //Invio situazione corrente
                fprintf(stderr, "\tAttesa mossa...\n");
                
                if(recv(fd_player2, &gameState, sizeof(gameState), 0) <=0){ //Ricezione situazione dopo mossa
                    stopMatch(fd_player1);
                    fprintf(stderr, "Partita terminata\n");
                }
                fprintf(stderr, "\tDati ricevuti...\n");

                elements = gameState[0] + gameState[1];
                playerRound = 1;

                break;
            }

            default:
                perror("Gestione turno");
        }//end switch-case
    }//end while(elements > 0)

    fprintf(stderr, "Fine della partita.\n Invio risultati...\n");

    if(playerRound == 1){   //Il vincitore è il giocatore 2
        char results[] = "Il vincitore è: giocatore 2!\n";
        int resultslen = strlen(results) + 1;
        send(fd_player1, &resultslen, sizeof(resultslen), 0); 
        send(fd_player1, results, resultslen, 0); 

        send(fd_player2, &resultslen, sizeof(resultslen), 0); 
        send(fd_player2, results, resultslen, 0); 
    }
    else{                   //Il vincitore è il giocatore 1
        char results[] = "Il vincitore è: giocatore 1!\n";
        int resultslen = strlen(results) + 1;
        send(fd_player1, &resultslen, sizeof(resultslen), 0); 
        send(fd_player1, results, resultslen, 0); 

        send(fd_player2, &resultslen, sizeof(resultslen), 0); 
        send(fd_player2, results, resultslen, 0); 
    }

    //Invio messaggio terminazione
    char end[] = "Partita terminata";
    int endlen = strlen(end) + 1;
    send(fd_player1, &endlen, sizeof(endlen), 0); 
    send(fd_player1, end, endlen, 0); 

    send(fd_player2, &endlen, sizeof(endlen), 0); 
    send(fd_player2, end, endlen, 0); 

    close(fd_player1); // Alla fine chiudiamo la connessione 1
    close(fd_player2); // Alla fine chiudiamo la connessione 2

}

void stopMatch(int fd_player){

    char errorMessage[] = "Avversario disconnesso";
    int errorMessagelen = strlen(errorMessage) + 1;

    send(fd_player, &errorMessagelen, sizeof(int), 0);
    send(fd_player, errorMessage, errorMessagelen, 0);
}

void handle_sigchld(int x) {
  int saved_errno = errno;
  while (waitpid(-1, 0, WNOHANG) > 0) {}    //attesa terminazione proc. figli e liberazione risorse
                                            //no processi zombie
  errno = saved_errno;
}