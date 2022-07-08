#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <netdb.h>
#include <sys/mman.h>

#define ECHOMAX 255
#define MAXPENDING 5

//initialize ID in server
int ownID;

//message structure for message to server
struct toServer{
    //indicated action to the server
    enum{login, logout, askWho, askAddress} messageType;

    //playerID to perform the action on
    unsigned int playerID;

    //port number to act on
    unsigned short int playerPort;
}
//initialize message sender
sendServ;

//initialize size of sent message
unsigned int sendServSize;

//message structure for message from the server
struct fromServer{
    //indicated action from server
    enum{ok, getAddress, getWho} messageType;

    //list of logged on playerIDs
    int loggedIn [10];

    //requested ID number
    unsigned int requestID;

    //requested player's receiving port
    unsigned short int requestPort;

    //requested player's address
    unsigned long int requestAddress;
}
//initialize message receiver
recvServ;

//initialize size of received message
unsigned int recvServSize;

//message structure for messages between players
struct betweenClient{
    //indicated action to player
    enum{play, confirm, decline, move} requestType;

    //own playerID sent to opponent
    unsigned int playerID;

    //column to take action in
    unsigned int move;
}
//initialize message receiver and sender
recvTCP, sendTCP;

//initialize size of TCP message
unsigned int tcpSize;

/**
 * Displays IDs of all logged in players
 */
void displayWho(){
    //initiate list display
    printf("Players currently online: \n");

    //initialize counter
    int i = 0;

    //as long as there is a valid ID number given
    while(recvServ.loggedIn[i]){
        //print user's ID
        printf("ID: %d", recvServ.loggedIn[i]);

        //indicate which ID is your own
        if(recvServ.loggedIn[i] == ownID)
            printf(" (You)");

        //print next line
        printf("\n");

        //increment counter
        ++i;
    }
}

//initialize TCP sending socket
int sendSock;

//initialize opponent address parameters
struct sockaddr_in oppAddr;
unsigned short * oppPort;
unsigned long int * oppIP;

/**
 * Send TCP Message to opponent
 * 
 * return:      Boolean for successful message sent (1 if failed)
 */
