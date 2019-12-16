#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>

#define SOCKADDR "localSocket"

void printStat(int *);

int main()
{
    //creazione socket per connessione a server
    int sock = socket(AF_LOCAL, SOCK_STREAM, 0);
    if(sock == -1) {
        perror("socket()");
        return 1;
    }

    // imposto l'indirizzo del socket
    struct sockaddr_un server = {
	    .sun_family = AF_LOCAL,
        .sun_path = SOCKADDR
    };

    //connessione a server in ascolto
    if(connect(sock, (struct sockaddr *)&server, sizeof(server)) == -1){
        perror("connect()");
        return 2;
    }

    int serverMessagelen = 0;
    while(recv(sock, &serverMessagelen,sizeof(int), 0) != -1){  //DOVREBBE uscire quando la comunicazione si interrompe
                                                                //Riceve la lunghezza del messaggio

        if(serverMessagelen > 2){  //Se ho ricevuto un messaggio (testo)

            char serverMessage[serverMessagelen];
            recv(sock, &serverMessage, serverMessagelen, 0);    //Ricezione messaggio 

            fprintf(stderr, "Server: %s\n", serverMessage);

            if(!strncmp(serverMessage, "Partita terminata", serverMessagelen)){
                return 0;
            }
            if(!strncmp(serverMessage, "Avversario disconnesso", serverMessagelen)){
                return 0;
            }
        }
        else{   //Altrimenti ho ricevuto lo stato della partita 


            int stateStack[2];
            int selectedStack, elementsToRemove;

            fprintf(stderr, "Ricezione stato partita\n");
            int r = recv(sock, &stateStack, sizeof(stateStack), 0);
            fprintf(stderr, "Dati ricevuti: %d\n", r);
            fprintf(stderr, "Visualizzazione stato partita\n");
            printStat(stateStack);

            fprintf(stderr, "Mossa richiesta\n");
            fprintf(stderr, "Da che pila vuoi togliere elementi? (1 o 2)\n");
            while(scanf("%d", &selectedStack) <= 0 || !(selectedStack==1 || selectedStack==2) || stateStack[selectedStack-1]==0){
                fprintf(stderr, "Inserisci un valore valido!\n");
                fprintf(stderr, "Da che pila vuoi togliere elementi? (1 o 2)\n");
            }

            fprintf(stderr,"Quanti elementi vuoi togliere?\n");
            while(scanf("%d", &elementsToRemove) <= 0 || elementsToRemove>stateStack[selectedStack-1] || elementsToRemove<=0){
                fprintf(stderr, "Inserisci un valore valido!\n");
                fprintf(stderr, "Quanti elementi vuoi togliere?\n");
            }

            stateStack[selectedStack-1] = stateStack[selectedStack-1] - elementsToRemove;

            send(sock, &stateStack, sizeof(stateStack), 0);
        }

    }

    close(sock);

    fprintf(stderr, "Partita terminata\n");

    return 0;
}






void printStat(int *stat){

    int stack1 = stat[0];
    int stack2 = stat[1];

    fprintf(stderr, "Situazione pile:\n");
    fprintf(stderr, "Pila 1 \t Pila 2\n");
    fprintf(stderr, "%d \t %d\n", stack1, stack2);

}