int sendTCPMessage(){
    //return boolean for successfull message sent
    int sendStatus = 0;

    //set opponent address parameters
    memset(&oppAddr, 0, sizeof(oppAddr));
    oppAddr.sin_family = AF_INET;
    oppAddr.sin_addr.s_addr = *oppIP;
    oppAddr.sin_port = htons(*oppPort);

    //create TCP output socket
    if((sendSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
        perror("Failed to establish TCP output socket");
        sendStatus = 1;
        goto exit;
    }

    //connect to TCP socket
    if(connect(sendSock, (struct sockaddr *) &oppAddr, sizeof(oppAddr)) < 0){
        perror("Failed to connect to opponent");
        sendStatus = 1;
        goto exit;
    }

    //send message over the socket
    if(send(sendSock, &sendTCP, tcpSize, 0) != tcpSize){
        perror("Failed to send message to player");
        sendStatus = 1;
        goto exit;
    }

    //label for failed process
    exit:

    //close socket and return to call location
    close(sendSock);
    return sendStatus;
}

//structure that holds the game board
struct gameBoard{
    //board played on during the game
    char gameArea[7][5];

    //piece that the player is using in the game
    char pieceName;
}
//initialize pointer to board
* activeBoard;

/**
 * Gets the next available row in the given column
 *
 * return: Next available row if one available
 */
int getNextSpot(int column){
    //start checking at the bottom of the column
    for(int i = 0; i < 5; ++i)
        //return next blank index
        if(activeBoard->gameArea[column][i] == ' ')
            return i;

    //indicate full row
    return -1;
}

/**
 * Check if the board has a victory in it
 *
 * return:  Boolean for if there is a victory or not
 */
int checkVictory(){
    //check each row and column
    for(int i = 0; i < 5; ++i){
        for(int j = 0; j < 7; ++j){
            //if the space has been taken
            if(activeBoard->gameArea[j][i] != ' '){
                //do a horizontal check left if space for four moves
                if(j < 4){
                    //stop check if chain is broken
                    for(int k = 1; k < 4; ++k){
                        if(activeBoard->gameArea[j + k][i] != activeBoard->gameArea[j][i])
                            goto endCheck1;
                    }

                    //victory if chain unbroken
                    return 1;
                }
                //label to exit horizontal check
                endCheck1:

                //check that any vertical victory possible in this position
                if(i < 2){
                    //check for victory directly above
                    for(int k = 1; k < 4; ++k){
                        //stop check if chain is broken
                        if(activeBoard->gameArea[j][i + k] != activeBoard->gameArea[j][i])
                            goto endCheck2;
                    }

                    //victory if chain unbroken
                    return 1;

                    //label to exit vertical check
                    endCheck2:

                    //check if space for right diagonal
                    if(j < 4){
                        //stop check if chain is broken
                        for(int k = 1; k < 4; ++k){
                            if(activeBoard->gameArea[j + k][i + k] != activeBoard->gameArea[j][i])
                                goto endCheck3;
                        }

                        //victory if chain unbroken
                        return 1;
                    }

                    //label to exit right diagonal check
                    endCheck3:

                    //check if space for left diagonal
                    if(j > 2){
                        //stop check if chain is broken
                        for(int k = 1; k < 4; ++k){
                            if(activeBoard->gameArea[j - k][i + k] != activeBoard->gameArea[j][i])
                                goto endCheck4;
                        }

                        //victory if chain unbroken
                        return 1;
                    }
                    //label and buffer to exit left diagonal check
                    endCheck4:
                    printf("");
                }
            }
        }
    }

    //no victory found in board
    return 0;
}

/**
 * Checks if the board is in a draw state
 *
 * return:  Boolean of draw state
 */
int checkDraw(){
    //check each board position
    for(int i = 0; i < 5; ++i)
        //return false if any blanks found
        for(int j = 0; j < 7; ++j)
            if(activeBoard->gameArea[j][i] == ' ')
                return 0;

    //return true if no blanks found
    return 1;
}

/**
 * Creates an image of the board's current state
 */
void drawBoard(){
    //start at top of board
    for(int i = 4; i >= 0; --i){
        //print initial divider
        printf("|");

        //for each piece in each column
        for(int j = 0; j < 7; ++j)
            printf(" %c |", activeBoard->gameArea[j][i]);

        //print next line
        printf("\n");
    }
}

/**
 * Plays a piece in the determined place
 *
 * param:   playHere    Column to play the piece in
 *          pieceType   What to fill the position with
 * return:              Boolean for piece successfully inserted
 */
int playPiece(int playHere, char pieceType){
    //initialize row to insert into
    int row;

    //check if column has space for a piece
    if((row = getNextSpot(playHere)) >= 0){
        //insert piece into board
        activeBoard->gameArea[playHere][row] = pieceType;

        //display board
        drawBoard();

        //return successful input
        return 0;
    }

    //display board to user
    drawBoard();

    //return failed input
    return 1;
}

/**
 * Creates the initial state of the game board
 *
 * param: declarePiece      Piece that this player will use in the game
 */
void initializeBoard(char declarePiece){
    //initialize each piece as a blank space
    for(int i = 0; i < 7; ++i)
        for(int j = 0; j < 5; ++j)
            activeBoard->gameArea[i][j] = ' ';

    //set the player's piece
    activeBoard->pieceName = declarePiece;

    //display empty board
    drawBoard();
}






int main(int argc, char * argv[]){
    //initialize socket identifiers
    int udpSock, servSock, cliSock;

    //initialize server address parameters
    struct sockaddr_in servAddr;
    char * servIP;
    unsigned short servPort;
    unsigned int servAddrLen;

    //initialize size of opponent's address
    unsigned int oppLen;

    //initialize own TCP port parameters
    struct sockaddr_in ownAddr;
    unsigned short tcpPort;

    //initialize UDP receiving address parameters
    struct sockaddr_in fromAddr;
    unsigned int fromSize;

    //initialize receiving address parameters
    unsigned int recvLen;

    //initialize user input string
    char instruction[ECHOMAX];

    //initialize own host parameters
    char host[256];
    char * IP;
    struct hostent * host_entry;
    int hostname;

    //initialize tcp received message length
    int recvOppSize;

    //initialize persistant booleans
    int * requestPending, * gameActive, * oppMove, * serverLogged;

    //initialize victory and draw state booleans
    int vicState, drawState;

    //check if appropriate number of arguments used
    if((argc < 3) || (argc > 4)){
        //notify user of action failure
        fprintf(stderr, "Usage: %s <Server IP> <TCP Port> [<Echo Port>]\n", argv[0]);
        exit(1);
    }

    //notify user of startup
    printf("Booting game program...\n");

    //set server's IP
    servIP = argv[1];

    //set up own TCP port
    tcpPort = atoi(argv[2]);

    //if server port given
    if(argc == 4)
        //set server's port number
        servPort = atoi(argv[3]);
    else
        //otherwise, default to port 7
        servPort = 7;

    //create UDP socket
    if((udpSock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
        perror("Failed to establish server socket");
        exit(1);
    }

    //create TCP input socket
    if((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
        perror("Failed to establish TCP input socket");
        exit(1);
    }

    //define output address as server's address
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(servIP);
    servAddr.sin_port = htons(servPort);

    //define own TCP input address
    memset(&ownAddr, 0, sizeof(ownAddr));
    ownAddr.sin_family = AF_INET;
    ownAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    ownAddr.sin_port = htons(tcpPort);

    //bind tcp socket
    if(bind(servSock, (struct sockaddr *) &ownAddr, sizeof(ownAddr)) < 0){
        perror("TCP bind failed");
        exit(1);
    }

    //listen for incoming connection
    if(listen(servSock, MAXPENDING) < 0){
        perror("Listen command failed");
        exit(1);
    }

    //set address size
    servAddrLen = sizeof(servAddr);

    //label for starting login
    loginInit:

    //set login message parameters
    sendServ.messageType = login;
    sendServ.playerID = 0;
    sendServ.playerPort = tcpPort;

    //set message size
    sendServSize = sizeof(sendServ);

    //determine own IP address
    hostname = gethostname(host, sizeof(host));
    host_entry = gethostbyname(host);
    IP = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0]));

    //notify user of login attempt
    printf("Logging in to server...\n");

    //send login request to server
    if(sendto(udpSock, &sendServ, sendServSize, 0, (struct sockaddr *) &servAddr, servAddrLen) != sendServSize){
        perror("Failed to send log in message");
        exit(1);
    }

    //set message sizes
    recvServSize = sizeof(recvServ);
    tcpSize = sizeof(struct betweenClient);
    fromSize = sizeof(fromAddr);

    //wait to receive message
    if((recvLen = recvfrom(udpSock, &recvServ, recvServSize, 0, (struct sockaddr *) &fromAddr, &fromSize)) < 0){
        perror("Log in failed: received bad acknowledgement");
        exit(1);
    }

    //check that message is received from the contacted server
    if(servAddr.sin_addr.s_addr != fromAddr.sin_addr.s_addr){
        perror("Log in failed: received a packet from unknown source!");
        exit(1);
    }

    //check that received IP and port are identical
    if(recvServ.requestAddress !=  inet_addr(IP) ||
        recvServ.requestPort != sendServ.playerPort){
            printf("Log in failed: returned items do not match!\n");
            exit(1);
        }

    //record own ID in server
    ownID = recvServ.requestID;

    //display all current users
    displayWho();

    //set variables consistent over fork
    requestPending = (int *) mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    gameActive = (int *) mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    oppMove = (int *) mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    
    //initialize logged in boolean
    serverLogged = (int *) malloc(sizeof(int));
    *serverLogged = 1;

    //set opponent address values consistent over fork
    oppIP = (unsigned long int *) mmap(NULL, sizeof(unsigned long int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    oppPort = (unsigned short *) mmap(NULL, sizeof(unsigned short), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);

    //set board item consistent over fork
    activeBoard = (struct gameBoard *) mmap(NULL, sizeof(struct gameBoard), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);

    //create fork for TCP receiver
    if(fork() == 0){
        //notify user of fork
        printf("Booting receiving port...\n");
        for(;;){
            //label for failed process in fork
            fTOP:

            //set size of opponent address item
            oppLen = sizeof(oppAddr);

            //clear tcp receiver
            memset(&recvTCP, 0, sizeof(recvTCP));

            //establish connection with opponent
            if((cliSock = accept(servSock, (struct sockaddr *) &oppAddr, &oppLen)) < 0){
                perror("Connection reception failed");
                goto fTOP;
            }

            //receive message from opponent
            if((recvOppSize = recv(cliSock, &recvTCP, tcpSize, 0)) < 0){
                perror("Failed to receive TCP message");
                goto fTOP;
            }

            //keep connection open for further message if needed
            while(recvOppSize > 0){
                if((recvOppSize = recv(cliSock, &recvTCP, tcpSize, 0)) < 0){
                    perror("Failed to continue reception");
                    goto fTOP;
                }
            }

            //if waiting on request reply
            if(* requestPending){
                //if opponent confirms request
                if(recvTCP.requestType == confirm){
                    //notify user of update
                    printf("Opponent agreed to play. Initiating game...\n");
                    
                    //set status as opponent's turn
                    *oppMove = 1;

                    //change request pending off
                    *requestPending = 0;

                    //turn on game active state
                    *gameActive = 1;

                    //create game board using 'X'
                    initializeBoard('X');
                }

                //
                else if(recvTCP.requestType = decline){
                    //notify user of update
                    printf("Opponent declined game. Closing connection...\n");
                    
                    //turn off requesting state
                    (* requestPending) = 0;
                }
            }

            //if currently engaged in a game
            else if(* gameActive){
                //if received opponent's move
                if(recvTCP.requestType == move){
                    //notify user of move made
                    printf("Player adds piece to %d\n", recvTCP.move);

                    //play their move in this version of board
                    if(activeBoard->pieceName == 'X')
                        playPiece(recvTCP.move - 1, 'O');
                    else
                        playPiece(recvTCP.move - 1, 'X');
                
                    //check if victory or draw reached
                    if((vicState = checkVictory()) || (drawState = checkDraw())){
                        //notify user of opponent victory
                        if(vicState)
                            printf("You lose...\n");

                        //notify user of draw reached
                        else if(drawState)
                            printf("Game ends in a tie.\n");

                        //turn off game state
                        *gameActive = 0;

                        //clear opponent address components
                        memset(oppIP, 0, sizeof(oppIP));
                        memset(oppPort, 0, sizeof(oppPort));
                    }

                    //switch off opponent's move state
                    *oppMove = 0;
                }
            }

            //if receiving a play request
            else if(recvTCP.requestType == play){
                //notify user of message received
                printf("Received play request message.\n");

                //clear server UDP message items
                memset(&sendServ, 0, sizeof(sendServ));
                memset(&recvServ, 0, sizeof(recvServ));

                //set parameters for askAddress message
                sendServ.messageType = askAddress;
                sendServ.playerID = recvTCP.playerID;

                //send request to server
                if(sendto(udpSock, &sendServ, sendServSize, 0, (struct sockaddr *) &servAddr, servAddrLen) != sendServSize){
                    perror("Failed to send port/address request");
                    goto fTOP;
                }

                //wait to receive message
                if((recvLen = recvfrom(udpSock, &recvServ, recvServSize, 0, (struct sockaddr *) &fromAddr, &fromSize)) < 0){
                    perror("Play command failed: received bad acknowledgement");
                    goto fTOP;
                }

                //check that message is received from the contacted server
                if(servAddr.sin_addr.s_addr != fromAddr.sin_addr.s_addr){
                    perror("Play command failed: received a packet from unknown source!");
                    goto fTOP;
                }

                //check that player can be connected with
                if(recvServ.requestPort == 0 || recvServ.requestAddress == 0){
                    printf("Player %d not found on server\n", recvTCP.playerID);
                    goto fTOP;
                }

                //prompt user for reply to message
                printf("Begin game with player %d?\n", recvTCP.playerID);

                //set opponent address parameters
                *oppIP = recvServ.requestAddress;
                *oppPort = recvServ.requestPort;

                //set requestPending state active
                (* requestPending) = 1;
            }
            
            //if received message to turn of receiver from self
            else if(recvTCP.requestType == move){
                //notify user of closing
                printf("Closing TCP receiver...\n");
                break;
            }

            //notify user of unknown message received
            else{
                printf("Received unknown message\n");
            }
        }
        
        //close fork
        exit(0);
    }

    //hold here for organization
    sleep(1);

    //notify user of successful login
    printf("Ready to play!\n");

    for(;;){
        //label to exit failed processes
        top:

        //if waiting on opponent's turn
        if(*oppMove){
            //notify user of state
            printf("Waiting for opponent's move...\n\n");

            //hold until change detected
            while(*oppMove);
        }

        //clear message items
        memset(&sendServ, 0, sizeof(sendServ));
        memset(&recvServ, 0, sizeof(recvServ));
        memset(&sendTCP, 0, sizeof(sendTCP));

        //prompt for user input
        printf("\nReady for instruction...\n");
        scanf("%s", instruction);

        //if currently logged out of server
        if(*serverLogged == 0){
            //user requests to log back in
            if(strcmp(instruction, "login") == 0){
                //return to section of code for login
                goto loginInit;
            }

            //user requests to quit program
            else if(strcmp(instruction, "exit") == 0)
                break;

            //invalid message received
            else
                printf("You need to either \"exit\" or \"login\" before giving any other command.");
        }

        //if opponent is waiting for a response
        else if(* requestPending){
            //user accepts request
            if(strcmp(instruction, "accept") == 0){
                //set message type
                sendTCP.requestType = confirm;

                //attempt to send message
                if(sendTCPMessage()){
                    goto top;
                }

                //remove request state
                (* requestPending) = 0;

                //initiate game state
                (* gameActive) = 1;

                //create board and piece used
                initializeBoard('O');
            }

            //user refuses request
            else if(strcmp(instruction, "decline") == 0){
                //set message type
                sendTCP.requestType = decline;

                //attempt to send message
                if(sendTCPMessage())
                    goto top;

                //remove request state
                (* requestPending) = 0;
            }

            //user makes some other message
            else{
                printf("You have a pending game request\n");
                printf("You must either \"accept\" or \"decline\" the request.\n");
            }
        }

        //if currently in game
        else if(* gameActive){
            //user wants to make a move
            if(strcmp(instruction, "move") == 0){
                //get desired move column
                int moveMade;
                scanf("%d", &moveMade);

                //check that valid column used
                if(moveMade > 7 || moveMade < 1){
                    printf("Move made must be a number between 1 and 7\n");
                    goto top;
                }

                //attempt to play piece in indicated column
                if(playPiece(moveMade - 1, activeBoard->pieceName)){
                    printf("Picked column is full! Choose another.\n");
                    goto top;
                }

                //set move message parameters
                sendTCP.requestType = move;
                sendTCP.move = moveMade;

                //attempt to send message to opponent
                if(sendTCPMessage())
                    goto top;

                //check if victory or draw met
                if((vicState = checkVictory()) || (drawState = checkDraw())){
                    //notify user of victory reached
                    if(vicState)
                        printf("You win!\n");

                    //notify user of draw reached
                    else if(drawState)
                        printf("Game ends in a tie.\n");

                    //deactivate game state
                    *gameActive = 0;

                    //clear opponent address parameters
                    memset(oppIP, 0, sizeof(oppIP));
                    memset(oppPort, 0, sizeof(oppPort));

                    //return to loop top
                    goto top;
                }

                //indicate it's opponent's next move
                *oppMove = 1;
            }

            //command not valid during game
            else{
                printf("Game is currently active.\n");
                printf("Type \"move\" and a number from 1-7 to play your piece.\n");
            }
        }

        //player is requesting a list of online users
        else if(strcmp(instruction, "who") == 0){
            //prepare query parameters
            sendServ.messageType = askWho;
            sendServ.playerID = ownID;

            //send message to server
            if(sendto(udpSock, &sendServ, sendServSize, 0, (struct sockaddr *) &servAddr, servAddrLen) != sendServSize){
                perror("Failed to send who message");
                goto top;
            }

            //wait to receive message
            if((recvLen = recvfrom(udpSock, &recvServ, recvServSize, 0, (struct sockaddr *) &fromAddr, &fromSize)) < 0){
                perror("Who command failed: received bad acknowledgement");
                goto top;
            }

            //check that message is received from the contacted server
            if(servAddr.sin_addr.s_addr != fromAddr.sin_addr.s_addr){
                perror("Who command failed: received a packet from unknown source!");
                goto top;
            }

            //display list of logged in users
            displayWho();       
        }

        //player wants to initiate a game
        else if(strcmp(instruction, "play") == 0){
            //get player's target opponent
            int getIDNum;
            scanf("%d", &getIDNum);

            //check that number is valid
            if(getIDNum >= 11){
                printf("Number given must be 10 or less.\n");
                goto top;
            }

            //check that request isn't self
            if(getIDNum == ownID){
                printf("Cannot connect game to self!\n");
                goto top;
            }

            //get IP info from server
            sendServ.messageType = askAddress;
            sendServ.playerID = getIDNum;

            //send message to server
            if(sendto(udpSock, &sendServ, sendServSize, 0, (struct sockaddr *) &servAddr, servAddrLen) != sendServSize){
                perror("Failed to send port/address request message");
                goto top;
            }            

            //wait to receive message
            if((recvLen = recvfrom(udpSock, &recvServ, recvServSize, 0, (struct sockaddr *) &fromAddr, &fromSize)) < 0){
                perror("Play command failed: received bad acknowledgement");
                goto top;
            }

            //check that message is received from the contacted server
            if(servAddr.sin_addr.s_addr != fromAddr.sin_addr.s_addr){
                perror("Play command failed: received a packet from unknown source!");
                goto top;
            }

            //check that player can be connected with
            if(recvServ.requestPort == 0 || recvServ.requestAddress == 0){
                printf("Player %d not found on server\n", getIDNum);
                goto top;
            }

            //get received address parameters
            (* oppIP) = recvServ.requestAddress;
            (* oppPort) = recvServ.requestPort;

            //prepare game request message
            sendTCP.requestType = play;
            sendTCP.playerID = ownID;

            //attempt to send message to opponent
            if(sendTCPMessage()){
                goto top;
            }

            //initialize request pending state
            (* requestPending) = 1;

            //wait until opponent responds
            printf("Waiting on player responses...\n");
            while(* requestPending);
        }

        //player wants to go offline
        else if(strcmp(instruction, "logout") == 0){
            //prepare logout message
            sendServ.messageType = logout;
            sendServ.playerID = ownID;

            //send message to server
            if(sendto(udpSock, &sendServ, sendServSize, 0, (struct sockaddr *) &servAddr, servAddrLen) != sendServSize){
                perror("Failed to send log out message");
                goto top;
            }

            //wait to receive message
            if((recvLen = recvfrom(udpSock, &recvServ, recvServSize, 0, (struct sockaddr *) &fromAddr, &fromSize)) < 0){
                perror("Logout command failed: received bad acknowledgement");
                goto top;
            }

            //check that message is received from the contacted server
            if(servAddr.sin_addr.s_addr != fromAddr.sin_addr.s_addr){
                perror("Logout command failed: received a packet from unknown source!");
                goto top;
            }

            //prepare message to self
            sendTCP.requestType = move;

            //establish output socket
            if((sendSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
                perror("Failed to establish TCP output socket");
                goto top;
            }

            //connect to self
            if(connect(sendSock, (struct sockaddr *) &ownAddr, sizeof(ownAddr)) < 0){
                perror("Failed to connect to self");
                goto top;
            }

            //send message to close TCP receiver
            if(send(sendSock, &sendTCP, tcpSize, 0) != tcpSize){
                perror("Failed to send message to self");
                goto top;
            }

            //close sending socket
            close(sendSock);

            //give own receiver time to close
            sleep(1);

            //set logged in state off
            *serverLogged = 0;
        }

        //player gives an invalid command
        else
            printf("\nNo command for %s\n", instruction);
    }
    //close program
    exit(0);
}